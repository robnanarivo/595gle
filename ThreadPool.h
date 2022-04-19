/*
 * Copyright ©2022 Travis McGaha.  All rights reserved.  Permission is
 * hereby granted to students registered for University of Pennsylvania
 * CIT 595 for use solely during Spring Semester 2022 for purposes of
 * the course.  No other use, copying, distribution, or modification
 * is permitted without prior written consent. Copyrights for
 * third-party components of this work must be honored.  Instructors
 * interested in reusing these course materials should contact the
 * author.
 */

#ifndef THREADPOOL_H_
#define THREADPOOL_H_

extern "C" {
  #include <pthread.h>  // for the pthread threading/mutex functions
}

#include <cstdint>   // for uint32_t, etc.
#include <list>       // for std::list

namespace searchserver {

// A ThreadPool is, well, a pool of threads. ;)  A ThreadPool is an
// abstraction that allows customers to dispatch tasks to a set of
// worker threads.  Tasks are queued, and as a worker thread becomes
// available, it pulls a task off the queue and invokes a function
// pointer in the task to process it.  When it is done processing the
// task, the thread returns to the pool to receive and process the next
// available task.
class ThreadPool {
 public:
  // Construct a new ThreadPool with a certain number of worker
  // threads.  Arguments:
  //
  //  - num_threads:  the number of threads in the pool.
  explicit ThreadPool(uint32_t num_threads);
  virtual ~ThreadPool();

  // This inner class defines what a Task is.  A worker thread will
  // pull a task off the task queue and invoke the thread_task_fn
  // function pointer inside of it, passing it the Task* itself as an
  // argument.  The thread_task_fn takes ownership of the Task and
  // must arrange to delete the task when it is done.  Customers will
  // probably want to subclass Task to add task-specific fields to it.
  class Task;
  typedef void (*thread_task_fn)(Task *arg);

  class Task {
   public:
    // "f" is the task function that a worker thread should invoke to
    // process the task.
    explicit Task(thread_task_fn func) : func_(func) { }

    // The dispatch function.
    thread_task_fn func_;
  };

  // Customers use dispatch() to enqueue a Task for dispatch to a
  // worker thread.
  void dispatch(Task *t);

  // A lock and condition variable that worker threads and the
  // dispatch function use to guard the Task queue.
  pthread_mutex_t q_lock_;
  pthread_cond_t  q_cond_;

  // The queue of Tasks waiting to be dispatched to a worker thread.
  std::list<Task *> work_queue_;

  // This should be set to "true" when it is time for the worker
  // threads to kill themselves, i.e., when the ThreadPool is
  // destroyed.  A worker thread will check this variable before
  // picking up its next piece of work; if it is true, the worker
  // threads will kill themselves off.
  bool killthreads_;

  // This variable stores how many threads are currently running.  As
  // worker threads are born, they increment it, and as worker threads
  // kill themselves, they decrement it.
  uint32_t num_threads_running_;

 private:
  // The pthreads pthread_t structures representing each thread.
  pthread_t *thread_array_;
};

}  // namespace searchserver

#endif  // THREADPOOL_H_