#include "position.h"
#include "zobrist.h"
#include <sstream>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cassert>

Position::Position() {
    clear();
}

void Position::clear() {
    for (int i = 0; i < SQ_NB; ++i) board[i] = NO_PIECE;
    color_bb[WHITE] = color_bb[BLACK] = 0;
    for (int i = 0; i < PIECE_TYPE_NB; ++i) type_bb[i] = 0;
    
    side = WHITE;
    state = &start_state;
    state->castle_rights = 0;
    state->ep_square = SQ_NONE;
    state->halfmove_clock = 0;
    state->key = 0;
    state->material_score = 0; 
    state->pst_score = 0;      
    state->previous = nullptr;
    history_index = 0;
    
    std::memset(hash_history, 0, sizeof(hash_history));
}

void Position::put_piece(Piece p, Square s) {
    board[s] = p;
    Bitboard b = Bitboards::square_bb(s);
    color_bb[color_of(p)] |= b;
    type_bb[type_of(p)] |= b;
    state->key ^= Zobrist::piece_keys[p][s];
}

void Position::remove_piece(Square s) {
    Piece p = board[s];
    Bitboard b = Bitboards::square_bb(s);
    color_bb[color_of(p)] &= ~b;
    type_bb[type_of(p)] &= ~b;
    board[s] = NO_PIECE;
    state->key ^= Zobrist::piece_keys[p][s];
}

bool Position::is_attacked(Square s, Color attacker) const {
    Bitboard occupied = all_pieces();
    if (Bitboards::pawn_attacks(s, static_cast<Color>(attacker ^ 1)) & pieces(attacker, PAWN)) return true;
    if (Bitboards::knight_attacks(s) & pieces(attacker, KNIGHT)) return true;
    if (Bitboards::bishop_attacks(s, occupied) & (pieces(attacker, BISHOP) | pieces(attacker, QUEEN))) return true;
    if (Bitboards::rook_attacks(s, occupied) & (pieces(attacker, ROOK) | pieces(attacker, QUEEN))) return true;
    if (Bitboards::king_attacks(s) & pieces(attacker, KING)) return true;
    return false;
}

bool Position::is_draw() const {
    if (state->halfmove_clock >= 100) return true;
    if (is_repetition()) return true;
    if (is_insufficient_material()) return true;
    return false;
}

bool Position::is_repetition() const {
    int count = 1;
    int end = std::max(0, history_index - state->halfmove_clock);
    
    for (int i = history_index - 2; i >= end; i -= 2) {
        if (hash_history[i] == state->key) {
            count++;
            if (count >= 3) return true;
        }
    }
    return false;
}

bool Position::is_insufficient_material() const {
    Bitboard all = all_pieces();
    int count = Bitboards::count(all);
    
    if (count == 2) return true; 
    
    if (count == 3) {
        if (type_bb[KNIGHT] || type_bb[BISHOP]) return true;
    }
    
    if (count == 4) {
        if (Bitboards::count(type_bb[BISHOP]) == 2) {
            if (pieces(WHITE, BISHOP) && pieces(BLACK, BISHOP)) {
                Square w_sq = Bitboards::lsb(pieces(WHITE, BISHOP));
                Square b_sq = Bitboards::lsb(pieces(BLACK, BISHOP));
                bool w_light = Bitboards::is_light_square(w_sq);
                bool b_light = Bitboards::is_light_square(b_sq);
                if (w_light == b_light) return true;
            }
        }
    }
    return false;
}

