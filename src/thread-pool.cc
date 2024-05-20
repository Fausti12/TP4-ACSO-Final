/**
 * File: thread-pool.cc
 * --------------------
 * Presents the implementation of the ThreadPool class.
 */

#include "thread-pool.h"
using namespace std;

ThreadPool::ThreadPool(size_t numThreads) : wts(numThreads) {
    // Create the dispatcher thread
    dt = thread([this](){
        while (true) {
            // Wait for a task to be available
            task_sem.wait();
            // Acquire the lock
            lock_guard<mutex> lg(dt_mutex);
            // Check if the thread pool is shutting down (por las dudas)
            if (tasks_.empty()) break;
            // Get the task and remove it from the tasks vector (FIFO)
            auto task = tasks_.front();
            tasks_.erase(tasks_.begin());
            // Notify the worker threads that a task is available
            worker_sem.signal();
        }
    });

    // Create the worker threads
    for (size_t i = 0; i < numThreads; i++) {
        wts[i] = thread([this](){
            while (true) {
                // Wait for a task sent by the dispatcher thread to be available
                worker_sem.wait();
                // Acquire the lock
                lock_guard<mutex> lg(worker_mutex);
                // Check if the thread pool is shutting down
                if (tasks_.empty()) break;
                // Get the task
                auto task = tasks_.back();
                tasks_.pop_back();
                // Execute the task
                task();
            }
        });
    }
}

void ThreadPool::schedule(const function<void(void)>& thunk) {
    // Acquire the lock
    lock_guard<mutex> lg(dt_mutex);
    // Add the task to the tasks vector
    tasks_.push_back(thunk);
    // Notify the dispatcher thread that a task is available
    task_sem.signal();
}

void ThreadPool::wait() {
}

ThreadPool::~ThreadPool() {}