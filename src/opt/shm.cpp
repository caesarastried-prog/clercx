#include "opt/shm.h"
#include <cstdlib>
#include <sys/mman.h>
#include <unistd.h>
#include <iostream>

// Platform specific includes for madvise usually
#ifndef MADV_HUGEPAGE
#define MADV_HUGEPAGE 14
#endif

namespace Opt {

void* aligned_large_alloc(size_t size) {
    void* ptr = nullptr;
    // Align to 2MB (typical huge page size)
    if (posix_memalign(&ptr, 2 * 1024 * 1024, size) != 0) {
        return nullptr;
    }
    
    // Advise kernel to use huge pages
    madvise(ptr, size, MADV_HUGEPAGE);
    
    return ptr;
}

void aligned_large_free(void* ptr) {
    free(ptr);
}

}