bool Position::is_pseudo_legal(Move m) const {
    Square from = m.from();
    Square to = m.to();
    Piece p = board[from];
    
    if (p == NO_PIECE || color_of(p) != side) return false;
    
    // Destination cannot be occupied by own piece
    if (board[to] != NO_PIECE && color_of(board[to]) == side) return false;

    MoveType type = m.type();
    
    if (type_of(p) == PAWN) {
        Direction up = (side == WHITE) ? NORTH : SOUTH;
        if (type == PROMOTION || type == NORMAL) {
            if (to == from + up) return board[to] == NO_PIECE;
            if ((side == WHITE && from >= SQ_A2 && from <= SQ_H2 && to == from + up + up) ||
                (side == BLACK && from >= SQ_A7 && from <= SQ_H7 && to == from + up + up)) {
                return board[from + up] == NO_PIECE && board[to] == NO_PIECE;
            }
            if (Bitboards::pawn_attacks(from, side) & Bitboards::square_bb(to)) {
                if (board[to] != NO_PIECE) return color_of(board[to]) != side;
                if (to == state->ep_square) return true;
            }
        }
        if (type == EN_PASSANT) {
            // Must target EP square
            if (to != state->ep_square) return false;
            // Must be diagonal attack
            if (!((Bitboards::pawn_attacks(from, side) & Bitboards::square_bb(to)))) return false;
            return true;
        }
        return false;
    }
    
    if (type == CASTLING) {
        if (type_of(p) != KING) return false;
        
        // Basic obstruction checks only (pseudo-legal). 
        // Real path attacks are for is_legal().
        if (side == WHITE) {
            if (to == SQ_G1) {
                if (!(state->castle_rights & 1)) return false;
                if (board[SQ_F1] != NO_PIECE || board[SQ_G1] != NO_PIECE) return false;
            } else if (to == SQ_C1) {
                if (!(state->castle_rights & 2)) return false;
                if (board[SQ_D1] != NO_PIECE || board[SQ_C1] != NO_PIECE || board[SQ_B1] != NO_PIECE) return false;
            } else return false;
        } else {
            if (to == SQ_G8) {
                if (!(state->castle_rights & 4)) return false;
                if (board[SQ_F8] != NO_PIECE || board[SQ_G8] != NO_PIECE) return false;
            } else if (to == SQ_C8) {
                if (!(state->castle_rights & 8)) return false;
                if (board[SQ_D8] != NO_PIECE || board[SQ_C8] != NO_PIECE || board[SQ_B8] != NO_PIECE) return false;
            } else return false;
        }
        return true;
    }

    Bitboard target = Bitboards::square_bb(to);
    switch (type_of(p)) {
        case KNIGHT: return (Bitboards::knight_attacks(from) & target) != 0;
        case BISHOP: return (Bitboards::bishop_attacks(from, all_pieces()) & target) != 0;
        case ROOK:   return (Bitboards::rook_attacks(from, all_pieces()) & target) != 0;
        case QUEEN:  return (Bitboards::queen_attacks(from, all_pieces()) & target) != 0;
        case KING:   return (Bitboards::king_attacks(from) & target) != 0;
        default:     return false;
    }
}

bool Position::is_legal(Move m) const {
    if (!is_pseudo_legal(m)) return false;
    
    // Castling special checks (cannot castle out of, through, or into check)
    if (m.type() == CASTLING) {
        Square from = m.from();
        Square to = m.to();
        Color us = side;
        Color them = static_cast<Color>(us ^ 1);
        
        if (is_attacked(from, them)) return false; // Cannot castle out of check
        
        if (us == WHITE) {
            if (to == SQ_G1) {
                if (is_attacked(SQ_F1, them) || is_attacked(SQ_G1, them)) return false;
            } else if (to == SQ_C1) {
                if (is_attacked(SQ_D1, them) || is_attacked(SQ_C1, them)) return false;
            }
        } else {
            if (to == SQ_G8) {
                if (is_attacked(SQ_F8, them) || is_attacked(SQ_G8, them)) return false;
            } else if (to == SQ_C8) {
                if (is_attacked(SQ_D8, them) || is_attacked(SQ_C8, them)) return false;
            }
        }
    }
    
    // Try move and check if king is left in check
    // We cannot use make_move/unmake_move here easily because make_move modifies 'state' pointer
    // and requires a new StateInfo buffer. 
    // Instead we do a light-weight check:
    // 1. If King moves, check if dest is attacked.
    // 2. If other moves, check if King is attacked.
    
    Square king_sq = Bitboards::lsb(pieces(side, KING));
    Square from = m.from();
    Square to = m.to();
    
    if (type_of(board[from]) == KING) {
        // King move
        // If castling, we already checked.
        if (m.type() != CASTLING) {
            // Temporarily update occupancy to check attacks accurately?
            // Actually, simply checking is_attacked(to) is almost enough, 
            // EXCEPT for x-rays through the king's original position (rare but possible).
            // But 'is_attacked' checks current board.
            // If we move king, we need to know if 'to' is attacked on the *resulting* board.
            // The safest way without full make_move is to verify attacks ignoring the king's old square.
            // Standard engine approach: just make the move.
            
            StateInfo st;
            // Since we are const, we need a non-const copy or cast. 
            // Better: use the full make_move logic on a temporary copy or refactor.
            // Given performance is less critical for 'is_legal' (used in UI/root), copy is fine.
            Position tmp = *this; 
            tmp.make_move(m, st);
            return !tmp.is_attacked(Bitboards::lsb(tmp.pieces(side, KING)), static_cast<Color>(side ^ 1));
        }
        return true;
    } else {
        // En Passant is tricky (removes two pieces, adds one)
        // Standard moves: remove from, add to.
        // Pin checks.
        
        // For S+++ robustness, let's just do the make_move check.
        // It's safer and cleaner than custom pin logic if we want "crash proof".
        Position tmp = *this;
        StateInfo st;
        tmp.make_move(m, st);
        
        Square k = Bitboards::lsb(tmp.pieces(side, KING));
        return !tmp.is_attacked(k, static_cast<Color>(side ^ 1));
    }
}

