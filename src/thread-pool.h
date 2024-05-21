/**
 * File: thread-pool.h
 * -------------------
 * This class defines the ThreadPool class, which accepts a collection
 * of thunks (which are zero-argument functions that don't return a value)
 * and schedules them in a FIFO manner to be executed by a constant number
 * of child threads that exist solely to invoke previously scheduled thunks.
 */

#ifndef _thread_pool_
#define _thread_pool_

#include <cstddef>     // for size_t
#include <functional>  // for the function template used in the schedule signature
#include <thread>      // for thread
#include <vector>      // for vector
#include <mutex>       // for mutex
#include <condition_variable>  // for condition_variable
#include "Semaphore.h"

struct Worker
{ 
  int id;
  std::thread wt;
  std::mutex wt_mutex;
  std::condition_variable wt_condition;
  std::condition_variable wt_finish_condition;
  std::function<void(void)> task;
  bool busy;
}typedef worker_t;

class ThreadPool {
 public:

/**
 * Constructs a ThreadPool configured to spawn up to the specified
 * number of threads.
 */
  ThreadPool(size_t numThreads);

/**
 * Schedules the provided thunk (which is something that can
 * be invoked as a zero-argument function without a return value)
 * to be executed by one of the ThreadPool's threads as soon as
 * all previously scheduled thunks have been handled.
 */
  void schedule(const std::function<void(void)>& thunk);  // va encolando tareas en el vector de tareas.

/**
 * Blocks and waits until all previously scheduled thunks
 * have been executed in full.
 */
  void wait(); // espera a que todas las tareas encoladas se hayan ejecutado.

/**
 * Waits for all previously scheduled thunks to execute, and then
 * properly brings down the ThreadPool and any resources tapped
 * over the course of its lifetime.
 */
  ~ThreadPool();
  
 private:
  std::thread dt;                // dispatcher thread handle
  std::vector<worker_t> wts;  // worker thread handles
  std::vector<std::function<void(void)>> tasks_;  // tasks vector

  Semaphore task_sem;  // semaphore to control access to the tasks vector
  Semaphore worker_sem;  // semaphore to control access to the worker threads
  
  std::mutex dt_mutex;  // mutex to protect the dispatcher thread
  std::mutex worker_mutex;  // mutex to protect the worker threads
  std::mutex tasks_mutex;  // mutex to protect the tasks vector

  std::condition_variable dt_condition;  // condition variable for the dispatcher thread
  std::condition_variable worker_condition;  // condition variable for the worker threads

  bool done_;  // flag to indicate that the ThreadPool is done

/**
 * ThreadPools are the type of thing that shouldn't be cloneable, since it's
 * not clear what it means to clone a ThreadPool (should copies of all outstanding
 * functions to be executed be copied?).
 *
 * In order to prevent cloning, we remove the copy constructor and the
 * assignment operator.  By doing so, the compiler will ensure we never clone
 * a ThreadPool.
 */
  ThreadPool(const ThreadPool& original) = delete;
  ThreadPool& operator=(const ThreadPool& rhs) = delete;
};




#endif