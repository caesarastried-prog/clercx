#ifndef MCACHE_H
#define MCACHE_H

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <atomic>

namespace MCache {

// --- Memory Management ---

void* aligned_alloc(size_t size, size_t alignment);
void aligned_free(void* ptr);

// --- Prefetching ---

enum PrefetchHint {
    READ = 0,
    WRITE = 1
};

enum Locality {
    NONE = 0,
    LOW = 1,
    MODERATE = 2,
    HIGH = 3
};

template <int HINT = READ, int LOC = HIGH>
inline void prefetch(void* addr) {
#if defined(__GNUC__) || defined(__clang__)
    __builtin_prefetch(addr, HINT, LOC);
#elif defined(_MSC_VER)
    _mm_prefetch((char*)addr, LOC == NONE ? _MM_HINT_NTA : _MM_HINT_T0);
#endif
}

// --- Generic Lockless Cache Table ---
// Suitable for Pawn Table, Material Table, Evaluation Cache.
// Uses a simple "always replace" or "depth-preferred" strategy depending on Policy.

template <typename EntryType, size_t Size>
class CacheTable {
public:
    CacheTable() : table_(nullptr) {
        // Size must be power of 2
        static_assert((Size & (Size - 1)) == 0, "CacheTable Size must be power of 2");
        size_t bytes = Size * sizeof(EntryType);
        table_ = static_cast<EntryType*>(aligned_alloc(bytes, 64)); // Cache-line aligned
    }

    ~CacheTable() {
        if (table_) aligned_free(table_);
    }

    EntryType* operator[](uint64_t key) {
        return &table_[key & (Size - 1)];
    }
    
    const EntryType* operator[](uint64_t key) const {
        return &table_[key & (Size - 1)];
    }

    void clear() {
        if (table_) {
            size_t bytes = Size * sizeof(EntryType);
            for (size_t i = 0; i < bytes; ++i) ((char*)table_)[i] = 0;
        }
    }

    void prefetch_entry(uint64_t key) {
        prefetch(&table_[key & (Size - 1)]);
    }

private:
    EntryType* table_;
};

} // namespace MCache

#endif // MCACHE_H