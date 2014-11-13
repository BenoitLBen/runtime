#include <cassert>

#include "runtime.hpp"


class Scheduler {
public:
  virtual void push(Task* t) = 0;
  virtual bool tryPop(Task*& t) = 0;
};

class EagerScheduler {
private:
  std::mutex mutex;
  std::deque<Task*> fifo;

public:
  EagerScheduler() : Scheduler() {}
  void push(Task* task) {
    std::lock_guard<std::mutex> lock(mutex);
    fifo.push_back(task);
  }
  bool tryPop(Task*& task) {
    std::lock_guard<std::mutex> lock(mutex);
    if (fifo.empty()) {
      return false;
    } else {
      task = fifo.front();
      fifo.pop_front();
      return true;
    }
  }
};


void Worker::mainLoop() {
  while (true) {
    Task* task = NULL;
    while (!q.tryPop(task)) {
      // Spin
    }
    // NULL task to signal completion
    if (!task) {
      return;
    }
    task->call();
    runtime.postTaskExecution(task);
  }
}


void Runtime::insertTask(Task* task, DepList params) {
  assert(task);
  task->index = (int) tasks.size();
  assert(succ.size() == tasks.size());
  tasks.push_back(task);
  succ.push_back((TaskSuccessors) {0, std::deque<int>()});

  // Warning: If the application is stupid enough to submit the same dep twice, we
  // will deadlock.
  for (auto& dep : params) {
    void* data = dep.first;
    AccessMode mode = p.second;
  }
}
