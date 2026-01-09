#ifndef BITBOARD_H
#define BITBOARD_H

#include "types.h"
#include <iostream>

namespace Bitboards {

inline Bitboard square_bb(Square sq) {
    return 1ULL << sq;
}

inline Square lsb(Bitboard bb) {
    if (bb == 0) return SQ_NONE;
    return static_cast<Square>(__builtin_ctzll(bb));
}

inline Square msb(Bitboard bb) {
    return static_cast<Square>(63 - __builtin_clzll(bb));
}

inline Square pop_lsb(Bitboard& bb) {
    Square s = lsb(bb);
    bb &= bb - 1;
    return s;
}

inline int count(Bitboard bb) {
    return __builtin_popcountll(bb);
}

inline bool is_light_square(Square s) {
    return ((s / 8) + (s % 8)) % 2 != 0;
}

void init();
void print(Bitboard bb);

Bitboard knight_attacks(Square s);
Bitboard king_attacks(Square s);
Bitboard bishop_attacks(Square s, Bitboard occupied);
Bitboard rook_attacks(Square s, Bitboard occupied);
Bitboard queen_attacks(Square s, Bitboard occupied);
Bitboard pawn_attacks(Square s, Color c);

extern Bitboard FileABB;
extern Bitboard Rank1BB;

} // namespace Bitboards

#endif // BITBOARD_H