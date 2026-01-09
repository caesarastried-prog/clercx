#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "position.h"
#include "move.h"
#include <vector>

namespace MoveGen {

enum GenType {
    ALL, CAPTURES, QUIETS
};

struct MoveList {
    Move moves[256];
    int count = 0;

    void add(Move m) { moves[count++] = m; }
    Move* begin() { return moves; }
    Move* end() { return moves + count; }
    size_t size() const { return count; }
    Move& operator[](int i) { return moves[i]; }
    const Move& operator[](int i) const { return moves[i]; }
};

template<GenType T>
void generate(const Position& pos, MoveList& moves);

// Keep vector version for compatibility if needed, but we should migrate.
template<GenType T>
void generate(const Position& pos, std::vector<Move>& moves);

} // namespace MoveGen

#endif // MOVEGEN_H
