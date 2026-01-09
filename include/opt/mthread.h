#ifndef MTHREAD_H
#define MTHREAD_H

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <functional>

namespace Opt {

// Spinlock for low-latency locking (e.g. in TT)
class SpinLock {
    std::atomic_flag locked = ATOMIC_FLAG_INIT;
public:
    void lock() {
        while (locked.test_and_set(std::memory_order_acquire)) {
            #if defined(__cpp_lib_atomic_wait)
                locked.wait(true, std::memory_order_relaxed);
            #endif
        }
    }
    void unlock() {
        locked.clear(std::memory_order_release);
        #if defined(__cpp_lib_atomic_wait)
            locked.notify_one();
        #endif
    }
};

class Thread {
public:
    std::thread std_thread;
    int id;
    std::atomic<bool> searching;
    
    Thread(int id) : id(id), searching(false) {}
    virtual ~Thread() { if(std_thread.joinable()) std_thread.join(); }
    
    // Bind thread to specific core
    void bind();
};

class ThreadPool {
    std::vector<std::unique_ptr<Thread>> threads;
    std::atomic<bool> stop_flag;
    
public:
    ThreadPool();
    ~ThreadPool();
    
    void init(int num_threads);
    void start_search(std::function<void(int)> search_func);
    void wait_for_completion();
    void stop();
    
    int size() const { return threads.size(); }
};

}

#endif // MTHREAD_H