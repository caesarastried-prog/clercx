#ifndef SCORE_H
#define SCORE_H

#include "types.h"
#include <string>
#include <cmath>

namespace ClercX {

using Value = int;

constexpr Value VALUE_ZERO = 0;
constexpr Value VALUE_DRAW = 0;
constexpr Value VALUE_MATE = 32000;
constexpr Value VALUE_INFINITE = 32001;
constexpr Value VALUE_NONE = 32002;

constexpr Value VALUE_MATE_IN_MAX_PLY = VALUE_MATE - 2 * MAX_PLY;
constexpr Value VALUE_MATED_IN_MAX_PLY = -VALUE_MATE + 2 * MAX_PLY;

class Score {
public:
    static std::string to_uci(Value v);
    static bool is_mate(Value v);
    static int mate_in(Value v);
    static Value normalize(Value v, int ply);
    static Value contempt(Value v, Color us);
};

} // namespace ClercX

#endif // SCORE_H