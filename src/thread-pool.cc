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
            dt_mutex.lock();
            // Check if the thread pool is shutting down (por las dudas)
            if (tasks_.empty()) {
                dt_condition.notify_one(); // Notify all waiting threads (This is for the wait function)
                dt_mutex.unlock();
                printf("Dispatcher thread is shutting down\n");
                break;
            }
            // Unlock the mutex
            dt_mutex.unlock();
        }
    });

    // Create the worker threads
    for (size_t i = 0; i < numThreads; i++) {
        wts[i] = thread([this](){
            while (true) {
                // Acquire the lock
                worker_mutex.lock(); 
                // Check if the thread pool is shutting down (por las dudas)
                if (tasks_.empty()){
                    worker_condition.notify_all(); // Notify all waiting threads (This is for the wait function)
                    worker_mutex.unlock();
                    printf("Worker thread is shutting down\n");
                    break;
                }
                done += 1;
                // Get the task and remove it from the tasks vector (FIFO)
                auto task = tasks_.front();
                tasks_.erase(tasks_.begin());
                // unlock the mutex
                worker_mutex.unlock();
                // Execute the task
                task();
                done -= 1;
               
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
    // Wait for all tasks to be executed
    unique_lock<mutex> ul(dt_mutex);
    dt_condition.wait(ul, [this] { return tasks_.empty(); });

    // Wait for all worker threads to finish
    printf("Waiting for worker threads to finish: %d \n", done);
    worker_condition.wait(ul, [this] { return done == 0; });

    // Print a message
    printf("All tasks have been executed\n");
}

ThreadPool::~ThreadPool() {
    // Wait for all worker threads to finish
    for (auto& wt : wts) {
        wt.join();
    }
    // Wait for the dispatcher thread to finish
    dt.join();
    // Print a message
    printf("All threads have been shut down\n");
}