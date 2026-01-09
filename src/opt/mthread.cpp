#include "opt/mthread.h"
#include <iostream>

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#endif

namespace Opt {

void Thread::bind() {
#if defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(id % std::thread::hardware_concurrency(), &cpuset);
    pthread_setaffinity_np(std_thread.native_handle(), sizeof(cpu_set_t), &cpuset);
#endif
}

ThreadPool::ThreadPool() : stop_flag(false) {}

ThreadPool::~ThreadPool() {
    stop();
}

void ThreadPool::init(int num_threads) {
    stop();
    threads.clear();
    for (int i = 0; i < num_threads; ++i) {
        threads.push_back(std::make_unique<Thread>(i));
    }
}

void ThreadPool::start_search(std::function<void(int)> search_func) {
    // Stop any existing search threads first (sanity check)
    for(auto& t : threads) {
        if(t->std_thread.joinable()) t->std_thread.join();
    }

    for (auto& t : threads) {
        // Capture raw pointer to Thread object and function
        Thread* t_ptr = t.get();
        t_ptr->std_thread = std::thread([t_ptr, search_func]() {
            t_ptr->bind();
            search_func(t_ptr->id);
        });
    }
}

void ThreadPool::wait_for_completion() {
    stop();
}

void ThreadPool::stop() {
    stop_flag = true;
    for(auto& t : threads) {
        if(t->std_thread.joinable()) t->std_thread.join();
    }
}

}