const int CastlePerm[64] = {
    13, 15, 15, 15, 12, 15, 15, 14, // Rank 1
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    7,  15, 15, 15, 3,  15, 15, 11  // Rank 8
};

void Position::make_move(Move m, StateInfo& next_state) {
    Square from = m.from();
    Square to = m.to();
    MoveType type = m.type();
    Piece p = board[from];
    Piece captured = board[to]; 
    
    assert(p != NO_PIECE);
    assert(color_of(p) == side);

    next_state = *state;
    next_state.previous = state;
    next_state.captured_piece = captured;
    state = &next_state;
    
    state->key ^= Zobrist::castle_keys[state->castle_rights];
    if (state->ep_square != SQ_NONE) {
        state->key ^= Zobrist::en_passant_keys[state->ep_square];
    }
    
    state->castle_rights &= CastlePerm[from];
    state->castle_rights &= CastlePerm[to];
    state->ep_square = SQ_NONE; 
    state->halfmove_clock++;
    
    if (type_of(p) == PAWN || captured != NO_PIECE) {
        state->halfmove_clock = 0;
    }
    
    state->key ^= Zobrist::side_key; 

    remove_piece(from);
    
    if (captured != NO_PIECE) {
        remove_piece(to);
    } else if (type == EN_PASSANT) {
        Square ep_victim = (side == WHITE) ? static_cast<Square>(to + SOUTH) : static_cast<Square>(to + NORTH);
        next_state.captured_piece = board[ep_victim];
        remove_piece(ep_victim);
    }
    
    if (type == PROMOTION) {
        put_piece(make_piece(side, m.promotion_piece()), to);
    } else if (type == CASTLING) {
        put_piece(p, to);
        Square r_from, r_to;
        if (to == SQ_G1) { r_from = SQ_H1; r_to = SQ_F1; }
        else if (to == SQ_C1) { r_from = SQ_A1; r_to = SQ_D1; }
        else if (to == SQ_G8) { r_from = SQ_H8; r_to = SQ_F8; }
        else { r_from = SQ_A8; r_to = SQ_D8; }
        
        Piece rook = board[r_from];
        remove_piece(r_from);
        put_piece(rook, r_to);
    } else {
        put_piece(p, to);
    }
    
    if (type_of(p) == PAWN && std::abs(from - to) == 16) {
        state->ep_square = static_cast<Square>((from + to) / 2);
        state->key ^= Zobrist::en_passant_keys[state->ep_square];
    }
    
    state->key ^= Zobrist::castle_keys[state->castle_rights];
    
    side = static_cast<Color>(side ^ 1);
    
    if (history_index < 1024) {
        hash_history[history_index++] = state->key;
    }
}

