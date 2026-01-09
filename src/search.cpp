#include "search.h"
#include "movegen.h"
#include "evaluate.h"
#include "tt.h"
#include "tune.h"
#include "misc.h"
#include "opt/mthread.h"
#include "syzygy/tbprobe.h"
#include <algorithm>
#include <iostream>
#include <chrono>
#include <cmath>
#include <vector>
#include <atomic>
#include <cstring>

namespace Search {

// --- Globals & Constants ---

std::atomic<bool> stop_search{false};
std::atomic<long long> total_nodes{0};
std::chrono::time_point<std::chrono::steady_clock> start_time;
long long time_limit = 0;

const int MAX_PLY_SEARCH = 128;
const int INFINITE_SCORE = 32000;
const int MATE_SCORE = 31000;
const int MATE_BOUND = 30000;

int Reductions[64][64];
int FutilityMargins[32];

Opt::ThreadPool thread_pool;

// --- Initialization ---

void init_search() {
    for (int d = 0; d < 64; ++d) {
        for (int m = 0; m < 64; ++m) {
            double r = std::log(d + 1) * std::log(m + 1) / 1.95 + 0.25;
            Reductions[d][m] = static_cast<int>(r);
            if (Reductions[d][m] < 0) Reductions[d][m] = 0;
        }
    }
    for (int d = 0; d < 32; ++d) {
        FutilityMargins[d] = 100 * d;
    }
}

// --- Thread Data ---

struct ThreadData {
    int thread_id;
    Move killers[MAX_PLY_SEARCH][2];
    int history[COLOR_NB][SQ_NB][SQ_NB];
    Move pv_table[MAX_PLY_SEARCH][MAX_PLY_SEARCH];
    int pv_length[MAX_PLY_SEARCH];
    long long nodes;
    
    ThreadData(int id) : thread_id(id), nodes(0) {
        clear();
    }
    
    void clear() {
        for (int i = 0; i < MAX_PLY_SEARCH; ++i) {
            killers[i][0] = killers[i][1] = Move::none();
            pv_length[i] = 0;
            for (int j = 0; j < MAX_PLY_SEARCH; ++j) pv_table[i][j] = Move::none();
        }
        for (int c = 0; c < COLOR_NB; ++c)
            for (int f = 0; f < SQ_NB; ++f)
                for (int t = 0; t < SQ_NB; ++t)
                    history[c][f][t] = 0;
    }
};

void update_history(ThreadData& td, Move m, Color c, int depth) {
    int bonus = depth * depth;
    if (bonus > 400) bonus = 400;
    int& entry = td.history[c][m.from()][m.to()];
    entry += bonus - entry * abs(bonus) / 512; 
}

// --- Move Ordering ---

struct MovePicker {
    const Position& pos;
    ThreadData& td;
    Move hash_move;
    Move killer1, killer2;
    int depth;
    int ply;
    int phase;
    MoveGen::MoveList moves;
    int scores[256];
    int index;
    
    MovePicker(const Position& p, ThreadData& t, Move hm, int d, int pl) 
        : pos(p), td(t), hash_move(hm), depth(d), ply(pl), phase(0), index(0) {
        killer1 = td.killers[ply][0];
        killer2 = td.killers[ply][1];
    }

    int score_capture(Move m) {
        Piece victim_p = pos.piece_on(m.to());
        int victim = (victim_p == NO_PIECE) ? 0 : type_of(victim_p);
        int attacker = type_of(pos.piece_on(m.from()));
        return (victim * 10) - attacker + 100000;
    }

    int score_quiet(Move m) {
        if (m == killer1) return 90000;
        if (m == killer2) return 80000;
        return td.history[pos.side_to_move()][m.from()][m.to()];
    }

