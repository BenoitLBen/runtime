#include "task.hpp"
#include "data.hpp"
#include "dependencies.hpp"
#include "task_timeline.hpp"

/** Force the compiler to really access a variable. */
#define ACCESS_ONCE(x) (*(volatile decltype(x) *)&(x))
#define RELAX_CPU __asm__ __volatile("rep; nop")


void Task::execute(Task* t, TaskTimeline* timeline) {
  if (!t->noPrefetch) {
    // Make sure that all the prefetched are done.
    for (auto& p : t->params) {
      Data* d = p.first;
      // Loop while the data has not been fetched
      while (true) {
        if (!ACCESS_ONCE(d->swapped)) {
          break;
        }
        RELAX_CPU;
      }
    }
  }
  trace::Node::setEnclosingContext(t->enclosingContext);
  if (timeline) {
    auto start = std::chrono::high_resolution_clock::now();
    t->call();
    auto stop = std::chrono::high_resolution_clock::now();
    timeline->addTask(t, start, stop);
  } else {
    t->call();
  }
  if (t->doPostExecution) {
    TaskScheduler::getInstance().postTaskExecution(t);
  }
}

bool Task::isReady() const {
  if (!noPrefetch) {
    for (auto& p : params) {
      Data* d = p.first;
      if (ACCESS_ONCE(d->swapped)) {
        return false;
      }
    }
  }
  return true;
}
