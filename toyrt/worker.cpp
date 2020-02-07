#include "worker.hpp"
#include "dependencies.hpp"

void Worker::mainLoop() {
  myId = std::this_thread::get_id();
  {
    DECLARE_CONTEXT;

  TaskPtr task;
  int spinCounter = 1;
  while (true) {
    bool notEmpty = q.tryPop(task);
    if (!notEmpty) {
      // Exponential backoff
      for (int i = 0; i < spinCounter; i++) {
          // Equivalent to the "pause" instruction. Approx 1e-8 s on macbook
        __asm__ __volatile("rep; nop");
      }
        if (spinCounter < (1 << 20)) { // Max pause loop is with 1e6 iterations = 10 ms approx.
        spinCounter <<= 1;
      }
      continue;
    }
    spinCounter = 1;
    // NULL task is a convention to signal the worker that it's time to stop.
    if (!task) {
      break;
    }
    // If the task is not ready (prefetch is still in progress), give it back to
    // the scheduler.
    if (false && !task->isReady()) {
      q.push(task);
    } else {
      Task::execute(task, &timeline);
    }
  }
  }
  myId = static_cast<std::thread::id>(0);
}
