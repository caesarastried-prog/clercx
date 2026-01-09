#ifndef SEARCH_H
#define SEARCH_H

#include "position.h"
#include "move.h"
#include <atomic>
#include <vector>

namespace Search {

struct Limits {
    int depth = 0;
    long long time = 0;
    long long inc = 0;
    long long movestogo = 0;
    long long nodes = 0;
    bool use_time = false;
    bool is_movetime = false;
    bool infinite = false;
    bool ponder = false;
    std::vector<Move> searchmoves; // Restrict search to these moves
};

struct SearchInfo {
    int depth;
    int seldepth;
    long long nodes;
    int time_ms;
    int score;
};

// Global stop flag
extern std::atomic<bool> stop_search;

// Main entry point
void iterate(Position& pos, Limits limits);

// Clear heuristics (History, Killers, etc.)
void clear();

} // namespace Search

#endif // SEARCH_H