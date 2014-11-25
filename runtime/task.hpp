#ifndef _TASK_HPP
#define _TASK_HPP
#include <vector>
#include <string>
#include <algorithm>

#include "common/context.hpp"


class TaskTimeline;

/** Access mode for the data used by the task scheduler.
 */
enum AccessMode {
  READ = 1 << 0,
  WRITE = 1 << 1,
};

class Data;

/** Set of priorities for the tasks */
#define PRIORITIES 3
enum Priority {LOW = 2, NORMAL = 1, HIGH = 0};

class Task {
  friend class TaskScheduler;
public:
  static void execute(Task* t, TaskTimeline* timeline = NULL);
  bool isReady() const;
  /** Return extra data.

      The data returned have to be formatted as JSON, to be used by the timeline
      JSON output.
   */
  virtual std::string extraData() const {
    return std::string("{}");
  }

private:
  std::vector<std::pair<Data*, AccessMode> > params;
  int index;
  void* enclosingContext;

protected:
  bool doPostExecution;
  bool isCallback;
  bool noPrefetch;

public:
  std::string name;
  Priority priority;

public:
  Task(std::string _name = "Task") : name(_name), index(-1),
                                     enclosingContext(trace::Node::currentReference()),
                                     doPostExecution(true), isCallback(false),
                                     noPrefetch(false) {}
protected:
  virtual void call() = 0;
};

typedef Task* TaskPtr;
#endif
