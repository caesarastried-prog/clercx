#ifndef MCACHE_H
#define MCACHE_H

#include <cstddef>

namespace MCache {

void* aligned_alloc(size_t size, size_t alignment);
void aligned_free(void* ptr);

// Prefetching
inline void prefetch(void* addr) {
#if defined(__GNUC__) || defined(__clang__)
    __builtin_prefetch(addr);
#endif
}

} // namespace MCache

#endif // MCACHE_H
