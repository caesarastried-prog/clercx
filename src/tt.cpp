#include "tt.h"
#include "mcache.h"
#include <cstring>
#include <iostream>
#include <new>

TranspositionTable TT(16); // Default 16MB

TranspositionTable::TranspositionTable(size_t size_mb) : generation(0), table(nullptr) {
    resize(size_mb);
}

TranspositionTable::~TranspositionTable() {
    if (table) MCache::aligned_free(table);
}

void TranspositionTable::resize(size_t size_mb) {
    if (table) MCache::aligned_free(table);
    
    // Ensure size is power of 2 for fast modulo/masking
    // But we use modulo currently: key % entry_count.
    // Ideally we'd use power of 2 size.
    
    size_t size_bytes = size_mb * 1024 * 1024;
    entry_count = size_bytes / sizeof(TTEntry);
    
    // Allocate 2MB aligned memory
    table = static_cast<TTEntry*>(MCache::aligned_alloc(entry_count * sizeof(TTEntry), 2 * 1024 * 1024));
    
    if (!table) {
        std::cerr << "Failed to allocate TT with huge pages, falling back to standard alignment" << std::endl;
        table = static_cast<TTEntry*>(MCache::aligned_alloc(entry_count * sizeof(TTEntry), 4096));
    }
    
    clear();
}

void TranspositionTable::clear() {
    if (table) std::memset(static_cast<void*>(table), 0, entry_count * sizeof(TTEntry));
    generation = 0;
}

void TranspositionTable::new_search() {
    generation++;
}

void TranspositionTable::prefetch(uint64_t key) {
    if (table) MCache::prefetch(&table[key % entry_count]);
}

void TranspositionTable::store(uint64_t key, Move m, int score, int depth, TTFlag flag, int ply) {
    if (!table) return;

    if (score > 29000) score += ply;
    else if (score < -29000) score -= ply;

    size_t idx = key % entry_count;
    TTEntry& entry = table[idx];
    
    // Replacement strategy:
    // 1. Different key (Always replace, it's a collision)
    // 2. Exact match from this search (Update better info)
    // 3. Deeper depth (Always replace)
    
    bool replace = false;
    if (entry.key != key) replace = true;
    else if (depth >= entry.depth) replace = true;
    else if (entry.age != generation) replace = true; // Old entry
    
    if (replace) {
        // Save move if we are overwriting a valuable entry with a worse one?
        // No, in standard simple TT, just overwrite.
        
        // Preserve move if new one is NONE but old one was valid?
        if (m == Move::none()) m = entry.move;

        entry.key = key;
        entry.move = m;
        entry.score = static_cast<int16_t>(score);
        entry.depth = static_cast<int8_t>(depth);
        entry.flag = static_cast<int8_t>(flag);
        entry.age = generation;
    }
}

bool TranspositionTable::probe(uint64_t key, TTEntry& entry) {
    if (!table) return false;
    
    size_t idx = key % entry_count;
    const TTEntry& tte = table[idx];
    
    if (tte.key == key) {
        entry = tte;
        return true;
    }
    return false;
}