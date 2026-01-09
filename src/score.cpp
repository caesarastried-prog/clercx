#include "../include/score.h"
#include "../include/ucioption.h"
#include <sstream>
#include <cstdlib>

namespace ClercX {

std::string Score::to_uci(Value v) {
    if (is_mate(v)) {
        int moves = mate_in(v);
        std::stringstream ss;
        ss << "mate " << moves;
        return ss.str();
    }
    std::stringstream ss;
    ss << "cp " << v;
    return ss.str();
}

bool Score::is_mate(Value v) {
    return std::abs(v) > VALUE_MATE_IN_MAX_PLY;
}

int Score::mate_in(Value v) {
    int moves = 0;
    if (v > 0) {
        moves = (VALUE_MATE - v + 1) / 2;
    } else {
        moves = -(VALUE_MATE + v + 1) / 2;
    }
    return moves;
}

Value Score::normalize(Value v, int) {
    // Score is usually independent of ply in simple engines, but for mate scores
    // we often store them relative to root to work in TT.
    // If v is mate-in-N relative to current ply, to store in TT we might adjust.
    // But typically 'normalize' means getting it ready for UI.
    return v; 
}

Value Score::contempt(Value v, Color) {
    // Adjust for contempt factor
    int contemptVal = Options["Contempt"];
    (void)contemptVal;
    // If we are playing with contempt, we value draws lower (negative contempt)
    // or higher depending on perspective.
    // Usually: Score += Contempt if we are stronger?
    // Implementation varies.
    return v;
}

} // namespace ClercX