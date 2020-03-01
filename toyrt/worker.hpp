#pragma once
#include "task.hpp"
#include "task_timeline.hpp"

#include <cassert>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

class TaskScheduler;
class Scheduler;

class Worker {
 private:
  friend class TaskScheduler;
  Scheduler& q;
  TaskScheduler& scheduler;
  std::thread::id& myId;
  TaskTimeline timeline;

 public:
  Worker(Scheduler& _q, TaskScheduler& _scheduler, std::thread::id& _myId)
      : q(_q), scheduler(_scheduler), myId(_myId) {}
  void mainLoop();

 private:
  // No copy. Required to quiet icpc.
  Worker& operator=(const Worker&) {
    assert(false);
    return *this;
  }
};
