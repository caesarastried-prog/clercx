#ifndef SHM_H
#define SHM_H

#include <cstddef>

namespace Opt {
    // Allocates memory potentially using large pages/huge TLB for performance
    void* aligned_large_alloc(size_t size);
    void aligned_large_free(void* ptr);
}

#endif // SHM_H
