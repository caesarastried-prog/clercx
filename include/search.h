#ifndef SEARCH_H
#define SEARCH_H

#include "position.h"
#include "move.h"
#include <atomic>

namespace Search {

struct Limits {
    int depth;
    long long time;
    long long inc;
    long long movestogo;
    bool use_time;
    bool is_movetime;
};

struct SearchInfo {
    int depth;
    int seldepth;
    long long nodes;
    int time;
    int score; // cp or mate
};

extern std::atomic<bool> stop_search;

void iterate(Position& pos, Limits limits);

} // namespace Search

#endif // SEARCH_H
