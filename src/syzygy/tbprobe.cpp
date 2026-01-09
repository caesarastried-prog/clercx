#include "syzygy/tbprobe.h"
#include "movegen.h"
#include "opt/shm.h"
#include "opt/mthread.h"
#include "zobrist.h"
#include "misc.h"

#include <iostream>
#include <vector>
#include <string>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <algorithm>
#include <cstring>
#include <thread>
#include <map>

// Compiler specific optimizations
#if defined(__GNUC__) || defined(__clang__)
    #define PREFETCH(ptr) __builtin_prefetch(ptr)
    #define LIKELY(x) __builtin_expect(!!(x), 1)
    #define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define PREFETCH(ptr)
    #define LIKELY(x) (x)
    #define UNLIKELY(x) (x)
#endif

namespace Syzygy {

namespace {

// Configuration
constexpr int MAX_TB_PIECES = 6; // Supports up to 6-man TBs usually
constexpr size_t CACHE_SIZE_MB = 128; // Default cache size
constexpr size_t CACHE_LINE_SIZE = 64;

// TB Return Values
constexpr int WDL_WIN = 2;
constexpr int WDL_LOSS = -2;
constexpr int WDL_DRAW = 1;
constexpr int WDL_BLESSED_LOSS = -1; // 50-move rule interference
constexpr int WDL_CURSED_WIN = 3;    // 50-move rule interference

// Internal Stats
std::atomic<uint64_t> tb_hits{0};
std::atomic<uint64_t> tb_probes{0};

// Cache Entry Structure - Aligned to 16 bytes
struct alignas(16) TBCacheEntry {
    uint64_t key;       // Position Key (Zobrist)
    uint16_t move;      // Best Move (if any)
    int8_t score;       // WDL/DTZ Score
    uint8_t generation; // For LRU/Age
    uint8_t quality;    // Depth/Type info

    // Check if entry matches key
    bool verify(uint64_t k) const { return key == k; }
};

// Global Cache
struct TBCache {
    TBCacheEntry* entries;
    size_t size;
    size_t mask;
    uint8_t generation;

    TBCache() : entries(nullptr), size(0), mask(0), generation(0) {}

    void init(size_t size_mb) {
        if (entries) Opt::aligned_large_free(entries);
        size_t num_entries = (size_mb * 1024 * 1024) / sizeof(TBCacheEntry);
        // Ensure power of 2
        size = 1;
        while (size < num_entries) size <<= 1;
        mask = size - 1;

        entries = static_cast<TBCacheEntry*>(Opt::aligned_large_alloc(size * sizeof(TBCacheEntry)));
        std::memset(entries, 0, size * sizeof(TBCacheEntry));
        generation = 1;
    }

    ~TBCache() {
        if (entries) Opt::aligned_large_free(entries);
    }

    bool probe(uint64_t key, int& score, Move& best_move) {
        size_t idx = key & mask;
        const TBCacheEntry& entry = entries[idx];

        // Prefetch likely next probe or surrounding lines?
        // Actually prefetch the entry itself before calling probe if possible, 
        // but here we are already accessing it.
        
        if (entry.verify(key)) {
            // Atomic read not strictly needed if we accept rare races, 
            // but for correctness in S+++ engine:
            uint64_t k = std::atomic_load_explicit((const std::atomic<uint64_t>*)&entry.key, std::memory_order_relaxed);
            if (k == key) {
                score = entry.score;
                best_move = Move(static_cast<Square>(entry.move & 0x3F), static_cast<Square>((entry.move >> 6) & 0x3F));
                // Basic move reconstruction, assuming simple move encoding in cache
                // Real impl would store full raw move or reconstruct
                tb_hits.fetch_add(1, std::memory_order_relaxed);
                return true;
            }
        }
        return false;
    }

    void store(uint64_t key, int score, Move m) {
        size_t idx = key & mask;
        TBCacheEntry& entry = entries[idx];
        
        // Simple always-replace or age-based
        // Lockless store: Just write. Races might corrupt an entry, 
        // but verify() checks key. 
        // We assume 64-bit atomic writes for key.
        
        TBCacheEntry new_entry;
        new_entry.key = key;
        new_entry.score = static_cast<int8_t>(score);
        new_entry.move = static_cast<uint16_t>(m.from()) | (static_cast<uint16_t>(m.to()) << 6);
        new_entry.generation = generation;
        
        // Store
        // Use memcpy to avoid tearing if possible, or reliance on aligned atomic 128-bit ops (cmpxchg16b)
        // For simplicity and speed in C++, we write fields. Key last ensures validity.
        entry.score = new_entry.score;
        entry.move = new_entry.move;
        entry.generation = new_entry.generation;
        std::atomic_store_explicit((std::atomic<uint64_t>*)&entry.key, key, std::memory_order_release);
    }
} tb_cache;

// Placeholder for Fathom context
struct TBHandle {
    // This would hold the pointer to loaded TBs (e.g. tb_t*)
    void* impl = nullptr;
    int max_pieces = 0;
};

TBHandle tb_ctx;

} // namespace

// ----------------------------------------------------------------------------
// Public Interface Implementation
// ----------------------------------------------------------------------------

void init(const std::string& path) {
    if (path.empty()) return;

    // 1. Initialize Cache
    tb_cache.init(CACHE_SIZE_MB);
    
    // 2. Scan for TB files (Simulation of Fathom init)
    // In a real integration, here we call: tb_init(path.c_str());
    // For this standalone file, we look for .rtbw and .rtbz files
    int wdl_count = 0;
    int dtz_count = 0;
    
    // Simple directory scan
    if (std::filesystem::exists(path)) {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            std::string ext = entry.path().extension().string();
            if (ext == ".rtbw") wdl_count++;
            else if (ext == ".rtbz") dtz_count++;
        }
    }

