#include "misc.h"
#include <mutex>

namespace Misc {
    static std::mutex log_mutex;

    void log(const std::string& msg) {
        std::lock_guard<std::mutex> lock(log_mutex);
        std::cout << "info string " << msg << std::endl;
    }
}
