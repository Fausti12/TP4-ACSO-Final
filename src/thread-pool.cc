/**
 * File: thread-pool.cc
 * --------------------
 * Presents the implementation of the ThreadPool class.
 */

#include "thread-pool.h"
using namespace std;

ThreadPool::ThreadPool(size_t numThreads) : wts(numThreads) {
    // create the worker threads
    for (size_t i = 0; i < numThreads; i++) {
        wts[i] = thread([this] {
            while (true) {
                // wait for the dispatcher to signal that there is a task
                unique_lock<mutex> lk(mutex_);
                dt_condition_.wait(lk, [this] { return !tasks_.empty() || done_; });
                // if there are no tasks and done is true, exit the thread
                if (tasks_.empty() && done_) return;
                // get the task
                auto task = tasks_.back();
                tasks_.pop_back();
                // signal the dispatcher that the task has been taken
                dt_condition_.notify_one();
                // execute the task
                task();
            }
        });
    }

    // create the dispatcher thread
    dt = thread([this] {
        // El hilo dispatcher debería ejecutar en un loop. Es decir, en cada iteración, debería
        // dormir hasta que schedule le indique que se ha añadido algo a la cola. Luego,
        // esperaría a que un worker esté disponible, lo seleccionaría, lo marcaría como no
        // disponible, extraería una función de la cola, pondría una copia de esa función en un
        // lugar donde el worker pueda acceder a ella y luego señalaría al trabajador para que
        // la ejecutara.
        while (true) {
            // wait for a task to be scheduled
            unique_lock<mutex> lk(mutex_);
            dt_condition_.wait(lk, [this] { return !tasks_.empty() || done_; });
            // if there are no tasks and done is true, exit the thread
            if (tasks_.empty() && done_) return;
            // signal a worker thread to execute the task
            wt_condition_.notify_one();

            // wait for the worker to take the task
            dt_condition_.wait(lk, [this] { return tasks_.empty(); });

            // if there are no tasks, exit the thread
            if (tasks_.empty()) return;
        }
    });    
}

void ThreadPool::schedule(const function<void(void)>& thunk) {
    // add the thunk to the tasks vector
    tasks_.push_back(thunk);
    // notify the dispatcher thread
    dt_condition_.notify_one(); 
}

void ThreadPool::wait() {
    // wait for all tasks to be executed
    unique_lock<mutex> lk(mutex_);
    dt_condition_.wait(lk, [this] { return tasks_.empty(); });

    // signal the worker threads to exit
    done_ = true;
    wt_condition_.notify_all();

    // wait for the worker threads to exit
    lk.unlock();
    for (auto& wt : wts) {
        wt.join();
    }

    // wait for the dispatcher thread to exit
    dt.join();

    // reset the done flag
    done_ = false;

    // notify the caller that all tasks have been executed
    dt_condition_.notify_all();

    // clear the tasks vector
    tasks_.clear();

    // notify the caller that the ThreadPool is ready to be used again
    dt_condition_.notify_all();
}

ThreadPool::~ThreadPool() {}