void Position::unmake_move(Move m) {
    history_index--;
    side = static_cast<Color>(side ^ 1);
    
    Square from = m.from();
    Square to = m.to();
    MoveType type = m.type();
    
    Piece p_moved = board[to]; 
    
    if (type == PROMOTION) {
        remove_piece(to);
        p_moved = make_piece(side, PAWN); 
    } else if (type == CASTLING) {
        remove_piece(to);
        Square r_from, r_to;
        if (to == SQ_G1) { r_from = SQ_H1; r_to = SQ_F1; }
        else if (to == SQ_C1) { r_from = SQ_A1; r_to = SQ_D1; }
        else if (to == SQ_G8) { r_from = SQ_H8; r_to = SQ_F8; }
        else { r_from = SQ_A8; r_to = SQ_D8; }
        Piece rook = board[r_to];
        remove_piece(r_to);
        put_piece(rook, r_from);
        
        p_moved = make_piece(side, KING); 
    } else {
        remove_piece(to);
    }
    
    put_piece(p_moved, from);
    
    Piece captured = state->captured_piece;
    if (captured != NO_PIECE) {
        if (type == EN_PASSANT) {
            Square ep_victim = (side == WHITE) ? static_cast<Square>(to + SOUTH) : static_cast<Square>(to + NORTH);
            put_piece(captured, ep_victim);
        } else {
            put_piece(captured, to);
        }
    }
    
    state = state->previous;
}

void Position::make_null_move(StateInfo& next_state) {
    next_state = *state;
    next_state.previous = state;
    state = &next_state;
    
    state->key ^= Zobrist::side_key;
    if (state->ep_square != SQ_NONE) {
        state->key ^= Zobrist::en_passant_keys[state->ep_square];
        state->ep_square = SQ_NONE;
    }
    
    state->halfmove_clock++;
    
    side = static_cast<Color>(side ^ 1);
    if (history_index < 1024) {
        hash_history[history_index++] = state->key;
    }
}

void Position::unmake_null_move() {
    history_index--;
    side = static_cast<Color>(side ^ 1);
    state = state->previous;
}

void Position::set_fen(const std::string& fen) {
    clear();
    std::stringstream ss(fen);
    std::string piece_str, side_str, castle_str, ep_str;
    int halfmove = 0, fullmove = 1;
    
    ss >> piece_str >> side_str >> castle_str >> ep_str >> halfmove >> fullmove;
    
    int r = 7, f = 0;
    for (char c : piece_str) {
        if (c == '/') { r--; f = 0; }
        else if (isdigit(c)) { f += c - '0'; }
        else {
            Piece p;
            switch(c) {
                case 'P': p = W_PAWN; break;
                case 'N': p = W_KNIGHT; break;
                case 'B': p = W_BISHOP; break;
                case 'R': p = W_ROOK; break;
                case 'Q': p = W_QUEEN; break;
                case 'K': p = W_KING; break;
                case 'p': p = B_PAWN; break;
                case 'n': p = B_KNIGHT; break;
                case 'b': p = B_BISHOP; break;
                case 'r': p = B_ROOK; break;
                case 'q': p = B_QUEEN; break;
                case 'k': p = B_KING; break;
                default: continue;
            }
            put_piece(p, static_cast<Square>(r * 8 + f));
            f++;
        }
    }
    
    side = (side_str == "w") ? WHITE : BLACK;
    if (side == BLACK) state->key ^= Zobrist::side_key;
    
    for (char c : castle_str) {
        if (c == 'K') state->castle_rights |= 1;
        if (c == 'Q') state->castle_rights |= 2;
        if (c == 'k') state->castle_rights |= 4;
        if (c == 'q') state->castle_rights |= 8;
    }
    state->key ^= Zobrist::castle_keys[state->castle_rights];
    
    if (ep_str != "-") {
        int f = ep_str[0] - 'a';
        int r = ep_str[1] - '1';
        state->ep_square = static_cast<Square>(r * 8 + f);
        state->key ^= Zobrist::en_passant_keys[state->ep_square];
    }
    
    state->halfmove_clock = halfmove;
    
    history_index = 0;
    hash_history[history_index++] = state->key;
}