    if (wdl_count > 0) {
        tb_ctx.max_pieces = MAX_TB_PIECES; // Assume we found up to N pieces
        std::cout << "info string Syzygy: Found " << wdl_count << " WDL and " << dtz_count << " DTZ tables." << std::endl;
        std::cout << "info string Syzygy: Cache initialized " << CACHE_SIZE_MB << "MB" << std::endl;
    }
    
    // Reset stats
    tb_hits = 0;
    tb_probes = 0;
}

// Optimized population count for material
inline int popcount(Bitboard b) {
    return __builtin_popcountll(b);
}

// Convert Engine Result to WDL
int wdl_to_value(int wdl, int ply) {
    // Convert -2..2 to Mate Scores
    if (wdl == WDL_WIN) return 30000 - ply;      // Mate in 'ply' (approx)
    if (wdl == WDL_LOSS) return -30000 + ply;    // Mated
    if (wdl == WDL_DRAW) return 0;
    return 0; // Cursed/Blessed treated as draw for safety or small eval
}

bool probe_wdl(const Position& pos, int& score) {
    if (tb_ctx.max_pieces == 0) return false;

    // 1. Early Exit: 50-move rule, Castling rights
    // Syzygy TBs do not contain castling rights.
    if (pos.state_ptr()->castle_rights != 0) return false;
    
    // 2. Material Check
    int piece_count = popcount(pos.all_pieces());
    if (piece_count > tb_ctx.max_pieces) return false;

    // 3. Cache Probe
    tb_probes.fetch_add(1, std::memory_order_relaxed);
    Move dummy_move;
    int cached_score = 0;
    if (tb_cache.probe(pos.hash(), cached_score, dummy_move)) {
        score = wdl_to_value(cached_score, 0); // Ply 0 for static probe
        return true;
    }

    // 4. Actual TB Probe (Placeholder for Fathom call)
    // int wdl = tb_probe_wdl_impl(pos...);
    // For now, we return false if no actual library is linked to do the decoding.
    // However, if we assume this code IS the implementation, we would decode here.
    // Given the constraints, we will simulate a "Miss" or return false to fallback to search
    // unless this is a mockup.
    
    // To be safe and functional without Fathom code:
    return false; 
    
    /* 
       Optimized Integration Note:
       If Fathom was present, we would:
       int wdl = tb_probe_wdl(pos.white_pieces, pos.black_pieces, ...);
       tb_cache.store(pos.hash(), wdl, Move::none());
       score = wdl_to_value(wdl, 0);
       return true;
    */
}

// Helper to check if a move captures
bool is_capture(const Position& pos, Move m) {
    return pos.piece_on(m.to()) != NO_PIECE || m.type() == EN_PASSANT;
}

bool probe_root(const Position& pos, Move& best_move, int& score) {
    if (tb_ctx.max_pieces == 0) return false;
    
    // Castling check
    if (pos.state_ptr()->castle_rights != 0) return false;

    // Material check
    if (popcount(pos.all_pieces()) > tb_ctx.max_pieces) return false;

    // Generate all legal moves
    MoveGen::MoveList moves;
    MoveGen::generate<MoveGen::ALL>(pos, moves);

    int best_val = -32000;
    Move best_m = Move::none();
    bool found_win = false;

    // Lockless approach:
    // Root probing typically doesn't need extreme lockless optimization as it happens once per search root.
    // However, inside the search (not root), use WDL probing.

    // "DTZ" Probing Logic (Simplified)
    for (int i = 0; i < moves.size(); ++i) {
        Move m = moves[i];
        if (!pos.is_legal(m)) continue;

        StateInfo st;
        Position next_pos = pos; // Copy position (expensive) or make/unmake
        // Use make_move on copy to avoid extensive unmake logic if not needed, 
        // but make/unmake is faster.
        // Assuming Position copy is slow, let's use make_move on a scratch pos?
        // Position is large. Better to use make_move/unmake_move on 'const Position' -> Need mutable.
        // The signature is 'const Position& pos'. We must clone or cast.
        // Actually, probe_root usually called at root where we can clone.
        
        Position p_child = pos;
        p_child.make_move(m, st);
        
        // Check for immediate checkmate or rule of 50
        if (p_child.is_draw()) {
            if (best_val < 0) { best_val = 0; best_m = m; }
            continue;
        }

        // Probe WDL of child
        // If child is loss for opponent -> win for us.
        int wdl_score = 0;
        // Inverting logic: result is for side to move.
        // We want min(opp_score).
        
        // This requires DTZ (Distance To Zero) to rank moves, 
        // but basic WDL can filter WIN vs DRAW vs LOSS.
        
        // Mock probe
        // bool res = probe_wdl(p_child, wdl_score); 
        // if (!res) continue; // Fallback to search
        
        // ... Logic to pick best DTZ ...
    }
    
    // If successful:
    // score = ...
    // best_move = ...
    // return true;

    return false;
}

} // namespace Syzygy