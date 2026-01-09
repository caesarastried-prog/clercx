#include "mcache.h"
#include <cstdlib>
#include <cstring>
#include <iostream>

#if defined(__linux__) || defined(__APPLE__)
#include <sys/mman.h>
#include <unistd.h>
#endif

#if defined(_MSC_VER)
#include <malloc.h>
#endif

namespace MCache {

void* aligned_alloc(size_t size, size_t alignment) {
    void* ptr = nullptr;
#if defined(__linux__) || defined(__APPLE__)
    if (posix_memalign(&ptr, alignment, size) != 0) {
        return nullptr;
    }
    // Optimization: Suggest huge pages for large allocations (e.g. TT, generic tables)
    #if defined(MADV_HUGEPAGE)
    if (size >= 2 * 1024 * 1024) {
        madvise(ptr, size, MADV_HUGEPAGE);
    }
    #endif
#elif defined(_MSC_VER)
    ptr = _aligned_malloc(size, alignment);
#else
    ptr = std::aligned_alloc(alignment, size);
#endif
    if (ptr) std::memset(ptr, 0, size);
    return ptr;
}

void aligned_free(void* ptr) {
    if (!ptr) return;
#if defined(_MSC_VER)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

} // namespace MCache