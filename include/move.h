#ifndef MOVE_H
#define MOVE_H

#include "types.h"
#include <string>

enum MoveType {
    NORMAL, PROMOTION, EN_PASSANT, CASTLING
};

class Move {
public:
    Move() : data(0) {}
    Move(Square from, Square to, MoveType type = NORMAL, PieceType promo = KNIGHT) {
        data = (from) | (to << 6) | (type << 12) | ((promo - KNIGHT) << 14);
    }
    
    Square from() const { return static_cast<Square>(data & 0x3F); }
    Square to() const { return static_cast<Square>((data >> 6) & 0x3F); }
    MoveType type() const { return static_cast<MoveType>((data >> 12) & 0x3); }
    PieceType promotion_piece() const { return static_cast<PieceType>(((data >> 14) & 0x3) + KNIGHT); }
    
    bool operator==(const Move& other) const { return data == other.data; }
    bool operator!=(const Move& other) const { return data != other.data; }
    
    uint16_t raw() const { return data; }

    static Move none() { return Move(); }

    std::string to_string() const {
        if (*this == none()) return "0000";
        Square f = from();
        Square t = to();
        std::string s = "";
        s += (char)('a' + (f % 8));
        s += (char)('1' + (f / 8));
        s += (char)('a' + (t % 8));
        s += (char)('1' + (t / 8));
        
        if (type() == PROMOTION) {
            char p = ' ';
            switch (promotion_piece()) {
                case KNIGHT: p = 'n'; break;
                case BISHOP: p = 'b'; break;
                case ROOK:   p = 'r'; break;
                case QUEEN:  p = 'q'; break;
                default: break;
            }
            if (p != ' ') s += p;
        }
        return s;
    }

private:
    uint16_t data;
};

#endif // MOVE_H