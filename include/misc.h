#ifndef MISC_H
#define MISC_H

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

namespace Misc {

    // High-resolution timer
    inline uint64_t now() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
    }

    // Pseudo-random number generator (Xorshift)
    class PRNG {
        uint64_t s;
    public:
        PRNG(uint64_t seed) : s(seed) {}
        template<typename T> T rand() { return T(rand64()); }
        uint64_t rand64() {
            s ^= s >> 12; s ^= s << 25; s ^= s >> 27; 
            return s * 2685821657736338717LL;
        }
    };

    // Logging (thread-safe ideally, but simple for now)
    void log(const std::string& msg);
}

#endif // MISC_H
