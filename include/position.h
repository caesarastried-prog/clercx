#ifndef POSITION_H
#define POSITION_H

#include "types.h"
#include "bitboard.h"
#include "move.h"
#include <string>
#include <vector>

struct StateInfo {
    uint8_t castle_rights;
    Square ep_square;
    int halfmove_clock;
    uint64_t key;
    Piece captured_piece;
    int material_score;
    int pst_score;
    StateInfo* previous;
};

class Position {
public:
    Position();
    void set_fen(const std::string& fen);
    
    Bitboard pieces(Color c) const { return color_bb[c]; }
    Bitboard pieces(PieceType pt) const { return type_bb[pt]; }
    Bitboard pieces(Color c, PieceType pt) const { return color_bb[c] & type_bb[pt]; }
    Bitboard all_pieces() const { return color_bb[WHITE] | color_bb[BLACK]; }
    
    Piece piece_on(Square s) const { return board[s]; }
    Color side_to_move() const { return side; }
    
    uint64_t hash() const { return state->key; }
    const StateInfo* state_ptr() const { return state; }
    
    void make_move(Move m, StateInfo& next_state);
    void unmake_move(Move m);
    
    void make_null_move(StateInfo& next_state);
    void unmake_null_move();

    bool is_attacked(Square s, Color attacker) const;
    bool is_draw() const;
    bool is_repetition() const;
    bool is_insufficient_material() const;
    
    bool is_pseudo_legal(Move m) const;
    bool is_legal(Move m) const;

    uint64_t hash_history[1024];
    int history_index;

private:
    void clear();
    void put_piece(Piece p, Square s);
    void remove_piece(Square s);

    Piece board[SQ_NB];
    Bitboard color_bb[COLOR_NB];
    Bitboard type_bb[PIECE_TYPE_NB];
    
    Color side;
    StateInfo* state;
    StateInfo start_state;
};

#endif // POSITION_H