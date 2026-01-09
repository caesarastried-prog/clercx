#include "uci.h"
#include "bitboard.h"
#include "zobrist.h"
#include "tune.h"

int main() {
    Bitboards::init();
    Zobrist::init();
    Tune::init();
    
    UCI::loop();
    
    return 0;
}