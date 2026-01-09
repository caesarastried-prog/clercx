#include "search.h"
#include "movegen.h"
#include "evaluate.h"
#include "tt.h"
#include "tune.h"
#include "misc.h"
#include "opt/mthread.h"
#include "syzygy/tbprobe.h"
#include "mcache.h"
#include <algorithm>
#include <iostream>
#include <chrono>
#include <cmath>
#include <vector>
#include <atomic>
#include <cstring>
#include <array>
#include <iomanip>

namespace Search {

// --- Globals & Constants ---

constexpr int MAX_PLY = 128;
constexpr int INFINITE_SCORE = 32000;
constexpr int MATE_SCORE = 31000;
constexpr int MATE_BOUND = 30000;

std::atomic<bool> stop_search{false};
std::atomic<long long> nodes_searched{0};
std::chrono::time_point<std::chrono::steady_clock> start_time;
long long time_hard_limit = 0;
long long time_soft_limit = 0;
bool run_infinite = false;

// --- Tuning Cache ---
struct SearchParams {
    int LMR_Base;
    int LMR_Factor;
    int RFP_Margin;
    int NMP_Depth;
} SP;

void refresh_params() {
    SP.LMR_Base = Tune::get("LMR_Base");
    SP.LMR_Factor = Tune::get("LMR_Factor");
    SP.RFP_Margin = Tune::get("RFP_Margin");
    // SP.NMP_Depth = Tune::get("NMP_Depth"); // Example
}

// --- Heuristics ---
// Shared History (Lazy SMP)
std::atomic<int> History[COLOR_NB][SQ_NB][SQ_NB]; 
Move Killers[MAX_PLY][2];

// --- Static Exchange Evaluation (SEE) ---

const int PieceValue[PIECE_TYPE_NB] = { 0, 100, 325, 325, 500, 975, 0 };

int see(const Position& pos, Move m) {
    if (m.type() == CASTLING || m.type() == EN_PASSANT) return 0; // Simplified
    
    Square to = m.to();
    Square from = m.from();
    
    int gain[32];
    int d = 0;
    
    Piece captured = pos.piece_on(to);
    gain[d] = (captured == NO_PIECE ? 0 : PieceValue[type_of(captured)]);
    
    // Initial piece
    PieceType attacker_pt = type_of(pos.piece_on(from));
    
    // If promotion
    if (m.type() == PROMOTION) {
        gain[d] += PieceValue[m.promotion_piece()] - PieceValue[PAWN];
        attacker_pt = m.promotion_piece();
    }
    
    // Hidden variable: occupancy
    // Ideally requires bitboard manipulation: xray attacks.
    // For this level, we can use a "swap list" approximation or full SEE.
    // Implementing full SEE requires "least valuable attacker" extraction.
    
    // Simplified SEE:
    return gain[0]; // Placeholder for safety if we don't have full `attackers_to`
}

// --- Thread Data ---

struct ThreadData {
    int id;
    long long nodes = 0;
    Move killers[MAX_PLY][2];
    int history[COLOR_NB][SQ_NB][SQ_NB]; // Local copy or pointer? using shared for now.
    
    ThreadData(int i) : id(i) {
        std::memset(killers, 0, sizeof(killers));
    }
};

Opt::ThreadPool thread_pool;

// --- Move Picker ---

struct MovePicker {
    const Position& pos;
    Move hash_move;
    int depth;
    MoveGen::MoveList moves;
    int scores[256];
    int cur = 0;
    int phase = 0;
    
    MovePicker(const Position& p, Move hm, int d) : pos(p), hash_move(hm), depth(d) {}
    
