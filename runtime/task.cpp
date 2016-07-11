#include "task.hpp"
#include "data.hpp"
#include "dependencies.hpp"
#include "task_timeline.hpp"

/** Force the compiler to really access a variable. */
#define ACCESS_ONCE(x) (*(volatile decltype(x) *)&(x))
#define RELAX_CPU __asm__ __volatile("rep; nop")


void Task::execute(Task* t, TaskTimeline* timeline) {
  // If 't' is a send/recv, t->call() will push a request to the MPI thread, and t will be deleted
  // after this request is done. So potentially, t is invalid right after t->call(). That is why we backup
  // some fields of 't' here.
  bool doPost(t->doPostExecution);
  std::string name_bak(t->name);
  std::string extraData_bak(t->extraData());

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
    timeline->addTask(name_bak, start, stop, extraData_bak);
  } else {
    t->call();
  }
  if (doPost) { // false only for: MpiSend, MpiRecv
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
