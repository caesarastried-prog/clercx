#ifndef ZOBRIST_H
#define ZOBRIST_H

#include "types.h"

namespace Zobrist {

extern uint64_t piece_keys[PIECE_NB][SQ_NB];
extern uint64_t side_key;
extern uint64_t castle_keys[16];
extern uint64_t en_passant_keys[SQ_NB];

void init();

} // namespace Zobrist

#endif // ZOBRIST_H
