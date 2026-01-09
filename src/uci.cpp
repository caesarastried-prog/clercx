#include "uci.h"
#include "search.h"
#include "zobrist.h"
#include "movegen.h"
#include "ucioption.h"
#include "tune.h"
#include "bitboard.h"
#include "tt.h"
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <deque>
#include <algorithm>

namespace UCI {

void loop() {
    Position pos;
    // Use deque for stable pointers
    std::deque<StateInfo> game_history;
    
    pos.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    std::string line, token;
    while (std::getline(std::cin, line)) {
        std::stringstream ss(line);
        ss >> token;
        
        if (token == "uci") {
            std::cout << "id name ClercX S+++" << std::endl;
            std::cout << "id author Gemini Agent" << std::endl;
            
            Tune::print_params();
            
            std::cout << "option name Hash type spin default 16 min 1 max 8192" << std::endl;
            std::cout << "option name Threads type spin default 1 min 1 max 128" << std::endl;
            std::cout << "uciok" << std::endl;
        } else if (token == "setoption") {
            std::string name, value;
            std::string sub;
            ss >> sub; // "name"
            
            while (ss >> sub && sub != "value") {
                if (!name.empty()) name += " ";
                name += sub;
            }
            
            if (sub == "value") {
                ss >> value;
                if (Tune::get(name) != 0 || name.find("Val") != std::string::npos || name.find("LMR") != std::string::npos) {
                    try {
                        Tune::set(name, std::stoi(value));
                    } catch (...) {}
                }
                
                if (name == "Hash") {
                    try {
                        TT.resize(std::stoi(value));
                    } catch (...) {}
                }
                if (name == "Threads") {
                    // Threads handled in Search::start
                }
            }
        } else if (token == "isready") {
            std::cout << "readyok" << std::endl;
        } else if (token == "ucinewgame") {
            TT.clear();
            game_history.clear();
        } else if (token == "position") {
            std::string sub;
            ss >> sub;
            game_history.clear();
            
            if (sub == "startpos") {
                pos.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
                ss >> sub; 
            } else if (sub == "fen") {
                std::string fen = "";
                while (ss >> sub && sub != "moves") {
                    if (!fen.empty()) fen += " ";
                    fen += sub;
                }
                pos.set_fen(fen);
            }
            
            if (sub == "moves") {
                std::string move_str;
                while (ss >> move_str) {
                    Move move = Move::none();
                    // We must generate moves to map string to Move object (with correct type)
                    std::vector<Move> legal_moves;
                    MoveGen::generate<MoveGen::ALL>(pos, legal_moves);
                    
                    for (Move m : legal_moves) {
                        std::string m_str = "";
                        m_str += static_cast<char>('a' + (m.from() % 8));
                        m_str += std::to_string(m.from() / 8 + 1);
                        m_str += static_cast<char>('a' + (m.to() % 8));
                        m_str += std::to_string(m.to() / 8 + 1);
                        if (m.type() == PROMOTION) {
                            switch (m.promotion_piece()) {
                                case QUEEN: m_str += 'q'; break;
                                case ROOK: m_str += 'r'; break;
                                case BISHOP: m_str += 'b'; break;
                                case KNIGHT: m_str += 'n'; break;
                                default: break;
                            }
                        }
                        if (m_str == move_str) {
                            move = m;
                            break;
                        }
                    }
                    
                    if (move != Move::none()) {
                        // Validate legality strictly
                         game_history.emplace_back();
                         pos.make_move(move, game_history.back());
                         
                         Square k = Bitboards::lsb(pos.pieces(static_cast<Color>(pos.side_to_move() ^ 1), KING));
                         if (k != SQ_NONE && pos.is_attacked(k, pos.side_to_move())) {
                             // Illegal move in input! Revert.
                             pos.unmake_move(move);
                             game_history.pop_back();
                         }
                    }
                }
            }
        } else if (token == "go") {
            Search::Limits limits;
            limits.depth = 100;
            limits.time = 0;
            limits.inc = 0;
            limits.movestogo = 0;
            limits.use_time = false;
            limits.is_movetime = false;

            std::string sub;
            while (ss >> sub) {
                if (sub == "depth") ss >> limits.depth;
                else if (sub == "wtime" && pos.side_to_move() == WHITE) { ss >> limits.time; limits.use_time = true; }
                else if (sub == "btime" && pos.side_to_move() == BLACK) { ss >> limits.time; limits.use_time = true; }
                else if (sub == "winc" && pos.side_to_move() == WHITE) ss >> limits.inc;
                else if (sub == "binc" && pos.side_to_move() == BLACK) ss >> limits.inc;
                else if (sub == "movestogo") ss >> limits.movestogo;
                else if (sub == "movetime") { ss >> limits.time; limits.use_time = true; limits.is_movetime = true; }
                else if (sub == "infinite") { limits.depth = 100; limits.use_time = false; }
            }
            Search::iterate(pos, limits);
        } else if (token == "stop") {
            Search::stop_search = true;
        } else if (token == "quit") {
            break;
        }
    }
}

} // namespace UCI
