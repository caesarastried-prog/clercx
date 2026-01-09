#ifndef TT_H
#define TT_H

#include "types.h"
#include "move.h"
#include <cstddef>

enum TTFlag {
    EXACT, ALPHA, BETA
};

struct TTEntry {
    uint64_t key;
    Move move;
    int16_t score;
    int8_t depth;
    int8_t flag;
    uint8_t age; 
    uint8_t pad; // Pad to 16 bytes
};

class TranspositionTable {
public:
    TranspositionTable(size_t size_mb);
    ~TranspositionTable();
    
    void resize(size_t size_mb);
    
    void store(uint64_t key, Move m, int score, int depth, TTFlag flag, int ply);
    bool probe(uint64_t key, TTEntry& entry);
    void prefetch(uint64_t key);
    void clear();
    void new_search(); 

private:
    TTEntry* table;
    size_t entry_count;
    uint8_t generation;
};

extern TranspositionTable TT;

#endif // TT_H