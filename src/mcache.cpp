#include "mcache.h"
#include <cstdlib>
#include <iostream>
#include <cstring>

#if defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace MCache {

void* aligned_alloc(size_t size, size_t alignment) {
    void* ptr = nullptr;
#if defined(__linux__) || defined(__APPLE__)
    if (posix_memalign(&ptr, alignment, size) != 0) {
        return nullptr;
    }
    // Advise kernel to use huge pages if possible (Linux specific optimization)
    #if defined(MADV_HUGEPAGE)
    madvise(ptr, size, MADV_HUGEPAGE);
    #endif
#else
    ptr = _aligned_malloc(size, alignment);
#endif
    if (ptr) std::memset(ptr, 0, size);
    return ptr;
}

void aligned_free(void* ptr) {
#if defined(__linux__) || defined(__APPLE__)
    free(ptr);
#else
    _aligned_free(ptr);
#endif
}

} // namespace MCache
