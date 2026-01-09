#include "bitboard.h"
#include <iomanip>

namespace Bitboards {

Bitboard FileABB = 0x0101010101010101ULL;
Bitboard Rank1BB = 0x00000000000000FFULL;

Bitboard KnightAttacks[SQ_NB];
Bitboard KingAttacks[SQ_NB];
Bitboard PawnAttacks[COLOR_NB][SQ_NB];

struct Magic {
    Bitboard mask;
    Bitboard magic;
    Bitboard* attacks;
    int shift;
};

Magic RookMagics[SQ_NB];
Magic BishopMagics[SQ_NB];

Bitboard RookTable[0x19000];
Bitboard BishopTable[0x1480];

const Direction RookDirs[4] = {NORTH, SOUTH, EAST, WEST};
const Direction BishopDirs[4] = {NORTH_EAST, NORTH_WEST, SOUTH_EAST, SOUTH_WEST};

Bitboard sliding_attack(Square sq, Bitboard occupied, const Direction* dirs, int dir_count) {
    Bitboard attacks = 0;
    for (int i = 0; i < dir_count; ++i) {
        Direction d = dirs[i];
        for (int s = sq; ; ) {
            int r = s / 8, f = s % 8;
            if (d == NORTH && r == 7) break;
            if (d == SOUTH && r == 0) break;
            if (d == EAST && f == 7) break;
            if (d == WEST && f == 0) break;
            if (d == NORTH_EAST && (r == 7 || f == 7)) break;
            if (d == NORTH_WEST && (r == 7 || f == 0)) break;
            if (d == SOUTH_EAST && (r == 0 || f == 7)) break;
            if (d == SOUTH_WEST && (r == 0 || f == 0)) break;
            
            s += d;
            attacks |= square_bb(static_cast<Square>(s));
            if (occupied & square_bb(static_cast<Square>(s))) break;
        }
    }
    return attacks;
}

uint64_t RookMagicsVal[64] = {
    0xa8802c46b0005000ULL, 0x180208003000c02ULL, 0x4200400001200802ULL, 0x40108820101002ULL,
    0x8010050410002100ULL, 0x80040041144020ULL, 0x8001004040008200ULL, 0x80210812040000ULL,
    0x10020104102040c2ULL, 0x44080800400201ULL, 0x4000410010404001ULL, 0x20010080100040ULL,
    0x4020080040080ULL, 0x10008020400081ULL, 0x1102000402028ULL, 0x4210081001000ULL,
    0x4121082104124401ULL, 0x2000100040200804ULL, 0x1001000402002ULL, 0x12080020040010ULL,
    0x20010100040ULL, 0x102000402008ULL, 0x2000402104040ULL, 0x21004000802ULL,
    0x880010020110200ULL, 0x1000404010020ULL, 0x1100040200210ULL, 0x101088100020004ULL,
    0x1081000200041ULL, 0x4100020100040ULL, 0x20010040100ULL, 0x200802001001ULL,
    0x210408100020ULL, 0x210008100401ULL, 0x20020101000ULL, 0x1022020202ULL,
    0x1088444001011ULL, 0x102004040400ULL, 0x10200200402ULL, 0x802000402ULL,
    0x82010208420ULL, 0x10104041020ULL, 0x80100401ULL, 0x400020020ULL,
    0x100040200ULL, 0x2010040802ULL, 0x1021001002ULL, 0x2012080ULL,
    0x10021001081ULL, 0x202002004008ULL, 0x2110004ULL, 0x1010402ULL,
    0x2011084ULL, 0x1104ULL, 0x210ULL, 0x420ULL,
    0x8110a00ULL, 0x10010001001040ULL, 0x12241ULL, 0x2044ULL,
    0x200400ULL, 0x208000ULL, 0x8221ULL, 0x10222ULL
};

uint64_t BishopMagicsVal[64] = {
    0x40040844404084ULL, 0x2004208a004208ULL, 0x10190041080202ULL, 0x1080608450410ULL,
    0x581104180800210ULL, 0x2112080446200010ULL, 0x1080820820060210ULL, 0x3c0808410220200ULL,
    0x4050410a004020ULL, 0x1001040010142ULL, 0x2010201210a02ULL, 0x440400410100ULL,
    0x1010440a0208200ULL, 0x801020200041ULL, 0x4010081021001ULL, 0x101040201004202ULL,
    0x10104104040402ULL, 0x10120220020040ULL, 0x80101000401ULL, 0x10080204100ULL,
    0x102020102420004ULL, 0x10022021040ULL, 0x8022141201ULL, 0x1102241004040ULL,
    0x8101001040ULL, 0x40021001042ULL, 0x44120201101ULL, 0x1102012ULL,
    0x1012102ULL, 0x102008020ULL, 0x1100104ULL, 0x1100202ULL,
    0x221040102100ULL, 0x102020004ULL, 0x202011ULL, 0x200201ULL,
    0x1042004ULL, 0x21010042ULL, 0x41108220ULL, 0x2011041ULL,
    0x10008040ULL, 0x10201010ULL, 0x4010ULL, 0x1004042ULL,
    0x11210ULL, 0x11120ULL, 0x1011ULL, 0x40004ULL,
    0x100200ULL, 0x1021ULL, 0x400ULL, 0x801ULL,
    0x41ULL, 0x1040ULL, 0x41ULL, 0x201ULL,
    0x20ULL, 0x40ULL, 0x101ULL, 0x202ULL,
    0x41ULL, 0x20ULL, 0x20ULL, 0x801ULL
};

void init() {
    for (int s = 0; s < SQ_NB; ++s) {
        Bitboard b = square_bb(static_cast<Square>(s));
        
        PawnAttacks[WHITE][s] = 0;
        if (b & ~FileABB) PawnAttacks[WHITE][s] |= (b << 7);
        if (b & ~(FileABB << 7)) PawnAttacks[WHITE][s] |= (b << 9);
        
        PawnAttacks[BLACK][s] = 0;
        if (b & ~FileABB) PawnAttacks[BLACK][s] |= (b >> 9);
        if (b & ~(FileABB << 7)) PawnAttacks[BLACK][s] |= (b >> 7);

        Bitboard knight = 0;
        if (b & ~(FileABB | (FileABB << 1))) knight |= (b << 6) | (b >> 10);
        if (b & ~FileABB) knight |= (b << 15) | (b >> 17);
        if (b & ~(FileABB << 7)) knight |= (b << 17) | (b >> 15);
        if (b & ~((FileABB << 7) | (FileABB << 6))) knight |= (b << 10) | (b >> 6);
        KnightAttacks[s] = knight;
        
        Bitboard king = (b << 1) | (b >> 1);
        Bitboard king_v = b | king;
        king |= (king_v << 8) | (king_v >> 8);
        king &= ~b;
        if (b & FileABB) king &= ~(FileABB << 7);
        if (b & (FileABB << 7)) king &= ~FileABB;
        KingAttacks[s] = king;
    }

    Bitboard* rook_ptr = RookTable;
    Bitboard* bishop_ptr = BishopTable;

    for (int s = 0; s < 64; ++s) {
        Square sq = static_cast<Square>(s);
        
        Bitboard mask = sliding_attack(sq, 0, RookDirs, 4);
        if (sq / 8 != 0) mask &= ~(Rank1BB);
        if (sq / 8 != 7) mask &= ~(Rank1BB << 56);
        if (sq % 8 != 0) mask &= ~FileABB;
        if (sq % 8 != 7) mask &= ~(FileABB << 7);
        
        RookMagics[s].mask = mask;
        RookMagics[s].magic = RookMagicsVal[s];
        RookMagics[s].attacks = rook_ptr;
        RookMagics[s].shift = 64 - count(mask);
        
        int variations = 1 << count(mask);
        for (int i = 0; i < variations; ++i) {
            Bitboard occupied = 0;
            Bitboard temp_mask = mask;
            for (int j = 0; j < count(mask); ++j) {
                Square l = pop_lsb(temp_mask);
                if (i & (1 << j)) occupied |= square_bb(l);
            }
            rook_ptr[(occupied * RookMagics[s].magic) >> RookMagics[s].shift] = sliding_attack(sq, occupied, RookDirs, 4);
        }
        rook_ptr += variations;

        mask = sliding_attack(sq, 0, BishopDirs, 4);
        mask &= ~(Rank1BB | (Rank1BB << 56) | FileABB | (FileABB << 7));
        
        BishopMagics[s].mask = mask;
        BishopMagics[s].magic = BishopMagicsVal[s];
        BishopMagics[s].attacks = bishop_ptr;
        BishopMagics[s].shift = 64 - count(mask);
        
        variations = 1 << count(mask);
        for (int i = 0; i < variations; ++i) {
            Bitboard occupied = 0;
            Bitboard temp_mask = mask;
            for (int j = 0; j < count(mask); ++j) {
                Square l = pop_lsb(temp_mask);
                if (i & (1 << j)) occupied |= square_bb(l);
            }
            bishop_ptr[(occupied * BishopMagics[s].magic) >> BishopMagics[s].shift] = sliding_attack(sq, occupied, BishopDirs, 4);
        }
        bishop_ptr += variations;
    }
}

Bitboard knight_attacks(Square s) { return KnightAttacks[s]; }
Bitboard king_attacks(Square s) { return KingAttacks[s]; }

Bitboard bishop_attacks(Square s, Bitboard occupied) {
    Magic m = BishopMagics[s];
    return m.attacks[((occupied & m.mask) * m.magic) >> m.shift];
}

Bitboard rook_attacks(Square s, Bitboard occupied) {
    Magic m = RookMagics[s];
    return m.attacks[((occupied & m.mask) * m.magic) >> m.shift];
}

Bitboard queen_attacks(Square s, Bitboard occupied) {
    return rook_attacks(s, occupied) | bishop_attacks(s, occupied);
}

Bitboard pawn_attacks(Square s, Color c) { return PawnAttacks[c][s]; }

void print(Bitboard bb) {
    std::cout << "+---+---+---+---+---+---+---+---+" << std::endl;
    for (int r = 7; r >= 0; --r) {
        for (int f = 0; f <= 7; ++f) {
            Square s = static_cast<Square>(r * 8 + f);
            std::cout << "| " << ((bb & square_bb(s)) ? "X " : ". ");
        }
        std::cout << "|" << std::endl;
        std::cout << "+---+---+---+---+---+---+---+---+" << std::endl;
    }
    std::cout << "Bitboard: 0x" << std::hex << std::setw(16) << std::setfill('0') << bb << std::dec << std::endl;
}

} // namespace Bitboards