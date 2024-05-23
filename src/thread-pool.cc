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
            // Wait for a task (or the wait function or the destructor to be called)
            task_sem.wait();

            // Check if the ThreadPool is done
            if (done_){
            break;
            }
            unique_lock<mutex> ul(dt_mutex);
            if (tasks_.empty()) {   // No tasks to do, notify the wait function
                dt_condition.notify_all();
                ul.unlock();
            } else {               
                // Get the task
                auto task_for_worker = tasks_.front();
                tasks_.erase(tasks_.begin());
                ul.unlock();

                // Put a worker thread to work
                worker_sem.wait(); // Wait for a worker thread to be available
                for (size_t i = 0; i < wts.size(); i++) {
                    if (!wts[i].busy && wts[i].task == nullptr) {   // Find the free worker thread
                        wts[i].task = task_for_worker;
                        wts[i].busy = true;
                        wts[i].wt_condition.notify_one();           // Notify the worker thread
                        break;
                    }
                }   
            }
        }
    });

    // Create the worker threads
    for (size_t i = 0; i < numThreads; i++) {
        wts[i].id = i;
        wts[i].busy = false;
        wts[i].task = nullptr;
        wts[i].wt = thread([this, i] {
            while (true) {
                // Signal that the worker thread is available ()
                worker_sem.signal();

                // Wait for a task
                unique_lock<mutex> ul(wts[i].wt_mutex);

                // Wait for a task (we tell the dispatcher thread that we are ready to work and 
                // the dispatcher thread will notify us when there is a task to do). Or wait for
                // the destructor to be called.
                wts[i].wt_condition.wait(ul, [this, i] {
                    return wts[i].task != nullptr || done_;
                });
                
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
                    ul.unlock();    
                }
            }
        });
    }
}

void ThreadPool::schedule(const function<void(void)>& thunk) {
    unique_lock<mutex> ul(dt_mutex); 
    tasks_.push_back(thunk);
    task_sem.signal();
    ul.unlock();
}

void ThreadPool::wait() {
    task_sem.signal();
    unique_lock<mutex> ul(dt_mutex);
    // Wait for all tasks to be done 
    dt_condition.wait(ul, [this] {  // Check if there are tasks to do
        return tasks_.empty();
    });
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
    ThreadPool::wait();
    // Signal the dispatcher thread to finish
    task_sem.signal();
    done_ = true;     
    
    dt.join();

    // Signal the worker threads to finish
    for (size_t i = 0; i < wts.size(); i++) {
        wts[i].wt_condition.notify_one();
    }
    for (size_t i = 0; i < wts.size(); i++) {
        wts[i].wt.join();
    }
}