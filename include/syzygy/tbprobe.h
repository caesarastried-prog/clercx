#ifndef TBPROBE_H
#define TBPROBE_H

#include "position.h"
#include <string>

namespace Syzygy {

void init(const std::string& path);
bool probe_wdl(const Position& pos, int& score);
bool probe_root(const Position& pos, Move& best_move, int& score);

}

#endif // TBPROBE_H