    int score(Move m) {
        if (m == hash_move) return 2000000;
        if (m.type() == PROMOTION && m.promotion_piece() == QUEEN) return 1000000;
        
        if (pos.is_capture(m)) {
            // MVV/LVA
            Piece attacker = pos.piece_on(m.from());
            Piece victim = pos.piece_on(m.to());
            return 100000 + PieceValue[type_of(victim)] * 10 - PieceValue[type_of(attacker)];
        }
        
        // Killers
        if (m == Killers[depth][0]) return 90000;
        if (m == Killers[depth][1]) return 80000;
        
        // History
        return History[pos.side_to_move()][m.from()][m.to()].load(std::memory_order_relaxed);
    }
    
    bool next(Move& m) {
        // 1. Hash
        if (phase == 0) {
            phase = 1;
            if (hash_move != Move::none() && pos.is_pseudo_legal(hash_move)) {
                m = hash_move;
                return true;
            }
        }
        
        // 2. Generate All (Simplification for readability/compactness)
        if (phase == 1) {
            phase = 2;
            MoveGen::generate<MoveGen::ALL>(pos, moves);
            for(int i=0; i<moves.count; ++i) {
                if(moves[i] == hash_move) { scores[i] = -1; continue; }
                scores[i] = score(moves[i]);
            }
        }
        
        // 3. Selection
        if (phase == 2) {
            int best = -1;
            int max_val = -10000000;
            for(int i=0; i<moves.count; ++i) {
                if (scores[i] > max_val) {
                    max_val = scores[i];
                    best = i;
                }
            }
            
            if (best != -1) {
                m = moves[best];
                scores[best] = -10000000; // consumed
                return true;
            }
        }
        
        return false;
    }
};

// --- Time Check ---

void check_time() {
    if (run_infinite || stop_search) return;
    auto now = std::chrono::steady_clock::now();
    long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
    
    if (elapsed > time_hard_limit) stop_search = true;
}

// --- QSearch ---

int qsearch(Position& pos, int alpha, int beta, int ply, ThreadData& td) {
    if ((td.nodes & 2047) == 0 && td.id == 0) check_time();
    if (stop_search) return 0;
    
    td.nodes++;
    
    int stand_pat = Eval::evaluate(pos);
    if (ply >= MAX_PLY) return stand_pat;
    
    if (stand_pat >= beta) return beta;
    if (alpha < stand_pat) alpha = stand_pat;
    
    MoveGen::MoveList moves;
    MoveGen::generate<MoveGen::CAPTURES>(pos, moves);
    
    // Sort
    int scores[256];
    for(int i=0; i<moves.count; ++i) {
        // MVV/LVA
        Piece v = pos.piece_on(moves[i].to());
        Piece a = pos.piece_on(moves[i].from());
        scores[i] = (v == NO_PIECE ? 0 : PieceValue[type_of(v)]) * 10 - PieceValue[type_of(a)];
    }
    
    for(int i=0; i<moves.count; ++i) {
        int best = i;
        for(int j=i+1; j<moves.count; ++j) if(scores[j] > scores[best]) best = j;
        std::swap(moves[i], moves[best]); std::swap(scores[i], scores[best]);
        
        Move m = moves[i];
        if (!pos.is_legal(m)) continue; // Expensive?
        
        // Delta Pruning
        // if (stand_pat + PieceValue[type_of(pos.piece_on(m.to()))] + 200 < alpha) continue;

        StateInfo st;
        pos.make_move(m, st);
        int score = -qsearch(pos, -beta, -alpha, ply+1, td);
        pos.unmake_move(m);
        
        if (stop_search) return 0;
        
        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }
    return alpha;
}

// --- Search ---

int search(Position& pos, int alpha, int beta, int depth, int ply, ThreadData& td, bool do_null) {
    if (stop_search) return 0;
    
    bool root = (ply == 0);
    
    if (!root) {
        if (pos.is_draw()) return 0;
        if (ply >= MAX_PLY) return Eval::evaluate(pos);
        
        // Mate Distance
        int mate = MATE_BOUND - ply;
        if (alpha < -mate) alpha = -mate;
        if (beta > mate-1) beta = mate-1;
        if (alpha >= beta) return alpha;
    }
    
    if (td.id == 0 && (td.nodes & 2047) == 0) check_time();
    
    td.nodes++;
    
    // QSearch at horizon
    if (depth <= 0) return qsearch(pos, alpha, beta, ply, td);
    
    bool in_check = pos.checkers();
    if (in_check) depth++; // Check Extension

    // TT
    TTEntry tte;
    Move hash_move = Move::none();
    if (TT.probe(pos.hash(), tte)) {
        hash_move = tte.move;
        if (!root && tte.depth >= depth) {
             int s = tte.score;
             if (s > MATE_BOUND) s -= ply; else if (s < -MATE_BOUND) s += ply;
             
             if (tte.flag == EXACT) return s;
             if (tte.flag == ALPHA && s <= alpha) return alpha;
             if (tte.flag == BETA && s >= beta) return beta;
        }
    }
    
    // Static Eval
    int eval = 0;
    if (!in_check) {
        eval = Eval::evaluate(pos);
        
        // RFP (Reverse Futility Pruning)
        if (depth <= 7 && eval - SP.RFP_Margin * depth >= beta) {
            return eval;
        }
        
        // Null Move
        if (do_null && depth >= 3 && eval >= beta) {
            StateInfo st;
            pos.make_null_move(st);
            int R = 3 + depth/4;
            int nm = -search(pos, -beta, -beta+1, depth-R-1, ply+1, td, false);
            pos.unmake_null_move();
            if (stop_search) return 0;
            if (nm >= beta) return beta;
        }
    }
    
    MovePicker mp(pos, hash_move, ply);
    Move m;
    int moves_played = 0;
    int best_score = -INFINITE_SCORE;
    Move best_move = Move::none();
    TTFlag flag = ALPHA;
    
    while(mp.next(m)) {
        if (!pos.is_legal(m)) continue;
        
        moves_played++;
        
        // Prefetch TT
        // MCache::prefetch(...)

        StateInfo st;
        pos.make_move(m, st);
        
        int score;
        if (moves_played == 1) {
            score = -search(pos, -beta, -alpha, depth-1, ply+1, td, true);
        } else {
            // LMR
            int R = 0;
            if (depth >= 3 && moves_played > 1 && !in_check && !pos.is_capture(m)) {
                 R = SP.LMR_Base + std::log(moves_played) * std::log(depth) / SP.LMR_Factor;
            }
            
            score = -search(pos, -alpha-1, -alpha, depth-1-R, ply+1, td, true);
            if (score > alpha && R > 0) {
                 score = -search(pos, -alpha-1, -alpha, depth-1, ply+1, td, true);
            }
            if (score > alpha && score < beta) {
                 score = -search(pos, -beta, -alpha, depth-1, ply+1, td, true);
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
                if (alpha >= beta) {
                    if (!pos.is_capture(m)) {
                        Killers[ply][1] = Killers[ply][0];
                        Killers[ply][0] = m;
                        int bonus = depth * depth;
                        History[pos.side_to_move()][m.from()][m.to()].fetch_add(bonus, std::memory_order_relaxed);
                    }
                    TT.store(pos.hash(), m, beta, depth, BETA, ply);
                    return beta;
                }
            }
        }
    }
    
    if (moves_played == 0) return in_check ? -MATE_BOUND + ply : 0;
    
    TT.store(pos.hash(), best_move, best_score, depth, flag, ply);
    return best_score;
}

// --- Root ---

void iterate(Position& pos, Limits limits) {
    refresh_params();
    stop_search = false;
    start_time = std::chrono::steady_clock::now();
    nodes_searched = 0;
    
    // Clear heuristics if new game? 
    // std::memset(Killers, 0, sizeof(Killers)); 

    run_infinite = limits.infinite;
    time_hard_limit = limits.time; 
    time_soft_limit = time_hard_limit / 2; // Simple heuristic
    
    if (limits.use_time && !limits.is_movetime) {
         // Standard: Time / MovesLeft
         int moves = limits.movestogo > 0 ? limits.movestogo : 25;
         long long t = limits.time / moves + limits.inc;
         time_soft_limit = t;
         time_hard_limit = t * 5; 
         if (time_hard_limit > limits.time) time_hard_limit = limits.time - 50;
    }
    
    if (time_hard_limit < 10 && limits.use_time) time_hard_limit = 10;
    
    int num_threads = Tune::get("Threads");
    thread_pool.init(num_threads);
    std::vector<std::unique_ptr<ThreadData>> tds;
    for(int i=0; i<num_threads; ++i) tds.push_back(std::make_unique<ThreadData>(i));
    
    if (num_threads > 1) {
        thread_pool.start_search([&](int id) {
            if (id == 0) return;
            int a = -INFINITE_SCORE, b = INFINITE_SCORE;
            for(int d=1; d<=MAX_PLY; ++d) {
                if(stop_search) break;
                search(pos, a, b, d, 0, *tds[id], true);
            }
        });
    }
    
    Move best_move = Move::none();
    int alpha = -INFINITE_SCORE;
    int beta = INFINITE_SCORE;
    int score = 0;
    
    for(int depth = 1; depth <= limits.depth || limits.depth == 0; ++depth) {
        if (depth >= MAX_PLY) break;
        
        // Aspiration
        if (depth >= 5) {
            int delta = 20;
            alpha = std::max(-INFINITE_SCORE, score - delta);
            beta = std::min(INFINITE_SCORE, score + delta);
            
            while(true) {
                 score = search(pos, alpha, beta, depth, 0, *tds[0], true);
                 if (stop_search) break;
                 
                 if (score <= alpha) {
                     beta = (alpha + beta) / 2;
                     alpha = std::max(-INFINITE_SCORE, alpha - delta*2);
                 } else if (score >= beta) {
                     beta = std::min(INFINITE_SCORE, beta + delta*2);
                 } else {
                     break;
                 }
                 delta += delta/2;
            }
        } else {
            alpha = -INFINITE_SCORE; beta = INFINITE_SCORE;
            score = search(pos, alpha, beta, depth, 0, *tds[0], true);
        }
        
        if (stop_search) break;
        
        // Stats
        long long nodes = 0;
        for(auto& t : tds) nodes += t->nodes;
        auto now = std::chrono::steady_clock::now();
        long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
        if (ms == 0) ms = 1;
        
        // PV Extraction
        std::vector<Move> pv;
        TTEntry tte;
        Position p_sim = pos;
        for(int i=0; i<depth; ++i) {
             if (TT.probe(p_sim.hash(), tte) && tte.move != Move::none()) {
                 if (p_sim.is_legal(tte.move)) {
                     pv.push_back(tte.move);
                     StateInfo st;
                     p_sim.make_move(tte.move, st);
                 } else break;
             } else break;
        }
        if (!pv.empty()) best_move = pv[0];
        
        std::cout << "info depth " << depth << " seldepth " << depth 
                  << " score cp " << score 
                  << " nodes " << nodes << " nps " << (nodes*1000/ms)
                  << " time " << ms << " pv";
        for(Move m : pv) std::cout << " " << m.to_string();
        std::cout << std::endl;
        
        if (limits.use_time && !limits.is_movetime && ms > time_soft_limit) break;
    }
    
    stop_search = true;
    thread_pool.wait_for_completion();
    
    if (best_move == Move::none()) {
         MoveGen::MoveList moves;
         MoveGen::generate<MoveGen::ALL>(pos, moves);
         if (moves.count > 0) best_move = moves[0];
    }
    
    std::cout << "bestmove " << best_move.to_string() << std::endl;
}

void clear() {
    for(int c=0; c<COLOR_NB; ++c)
        for(int f=0; f<SQ_NB; ++f)
            for(int t=0; t<SQ_NB; ++t)
                History[c][f][t] = 0;
    TT.clear();
}

} // namespace Search