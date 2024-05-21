/**
 * File: thread-pool.cc
 * --------------------
 * Presents the implementation of the ThreadPool class.
 */
#include "thread-pool.h"
using namespace std;

ThreadPool::ThreadPool(size_t numThreads) : wts(numThreads), done_(false) {
    // Create the dispatcher thread
    dt = thread([this] {
        while (true) {
            unique_lock<mutex> ul(dt_mutex);      // VER DE APLICAR UN SEMÁFORO ACÁ
            // Check if the ThreadPool is done
            if (done_) break;
         
            if (tasks_.empty()) {   // No tasks to do
                printf("No tasks to do\n");
                dt_condition.notify_all();
            } else {                // Get the task
                auto task_for_worker = tasks_.front();
                tasks_.erase(tasks_.begin());
                ul.unlock();

                // Put a worker thread to work
                for (size_t i = 0; i < wts.size(); i++) {
                    if (!wts[i].busy && wts[i].task == nullptr) {  // Find a free worker thread
                        wts[i].task = task_for_worker;
                        wts[i].busy = true;
                        wts[i].wt_condition.notify_one();
                        break;
                    }
                    if (i == wts.size() - 1) {  // All worker threads are busy, put the task back
                        ul.lock();
                        tasks_.push_back(task_for_worker);
                        ul.unlock();
                        task_sem.signal();
                    }
                }
                ul.lock();
            }
            ul.unlock();
            //task_sem.wait();              // REVISARRRRR
        }
    });

    // Create the worker threads
    for (size_t i = 0; i < numThreads; i++) {
        wts[i].id = i;
        wts[i].busy = false;
        wts[i].task = nullptr;
        wts[i].wt = thread([this, i] {
            while (true) {
                unique_lock<mutex> ul(wts[i].wt_mutex);
                
                // Check if the ThreadPool is done
                if (done_ && wts[i].task == nullptr) break;

                // Wait for a task
                printf("Worker thread %d waiting\n", i);
                wts[i].wt_condition.wait(ul, [this, i] {
                    return wts[i].task != nullptr || done_;
                });
                printf("Worker thread %d working\n", i);
                
                // Check if the ThreadPool is done
                if (done_ && wts[i].task == nullptr) break;
                ul.unlock();

                // Execute the task
                if (wts[i].task) {
                    wts[i].task();  // Execute the task
                    ul.lock();
                    wts[i].task = nullptr;
                    wts[i].busy = false;
                    wts[i].wt_finish_condition.notify_one();
                }
                ul.unlock();
            }
        });
    }
}

void ThreadPool::schedule(const function<void(void)>& thunk) {
    lock_guard<mutex> lg(dt_mutex);
    tasks_.push_back(thunk);
    task_sem.signal();
    printf("Task scheduled\n");
}

void ThreadPool::wait() {
    printf("Wait function called\n");
    unique_lock<mutex> ul(dt_mutex);
    // Wait for all tasks to be done
    printf("Waiting for all tasks to be done\n");
    dt_condition.wait(ul, [this] {
        return tasks_.empty();
    });

    printf("All tasks done\n");

    ul.unlock();

    for (size_t i = 0; i < wts.size(); i++) {
        // Wait for all worker threads to be done
        if (wts[i].busy) {
            unique_lock<mutex> ul(wts[i].wt_mutex);
            wts[i].wt_finish_condition.wait(ul, [this, i] {
                return !wts[i].busy;
            });

            ul.unlock();
        }
    }
}

ThreadPool::~ThreadPool() {
    printf("Destructor called\n");
    // Signal the dispatcher thread to finish
    unique_lock<mutex> dt_lock(dt_mutex);
    done_ = true;
    task_sem.signal();
    dt_condition.notify_all();
    dt_lock.unlock();

    
    dt.join();

    // Signal the worker threads to finish
    for (size_t i = 0; i < wts.size(); i++) {
        unique_lock<mutex> wt_lock(wts[i].wt_mutex);
        wts[i].wt_condition.notify_one();
        wt_lock.unlock();

        wts[i].wt.join();
    }
}