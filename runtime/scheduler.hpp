#ifndef _SCHEDULER_HPP
#define _SCHEDULER_HPP
#include <mutex>
#include <deque>

#include "task.hpp"
#include "common/data_recorder.hpp"

/** Synchronized queue (FIFO) */
template<typename T> class Queue {
private:
  std::deque<T> q;
  std::mutex mutex;

public:
  /** Clear the queue.
   */
  void clear() {
    std::lock_guard<std::mutex> guard(mutex);
    q.clear();
  }
  /** Push a new value into the queue.
   */
  void push(T& value) {
    std::lock_guard<std::mutex> guard(mutex);
    q.push_back(value);
  }
  /** Try to get a value from the queue, return true for success.

      \param value
      \return true if a value is popped, false otherwise.
   */
  bool tryPop(T& value) {
    std::lock_guard<std::mutex> guard(mutex);
    if (q.empty()) {
      return false;
    }
    value = q.front();
    q.pop_front();
    return true;
  }
};


/** Generic Task scheduler.

    This class handles the scheduling of a set of available tasks. The tasks are
    pushed into the scheduler, and the order in which they are popped is
    determined by the scheduler.
 */
class Scheduler {
protected:
  TimedDataRecorder<int>* recorder;
  int taskCount;

public:
  Scheduler(TimedDataRecorder<int>* _recorder = NULL) : recorder(_recorder),
                                                        taskCount(0) {}
  virtual ~Scheduler() {};
  /** Reset the scheduler. */
  virtual void clear() = 0;
  /** Push a task */
  virtual void push(TaskPtr task) = 0;
  /** Try to pop a task. See \a Queue<T>::tryPop(). */
  virtual bool tryPop(TaskPtr& task) = 0;
};


/** Simple FIFO scheduler.
 */
class EagerScheduler : public Scheduler {
private:
  Queue<TaskPtr> q;

public:
  EagerScheduler(TimedDataRecorder<int>* recorder = NULL) : Scheduler(recorder) {}
  void clear();
  void push(TaskPtr task);
  bool tryPop(TaskPtr& task);
};


/** Simple FIFO scheduler with priorities (several FIFOs).
 */
class PriorityScheduler : public Scheduler {
private:
  std::deque<TaskPtr> q[PRIORITIES];
  std::mutex mutex;

public:
  PriorityScheduler(TimedDataRecorder<int>* recorder = NULL) : Scheduler(recorder) {}
  void clear();
  void push(TaskPtr task);
  bool tryPop(TaskPtr& task);
};
#endif
