#include "movegen.h"

namespace MoveGen {

template<GenType T>
void generate(const Position& pos, MoveList& moves) {
    Color us = pos.side_to_move();
    Color them = static_cast<Color>(us ^ 1);
    Bitboard occupied = pos.all_pieces();
    Bitboard enemies = pos.pieces(them);
    Bitboard targets = (T == CAPTURES) ? enemies : (T == QUIETS) ? ~occupied : ~pos.pieces(us);

    // Pawns - iterate per-pawn to ensure correct EP, promotions and double-push logic
    Bitboard pawns = pos.pieces(us, PAWN);
    Direction up = (us == WHITE) ? NORTH : SOUTH;
    Bitboard rank2 = (us == WHITE) ? 0x000000000000FF00ULL : 0x00FF000000000000ULL; // pawns that can double
    Bitboard rank7 = (us == WHITE) ? 0x00FF000000000000ULL : 0x000000000000FF00ULL; // pawns that promote on next move

    Bitboard b = pawns;
    Square ep_sq = pos.state_ptr()->ep_square;

    while (b) {
        Square from = Bitboards::pop_lsb(b);
        int t1 = static_cast<int>(from) + up;

        // Single push / promotion by push (validate board indices)
        if (t1 >= 0 && t1 < SQ_NB && T != CAPTURES) {
            Square to1 = static_cast<Square>(t1);
            if (pos.piece_on(to1) == NO_PIECE) {
                if (Bitboards::square_bb(from) & rank7) {
                    moves.add(Move(from, to1, PROMOTION, QUEEN));
                    moves.add(Move(from, to1, PROMOTION, ROOK));
                    moves.add(Move(from, to1, PROMOTION, BISHOP));
                    moves.add(Move(from, to1, PROMOTION, KNIGHT));
                } else {
                    moves.add(Move(from, to1));

                    // Double push
                    if ((Bitboards::square_bb(from) & rank2) != 0) {
                        int t2 = static_cast<int>(from) + 2 * up;
                        if (t2 >= 0 && t2 < SQ_NB) {
                            Square to2 = static_cast<Square>(t2);
                            if (pos.piece_on(to2) == NO_PIECE && pos.piece_on(to1) == NO_PIECE) {
                                moves.add(Move(from, to2));
                            }
                        }
                    }
                }
            }
        }

        // Captures (including promotions)
        Bitboard attacks = Bitboards::pawn_attacks(from, us);
        Bitboard caps = attacks & enemies;
        Bitboard tmp = caps;
        while (tmp) {
            Square to = Bitboards::pop_lsb(tmp);
            if (Bitboards::square_bb(from) & rank7) {
                moves.add(Move(from, to, PROMOTION, QUEEN));
                moves.add(Move(from, to, PROMOTION, ROOK));
                moves.add(Move(from, to, PROMOTION, BISHOP));
                moves.add(Move(from, to, PROMOTION, KNIGHT));
            } else {
                moves.add(Move(from, to));
            }
        }

        // En passant
        if (ep_sq != SQ_NONE && (attacks & Bitboards::square_bb(ep_sq))) {
            // EP is a capture type - generate only when captures allowed
            if (T != QUIETS) {
                moves.add(Move(from, ep_sq, EN_PASSANT));
            }
        }
    }

    // Knights
    Bitboard knights = pos.pieces(us, KNIGHT);
    while (knights) {
        Square from = Bitboards::pop_lsb(knights);
        Bitboard attacks = Bitboards::knight_attacks(from) & targets;
        while (attacks) {
            moves.add(Move(from, Bitboards::pop_lsb(attacks)));
        }
    }

    // Bishops
    Bitboard bishops = pos.pieces(us, BISHOP);
    while (bishops) {
        Square from = Bitboards::pop_lsb(bishops);
        Bitboard attacks = Bitboards::bishop_attacks(from, occupied) & targets;
        while (attacks) {
            moves.add(Move(from, Bitboards::pop_lsb(attacks)));
        }
    }

    // Rooks
    Bitboard rooks = pos.pieces(us, ROOK);
    while (rooks) {
        Square from = Bitboards::pop_lsb(rooks);
        Bitboard attacks = Bitboards::rook_attacks(from, occupied) & targets;
        while (attacks) {
            moves.add(Move(from, Bitboards::pop_lsb(attacks)));
        }
    }

    // Queens
    Bitboard queens = pos.pieces(us, QUEEN);
    while (queens) {
        Square from = Bitboards::pop_lsb(queens);
        Bitboard attacks = Bitboards::queen_attacks(from, occupied) & targets;
        while (attacks) {
            moves.add(Move(from, Bitboards::pop_lsb(attacks)));
        }
    }

    // King (including castling generation)
    Bitboard king = pos.pieces(us, KING);
    if (king) {
        Square from = Bitboards::lsb(king);
        Bitboard attacks = Bitboards::king_attacks(from) & targets;
        while (attacks) {
            moves.add(Move(from, Bitboards::pop_lsb(attacks)));
        }

        // Castling: ensure rights, empty squares and not passing through/into check
        uint8_t castle = pos.state_ptr()->castle_rights;
        // Only add castling as quiet moves (not captures)
        if (T != CAPTURES) {
            if (us == WHITE) {
                // King side
                if ((castle & 1) != 0) {
                    if (pos.piece_on(SQ_F1) == NO_PIECE && pos.piece_on(SQ_G1) == NO_PIECE) {
                        if (!pos.is_attacked(from, them) && !pos.is_attacked(SQ_F1, them) && !pos.is_attacked(SQ_G1, them)) {
                            moves.add(Move(from, SQ_G1, CASTLING));
                        }
                    }
                }
                // Queen side
                if ((castle & 2) != 0) {
                    if (pos.piece_on(SQ_B1) == NO_PIECE && pos.piece_on(SQ_C1) == NO_PIECE && pos.piece_on(SQ_D1) == NO_PIECE) {
                        if (!pos.is_attacked(from, them) && !pos.is_attacked(SQ_D1, them) && !pos.is_attacked(SQ_C1, them)) {
                            moves.add(Move(from, SQ_C1, CASTLING));
                        }
                    }
                }
            } else {
                if ((castle & 4) != 0) {
                    if (pos.piece_on(SQ_F8) == NO_PIECE && pos.piece_on(SQ_G8) == NO_PIECE) {
                        if (!pos.is_attacked(from, them) && !pos.is_attacked(SQ_F8, them) && !pos.is_attacked(SQ_G8, them)) {
                            moves.add(Move(from, SQ_G8, CASTLING));
                        }
                    }
                }
                if ((castle & 8) != 0) {
                    if (pos.piece_on(SQ_B8) == NO_PIECE && pos.piece_on(SQ_C8) == NO_PIECE && pos.piece_on(SQ_D8) == NO_PIECE) {
                        if (!pos.is_attacked(from, them) && !pos.is_attacked(SQ_D8, them) && !pos.is_attacked(SQ_C8, them)) {
                            moves.add(Move(from, SQ_C8, CASTLING));
                        }
                    }
                }
            }
        }
    }
}


template<GenType T>
void generate(const Position& pos, std::vector<Move>& moves) {
    MoveList ml;
    generate<T>(pos, ml);
    for (int i = 0; i < ml.count; ++i) {
        moves.push_back(ml.moves[i]);
    }
}

// Explicit template instantiation
template void generate<ALL>(const Position& pos, MoveList& moves);
template void generate<CAPTURES>(const Position& pos, MoveList& moves);
template void generate<QUIETS>(const Position& pos, MoveList& moves);

template void generate<ALL>(const Position& pos, std::vector<Move>& moves);
template void generate<CAPTURES>(const Position& pos, std::vector<Move>& moves);
template void generate<QUIETS>(const Position& pos, std::vector<Move>& moves);

} // namespace MoveGen
