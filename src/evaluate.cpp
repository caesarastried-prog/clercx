#include "evaluate.h"
#include "bitboard.h"
#include "misc.h"
#include <algorithm>
#include <array>

namespace Eval {

// --- Constants & Weights ---

// Material: Pawn, Knight, Bishop, Rook, Queen, King
const int Material[PIECE_TYPE_NB] = { 88, 325, 345, 510, 1025, 0 };

// Game Phase: Pawn, Knight, Bishop, Rook, Queen, King
const int PhaseInc[PIECE_TYPE_NB] = { 0, 1, 1, 2, 4, 0 };
const int TotalPhase = 16 * PhaseInc[PAWN] + 4 * PhaseInc[KNIGHT] + 4 * PhaseInc[BISHOP] + 4 * PhaseInc[ROOK] + 2 * PhaseInc[QUEEN];

// PSQTs (Symmetrical, defined for White)
// Flip for Black: sq ^ 56
// Order: Pawn, Knight, Bishop, Rook, Queen, King

const int PSQT_MG[PIECE_TYPE_NB][64] = {
    { // Pawn
         0,   0,   0,   0,   0,   0,   0,   0,
        98, 134,  61,  95,  68, 126,  34, -11,
        -6,   7,  26,  31,  65,  56,  25, -20,
       -14,  13,   6,  21,  23,  12,  17, -23,
       -27,  -2,  -5,  12,  17,   6,  10, -25,
       -26,  -4,  -4, -10,   3,   3,  33, -12,
       -35,  -1, -20, -23, -15,  24,  38, -22,
         0,   0,   0,   0,   0,   0,   0,   0
    },
    { // Knight
      -167, -89, -34, -49,  61, -97, -15, -107,
       -73, -41,  72,  36,  23,  62,   7,  -17,
       -47,  60,  37,  65,  84, 129,  73,   44,
        -9,  17,  19,  53,  37,  69,  18,   22,
       -13,   4,  16,  13,  28,  19,  21,   -8,
       -23,  -9,  12,  10,  19,  17,  25,  -16,
       -29, -53, -12,  -3,  -1,  18, -14,  -19,
      -105, -21, -58, -33, -17, -28, -19,  -23
    },
    { // Bishop
       -29,   4, -82, -37, -25, -42,   7,  -8,
       -26,  16, -18, -13,  30,  59,  18, -47,
       -16,  37,  43,  40,  35,  50,  37,  -2,
        -4,   5,  19,  50,  37,  37,   7,  -2,
        -6,  13,  13,  26,  34,  12,  10,   4,
         0,  15,  15,  15,  14,  27,  18,  10,
         4,  15,  16,   0,   7,  21,  33,   1,
       -33,  -3, -14, -21, -13, -12, -39, -21
    },
    { // Rook
        32,  42,  32,  51,  63,   9,  31,  43,
        27,  32,  58,  62,  80,  67,  26,  44,
        -5,  19,  26,  36,  17,  45,  61,  16,
       -24, -11,   7,  26,  24,  35,  -8, -20,
       -36, -26, -12,  -1,   9,  -7,   6, -23,
       -45, -25, -16, -17,   3,   0,  -5, -33,
       -44, -16, -20,  -9,  -1,  11,  -6, -71,
       -19, -13,   1,  17,  16,   7, -37, -26
    },
    { // Queen
       -28,   0,  29,  12,  59,  44,  43,  45,
       -24, -39,  -5,   1, -16,  57,  28,  54,
       -13, -17,   7,   8,  29,  56,  47,  57,
       -27, -27, -16, -16,  -1,  17,  -2,   1,
        -9, -26, -9, -10,  -2,  -4,   3,  -3,
       -14,   2, -11,  -2,  -5,   2,  14,   5,
       -35,  -8,  11,   2,   8,  15,  -3,   1,
        -1, -18,  -9, -19, -30, -15, -13, -32
    },
    { // King
       -65,  23,  16, -15, -56, -34,   2,  13,
        29,  -1, -20,  -7,  -8,  -4, -38, -29,
        -9,  24,   2, -16, -20,   6,  22, -22,
       -17, -20, -12, -27, -30, -25, -14, -36,
       -49,  -1, -27, -39, -46, -44, -33, -51,
       -14, -14, -22, -46, -44, -30, -15, -27,
         1,   7,  -8, -64, -43, -16,   9,   8,
       -15,  36,  12, -54,   8, -28,  24,  14
    }
};

const int PSQT_EG[PIECE_TYPE_NB][64] = {
    { // Pawn
         0,   0,   0,   0,   0,   0,   0,   0,
       178, 173, 158, 134, 147, 132, 165, 187,
        94, 100,  85,  67,  56,  53,  82,  84,
        32,  24,  13,   5,  -2,   4,  17,  17,
        13,   9,  -3,  -7,  -7,  -8,   3,  -1,
         4,   7,  -6,   1,   0,  -5,  -1,  -8,
        13,   8,   8,  10,  13,   0,   2,  -7,
         0,   0,   0,   0,   0,   0,   0,   0
    },
    { // Knight
       -58, -38, -13, -28, -31, -27, -63, -99,
       -25,  -8, -25,  -2,  -9, -25, -24, -52,
       -24, -20,  10,   9,  -1,  -9, -19, -41,
       -17,   3,  22,  22,  22,  11,   8, -18,
       -18,  -6,  16,  25,  16,  17,   4, -18,
       -23,  -3,  -1,  15,  10,  -3, -20, -22,
       -42, -20, -10,  -5,  -2, -20, -23, -44,
       -29, -51, -23, -15, -22, -18, -50, -64
    },
    { // Bishop
       -14, -21, -11,  -8, -7,  -9, -17, -24,
        -8,  -4,   7, -12, -3, -13,  -4, -14,
         2,  -8,   0,  -1, -2,   6,   0,   4,
        -3,   9,  12,   9, 14,  10,   3,   2,
        -6,   3,  13,  19,  7,  10,  -3,  -9,
       -12,  -3,   5,  10, 10,   5,  -6,  -7,
       -15, -10, -12, -10, -8,  -2, -16, -14,
       -21, -42, -11, -10, -6, -24, -22, -32
    },
    { // Rook
        13,  10,  18,  15,  12,  12,   8,   5,
        11,  13,  13,  11,  -3,   3,   8,   3,
         7,   7,   7,   5,   4,  -3,  -5,  -3,
         4,   3,  13,   1,   2,   1,  -1,   2,
         3,   5,   8,   4,  -5,  -6,  -8, -11,
        -4,   0,  -5,  -1,  -7, -12,  -8, -16,
        -6,  -6,   0,   2,  -9,  -9, -11,  -3,
        -9,   2,   3,  -1,  -5, -13,   4, -20
    },
    { // Queen
        -9,  22,  22,  27,  27,  19,  10,  20,
       -17,  20,  32,  41,  58,  25,  30,   0,
       -20,   6,   9,  49,  47,  35,  19,   9,
         3,  22,  24,  45,  57,  40,  57,  36,
       -18,  28,  19,  47,  31,  34,  39,  23,
       -16, -27,  15,   6,   9,  17,  10,   5,
       -22, -23, -30, -16, -16, -23, -36, -32,
       -33, -28, -22, -43,  -5, -32, -20, -41
    },
    { // King
       -74, -35, -18, -18, -11,  15,   4, -17,
       -12,  17,  14,  17,  17,  38,  23,  11,
        10,  17,  23,  15,  20,  45,  44,  13,
        -8,  22,  24,  27,  26,  33,  26,   3,
       -18,  -4,  21,  24,  27,  23,   9, -11,
       -19,  -3,  11,  21,  23,  16,   7,  -9,
       -27, -11,   4,  13,  14,   4,  -5, -17,
       -53, -34, -21, -11, -28, -14, -24, -43
    }
};

// Pawn Structure
const int PassedPawn[8] = { 0, 5, 10, 20, 35, 60, 100, 0 };
const int IsolatedPawn = -15;
const int DoubledPawn = -10;
const int BackwardPawn = -8;

// Mobility (MG, EG)
// PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING
const int MobilityBonus[PIECE_TYPE_NB][2] = {
    {0, 0}, {5, 5}, {6, 6}, {2, 4}, {1, 2}, {0, 0}
};

// King Safety
// Attacker weights: PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING
const int AttackerWeight[PIECE_TYPE_NB] = { 0, 2, 2, 3, 5, 0 };
// Safety table by attack units (SafetyTable[count] -> penalty)
const int SafetyTable[100] = {
    0,  0,  1,  2,  3,  5,  7,  9, 12, 15,
   18, 22, 26, 30, 35, 40, 45, 50, 55, 61,
   67, 73, 79, 86, 93,100,108,116,124,133,
   142,152,162,172,183,194,205,217,229,242,
   255,268,282,296,311,326,341,357,373,390
};

struct Term {
    int mg, eg;
    void add(int m, int e) { mg += m; eg += e; }
    void add(Term t) { mg += t.mg; eg += t.eg; }
    void sub(Term t) { mg -= t.mg; eg -= t.eg; }
};

int get_safety(const Position& pos, Color c, Bitboard us_pieces, Bitboard them_pieces) {
    Square king_sq = Bitboards::lsb(pos.pieces(c, KING));
    if (king_sq == SQ_NONE) return 0;
    
    // King zone: King square + neighbors + 2 squares forward
    Bitboard zone = Bitboards::king_attacks(king_sq);
    if (c == WHITE) zone |= (zone << 8);
    else zone |= (zone >> 8);
    
    int attack_units = 0;
    int attackers = 0;
    
    Bitboard enemy_rooks = pos.pieces(static_cast<Color>(c ^ 1), ROOK);
    Bitboard enemy_bishops = pos.pieces(static_cast<Color>(c ^ 1), BISHOP);
    Bitboard enemy_knights = pos.pieces(static_cast<Color>(c ^ 1), KNIGHT);
    Bitboard enemy_queens = pos.pieces(static_cast<Color>(c ^ 1), QUEEN);
    
    auto process_attackers = [&](Bitboard pieces, PieceType pt) {
        while(pieces) {
            Square s = Bitboards::pop_lsb(pieces);
            Bitboard att = 0;
            if (pt == KNIGHT) att = Bitboards::knight_attacks(s);
            else if (pt == BISHOP) att = Bitboards::bishop_attacks(s, us_pieces | them_pieces);
            else if (pt == ROOK) att = Bitboards::rook_attacks(s, us_pieces | them_pieces);
            else if (pt == QUEEN) att = Bitboards::queen_attacks(s, us_pieces | them_pieces);
            
            if (att & zone) {
                attackers++;
                attack_units += AttackerWeight[pt];
                if (att & Bitboards::king_attacks(king_sq)) attack_units += 1;
            }
        }
    };
    
    process_attackers(enemy_knights, KNIGHT);
    process_attackers(enemy_bishops, BISHOP);
    process_attackers(enemy_rooks, ROOK);
    process_attackers(enemy_queens, QUEEN);
    
    if (attackers < 2) return 0;
    
    if (attack_units > 49) attack_units = 49;
    return SafetyTable[attack_units];
}

int evaluate(const Position& pos) {
    Term score = {0, 0};
    int phase = 0;
    
    Color us = pos.side_to_move();
    Color them = static_cast<Color>(us ^ 1);
    
    Bitboard us_pieces = pos.pieces(us);
    Bitboard them_pieces = pos.pieces(them);
    Bitboard all_pieces = us_pieces | them_pieces;

    auto eval_side = [&](Color c) -> Term {
        Term t = {0, 0};
        
        // Loop through all pieces types
        for (int pt = 0; pt < PIECE_TYPE_NB; ++pt) {
             Bitboard bb = pos.pieces(c, static_cast<PieceType>(pt));
             while (bb) {
                 Square s = Bitboards::pop_lsb(bb);
                 
                 // Material
                 t.add(Material[pt], Material[pt]);
                 
                 // PSQT
                 int idx = (c == WHITE) ? s : (s ^ 56);
                 t.add(PSQT_MG[pt][idx], PSQT_EG[pt][idx]);
                 
                 // Phase
                 phase += PhaseInc[pt];
                 
                 // Pawn Structure
                 if (pt == PAWN) {
                     int r = s / 8;
                     int f = s % 8;
                     
                     // Passed Pawn
                     Bitboard forward_mask = 0;
                     if (c == WHITE) {
                         for(int yy=r+1; yy<8; ++yy) forward_mask |= (0x0101010101010101ULL & (0xFFULL << (yy*8)));
                     } else {
                         for(int yy=r-1; yy>=0; --yy) forward_mask |= (0x0101010101010101ULL & (0xFFULL << (yy*8)));
                     }
                     Bitboard file_mask = (0x0101010101010101ULL << f);
                     if (f > 0) file_mask |= (0x0101010101010101ULL << (f-1));
                     if (f < 7) file_mask |= (0x0101010101010101ULL << (f+1));
                     
                     Bitboard span = forward_mask & file_mask;
                     
                     if (!(span & pos.pieces(static_cast<Color>(c ^ 1), PAWN))) {
                         int rank = (c == WHITE) ? r : 7 - r;
                         t.add(0, PassedPawn[rank]);
                     }
                     
                     // Isolated
                     Bitboard adj_files = 0;
                     if (f > 0) adj_files |= (0x0101010101010101ULL << (f-1));
                     if (f < 7) adj_files |= (0x0101010101010101ULL << (f+1));
                     if (!(adj_files & pos.pieces(c, PAWN))) {
                         t.add(IsolatedPawn, IsolatedPawn);
                     }
                     
                     // Doubled
                     Bitboard file_bb = (0x0101010101010101ULL << f);
                     if ((file_bb & pos.pieces(c, PAWN)) & ~(1ULL << s)) {
                         t.add(DoubledPawn, DoubledPawn);
                     }
                 }
                 else if (pt != KING) { // Mobility for non-pawns/non-kings
                     Bitboard att = 0;
                     if (pt == KNIGHT) att = Bitboards::knight_attacks(s);
                     else if (pt == BISHOP) att = Bitboards::bishop_attacks(s, all_pieces);
                     else if (pt == ROOK) att = Bitboards::rook_attacks(s, all_pieces);
                     else if (pt == QUEEN) att = Bitboards::queen_attacks(s, all_pieces);
                     
                     int mob = Bitboards::count(att);
                     t.add(mob * MobilityBonus[pt][0], mob * MobilityBonus[pt][1]);
                 }
             }
        }
        return t;
    };
    
    Term us_score = eval_side(us);
    Term them_score = eval_side(them);
    
    score.mg = us_score.mg - them_score.mg;
    score.eg = us_score.eg - them_score.eg;
    
    // King Safety
    score.mg -= get_safety(pos, us, us_pieces, them_pieces);
    score.mg += get_safety(pos, them, them_pieces, us_pieces);
    
    // Interpolation
    int mg_phase = phase;
    if (mg_phase > 24) mg_phase = 24;
    int eg_phase = 24 - mg_phase;
    
    int final = (score.mg * mg_phase + score.eg * eg_phase) / 24;
    return final;
}

} // namespace Eval