    bool next(Move& m) {
        // Phase 0: Hash Move
        if (phase == 0) {
            phase = 1;
            if (hash_move != Move::none() && pos.is_pseudo_legal(hash_move)) {
                m = hash_move;
                return true;
            }
        }
        
        // Phase 1: Captures
        if (phase == 1) {
            if (index == 0) {
                moves.count = 0;
                MoveGen::generate<MoveGen::CAPTURES>(pos, moves);
                for (int i = 0; i < moves.count; ++i) {
                     if (moves[i] == hash_move) {
                         moves[i] = moves[--moves.count];
                         i--;
                         continue;
                     }
                     scores[i] = score_capture(moves[i]);
                }
            }
            if (index < moves.count) {
                int best = index;
                for(int i=index+1; i<moves.count; ++i) if(scores[i] > scores[best]) best = i;
                std::swap(moves[index], moves[best]);
                std::swap(scores[index], scores[best]);
                m = moves[index++];
                return true;
            }
            phase = 2; index = 0;
        }

        // Phase 2: Killers
        if (phase == 2) {
             phase = 3;
             if (killer1 != Move::none() && killer1 != hash_move && pos.is_pseudo_legal(killer1) && pos.piece_on(killer1.to()) == NO_PIECE) {
                 m = killer1;
                 return true;
             }
        }
        if (phase == 3) {
             phase = 4;
             if (killer2 != Move::none() && killer2 != hash_move && killer2 != killer1 && pos.is_pseudo_legal(killer2) && pos.piece_on(killer2.to()) == NO_PIECE) {
                 m = killer2;
                 return true;
             }
        }

        // Phase 4: Quiets
        if (phase == 4) {
             if (index == 0) {
                 moves.count = 0;
                 MoveGen::generate<MoveGen::QUIETS>(pos, moves);
                 for (int i=0; i<moves.count; ++i) {
                     if (moves[i] == hash_move || moves[i] == killer1 || moves[i] == killer2) {
                         moves[i] = moves[--moves.count];
                         i--;
                         continue;
                     }
                     scores[i] = score_quiet(moves[i]);
                 }
             }
             if (index < moves.count) {
                 int best = index;
                 for(int i=index+1; i<moves.count; ++i) if(scores[i] > scores[best]) best = i;
                 std::swap(moves[index], moves[best]);
                 std::swap(scores[index], scores[best]);
                 m = moves[index++];
                 return true;
             }
             phase = 5;
        }
        return false;
    }
};

void check_time() {
    if (time_limit == 0) return;
    auto now = std::chrono::steady_clock::now();
    long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
    if (elapsed >= time_limit) stop_search = true;
}

// --- Quiescence Search ---

int qsearch(Position& pos, int alpha, int beta, int ply, ThreadData& td) {
    if ((td.nodes & 2047) == 0) {
        if (td.thread_id == 0) check_time();
        if (stop_search) return 0;
    }
    
    td.nodes++;
    if (ply >= MAX_PLY_SEARCH) return Eval::evaluate(pos);

    int static_eval = Eval::evaluate(pos);
    
    // Standing Pat
    if (static_eval >= beta) return beta;
    if (alpha < static_eval) alpha = static_eval;

    MoveGen::MoveList moves;
    MoveGen::generate<MoveGen::CAPTURES>(pos, moves);
    
    int scores[256];
    for(int i=0; i<moves.count; ++i) {
        Piece victim = pos.piece_on(moves[i].to());
        int val = (victim == NO_PIECE) ? 0 : type_of(victim);
        scores[i] = val * 10 - type_of(pos.piece_on(moves[i].from()));
        if (moves[i].type() == PROMOTION) scores[i] += 1000;
    }

    for(int i=0; i<moves.count; ++i) {
        int best = i;
        for(int j=i+1; j<moves.count; ++j) if(scores[j] > scores[best]) best = j;
        std::swap(moves[i], moves[best]);
        std::swap(scores[i], scores[best]);

        Move m = moves[i];
        
        // Delta Pruning
        if (static_eval + 200 + (type_of(pos.piece_on(m.to())) * 100) < alpha && m.type() != PROMOTION) continue;

        StateInfo st;
        pos.make_move(m, st);
        
        if (Bitboards::lsb(pos.pieces(static_cast<Color>(pos.side_to_move() ^ 1), KING)) == SQ_NONE || 
            pos.is_attacked(Bitboards::lsb(pos.pieces(static_cast<Color>(pos.side_to_move() ^ 1), KING)), pos.side_to_move())) {
            pos.unmake_move(m);
            continue;
        }

        int score = -qsearch(pos, -beta, -alpha, ply+1, td);
        pos.unmake_move(m);

        if (stop_search) return 0;
        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }
    return alpha;
}

// --- Main Search (Alpha-Beta) ---

int alpha_beta(Position& pos, int alpha, int beta, int depth, int ply, ThreadData& td, bool do_null = true) {
    if (stop_search) return 0;
    if (ply >= MAX_PLY_SEARCH) return Eval::evaluate(pos);
    
    if (depth <= 0) return qsearch(pos, alpha, beta, ply, td);
    
    if ((td.nodes & 2047) == 0) {
        if (td.thread_id == 0) check_time();
        if (stop_search) return 0;
    }

    if (pos.is_draw()) return 0;

    int mate_val = MATE_BOUND - ply;
    if (alpha < -mate_val) alpha = -mate_val;
    if (beta > mate_val - 1) beta = mate_val - 1;
    if (alpha >= beta) return alpha;

    td.pv_length[ply] = ply;

    Square us_king = Bitboards::lsb(pos.pieces(pos.side_to_move(), KING));
    bool in_check = (us_king != SQ_NONE && pos.is_attacked(us_king, static_cast<Color>(pos.side_to_move() ^ 1)));

    // Check Extension
    if (in_check) depth++;

    // TT Probe
    TTEntry tte;
    Move hash_move = Move::none();
    int tt_score = -INFINITE_SCORE;
    bool tt_hit = TT.probe(pos.hash(), tte);
    
    if (tt_hit) {
        hash_move = tte.move;
        tt_score = tte.score;
        if (tt_score > MATE_BOUND) tt_score -= ply;
        else if (tt_score < -MATE_BOUND) tt_score += ply;

        if (tte.depth >= depth && !in_check) { // Don't return from TT in check if not exact? (Safe to return if bounds valid)
            if (tte.flag == EXACT) return tt_score;
            if (tte.flag == ALPHA && tt_score <= alpha) return alpha;
            if (tte.flag == BETA && tt_score >= beta) return beta;
        }
    }

    // Syzygy Probe (Roots handled in iterate, this is for tree)
    // if (depth < 5) ... probe_wdl ... (Skipped for now)

    int static_eval = Eval::evaluate(pos);
    if (in_check) static_eval = -INFINITE_SCORE;

    // Static Null Move Pruning (Reverse Futility)
    if (!in_check && depth < 5 && abs(beta) < MATE_BOUND) {
        int margin = FutilityMargins[depth];
        if (static_eval - margin >= beta) return beta;
    }

    // Null Move Pruning
    if (do_null && !in_check && depth >= 3 && static_eval >= beta && abs(beta) < MATE_BOUND) {
        StateInfo st;
        pos.make_null_move(st);
        int R = 3 + depth / 4;
        int score = -alpha_beta(pos, -beta, -beta+1, depth - 1 - R, ply + 1, td, false);
        pos.unmake_null_move();
        if (stop_search) return 0;
        if (score >= beta) return beta;
    }

    // Internal Iterative Deepening
    if (depth >= 6 && hash_move == Move::none() && !in_check) {
        int r = depth - 2;
        alpha_beta(pos, alpha, beta, r, ply, td, do_null);
        if (TT.probe(pos.hash(), tte)) hash_move = tte.move;
    }

    MovePicker picker(pos, td, hash_move, depth, ply);
    Move m;
    int moves_count = 0;
    int best_score = -INFINITE_SCORE;
    Move best_move = Move::none();
    TTFlag flag = ALPHA;

    while(picker.next(m)) {
        bool is_quiet = (pos.piece_on(m.to()) == NO_PIECE);
        
        StateInfo st;
        pos.make_move(m, st);
        
        Square k = Bitboards::lsb(pos.pieces(static_cast<Color>(pos.side_to_move() ^ 1), KING));
        if (k == SQ_NONE || pos.is_attacked(k, pos.side_to_move())) {
            pos.unmake_move(m);
            continue;
        }

        moves_count++;
        int score;

        // PVS & LMR
        if (moves_count == 1) {
             score = -alpha_beta(pos, -beta, -alpha, depth-1, ply+1, td, true);
        } else {
             int R = 0;
             if (depth >= 3 && is_quiet && !in_check && moves_count > 1) {
                 int r_idx = (moves_count > 63) ? 63 : moves_count;
                 int d_idx = (depth > 63) ? 63 : depth;
                 R = Reductions[d_idx][r_idx];
                 
                 // Reduce less if PV node (alpha > -inf)? No, standard LMR is fine.
                 // Reduce less for killers?
                 if (m == td.killers[ply][0] || m == td.killers[ply][1]) R--;
             }
             
             // Search with reduced depth (Null Window)
             score = -alpha_beta(pos, -alpha-1, -alpha, depth-1-R, ply+1, td, true);
             
             // Re-search if failed high
             if (score > alpha && R > 0) {
                 score = -alpha_beta(pos, -alpha-1, -alpha, depth-1, ply+1, td, true);
             }
             
             // Re-search with full window if PVS failed
             if (score > alpha && score < beta) {
                 score = -alpha_beta(pos, -beta, -alpha, depth-1, ply+1, td, true);
             }
        }

        pos.unmake_move(m);
        if (stop_search) return 0;

        if (score > best_score) {
            best_score = score;
            best_move = m;
            if (score > alpha) {
                alpha = score;
                flag = EXACT;
                
                // Update PV
                td.pv_table[ply][ply] = m;
                for(int j=ply+1; j<td.pv_length[ply+1]; ++j) 
                    td.pv_table[ply][j] = td.pv_table[ply+1][j];
                td.pv_length[ply] = td.pv_length[ply+1];
                
                if (alpha >= beta) {
                    if (is_quiet) {
                        td.killers[ply][1] = td.killers[ply][0];
                        td.killers[ply][0] = m;
                        update_history(td, m, pos.side_to_move(), depth);
                    }
                    TT.store(pos.hash(), m, beta, depth, BETA, ply);
                    return beta;
                }
            }
        }
    }

    if (moves_count == 0) {
        if (in_check) return -MATE_BOUND + ply;
        return 0; // Stalemate
    }

    TT.store(pos.hash(), best_move, best_score, depth, flag, ply);
    return best_score;
}

// --- Thread Entry Point ---

void thread_search(Position pos, Limits limits, ThreadData* td) {
    // Helper threads strictly do Lazy SMP
    int alpha = -INFINITE_SCORE, beta = INFINITE_SCORE;
    for (int d = 1; d <= limits.depth; ++d) {
        if (stop_search) break;
        // Helpers usually just search with open window or copy main thread's root logic.
        // For simplicity:
        alpha_beta(pos, alpha, beta, d, 0, *td);
    }
}

// --- Iterative Deepening ---

void iterate(Position& pos, Limits limits) {
    if (Reductions[0][0] == 0) init_search(); // Ensure initialized

    total_nodes = 0;
    stop_search = false;
    start_time = std::chrono::steady_clock::now();
    
    // Time Management
    time_limit = 0;
    if (limits.use_time) {
        if (limits.is_movetime) {
            time_limit = limits.time - 50;
        } else {
            long long t = limits.time;
            int mvt = limits.movestogo > 0 ? limits.movestogo : 25;
            time_limit = t / mvt + limits.inc - 50;
        }
        if (time_limit < 50) time_limit = 50;
    }

    TT.new_search();
    
    int num_threads = Tune::get("Threads");
    if (num_threads < 1) num_threads = 1;
    
    // Setup threads
    thread_pool.init(num_threads);
    std::vector<std::unique_ptr<ThreadData>> thread_datas;
    for(int i=0; i<num_threads; ++i) thread_datas.push_back(std::make_unique<ThreadData>(i));

    // Launch helpers
    if (num_threads > 1) {
         // Using a simple lambda to bridge between ThreadPool and thread_search
         thread_pool.start_search([&](int id) {
             if (id == 0) return; // Main thread handled separately
             thread_search(pos, limits, thread_datas[id].get());
         });
    }

    ThreadData& main_td = *thread_datas[0];
    Move best_move = Move::none();
    int alpha = -INFINITE_SCORE, beta = INFINITE_SCORE;
    int score = 0;

    for (int d = 1; d <= limits.depth; ++d) {
        // Aspiration Windows
        if (d >= 5) {
            int delta = 25;
            alpha = std::max(-INFINITE_SCORE, score - delta);
            beta = std::min(INFINITE_SCORE, score + delta);
            
            while (true) {
                score = alpha_beta(pos, alpha, beta, d, 0, main_td);
                if (stop_search) break;
                
                if (score <= alpha) {
                    beta = (alpha + beta) / 2;
                    alpha = std::max(-INFINITE_SCORE, alpha - delta * 2);
                    delta *= 2;
                } else if (score >= beta) {
                    beta = std::min(INFINITE_SCORE, beta + delta * 2);
                    delta *= 2; // alpha stays same to save work? No, usually widen both or just beta.
                } else {
                    break;
                }
            }
        } else {
            alpha = -INFINITE_SCORE;
            beta = INFINITE_SCORE;
            score = alpha_beta(pos, alpha, beta, d, 0, main_td);
        }
        
        if (stop_search) break;

        if (main_td.pv_table[0][0] != Move::none()) best_move = main_td.pv_table[0][0];

        // Stats
        long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count();
        if (ms == 0) ms = 1;
        long long nodes = main_td.nodes;
        for(size_t i=1; i<thread_datas.size(); ++i) nodes += thread_datas[i]->nodes;
        
        std::cout << "info depth " << d << " score cp " << score << " nodes " << nodes << " nps " << (nodes * 1000 / ms) << " time " << ms << " pv";
        for(int i=0; i<main_td.pv_length[0]; ++i) {
            Move m = main_td.pv_table[0][i];
            std::cout << " " << static_cast<char>('a' + (m.from()%8)) << (m.from()/8+1) 
                      << static_cast<char>('a' + (m.to()%8)) << (m.to()/8+1);
            if (m.type() == PROMOTION) {
                char pchar = ' ';
                switch(m.promotion_piece()) {
                    case QUEEN: pchar = 'q'; break;
                    case ROOK: pchar = 'r'; break;
                    case BISHOP: pchar = 'b'; break;
                    case KNIGHT: pchar = 'n'; break;
                    default: break;
                }
                std::cout << pchar;
            }
        }
        std::cout << std::endl;

        if (limits.use_time && ms > time_limit / 2) break;
    }
    
    stop_search = true;
    thread_pool.wait_for_completion(); // Or stop()
    
    if (best_move == Move::none()) {
        MoveGen::MoveList moves;
        MoveGen::generate<MoveGen::ALL>(pos, moves);
        if (moves.count > 0) best_move = moves[0];
    }
    
    std::cout << "bestmove " << static_cast<char>('a' + (best_move.from()%8)) << (best_move.from()/8+1) 
              << static_cast<char>('a' + (best_move.to()%8)) << (best_move.to()/8+1);
    if (best_move.type() == PROMOTION) {
         switch(best_move.promotion_piece()) {
             case QUEEN: std::cout << 'q'; break;
             case ROOK: std::cout << 'r'; break;
             case BISHOP: std::cout << 'b'; break;
             case KNIGHT: std::cout << 'n'; break;
             default: break;
         }
    }
    std::cout << std::endl;
}

} // namespace Search