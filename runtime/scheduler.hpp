#ifndef _SCHEDULER_HPP
#define _SCHEDULER_HPP
#include <mutex>
#include <deque>

#include "task.hpp"
#include "common/data_recorder.hpp"

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
  std::mutex mutex;
  std::deque<TaskPtr> q;

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
