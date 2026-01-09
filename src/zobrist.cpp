#include "zobrist.h"
#include <random>

namespace Zobrist {

uint64_t piece_keys[PIECE_NB][SQ_NB];
uint64_t side_key;
uint64_t castle_keys[16];
uint64_t en_passant_keys[SQ_NB];

void init() {
    std::mt19937_64 rng(1070372); // Deterministic seed
    
    for (int p = 0; p < PIECE_NB; ++p)
        for (int s = 0; s < SQ_NB; ++s)
            piece_keys[p][s] = rng();
            
    side_key = rng();
    
    for (int i = 0; i < 16; ++i)
        castle_keys[i] = rng();
        
    for (int i = 0; i < SQ_NB; ++i)
        en_passant_keys[i] = rng();
}

} // namespace Zobrist
