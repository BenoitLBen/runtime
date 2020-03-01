#pragma once
#include <algorithm>
#include <string>
#include <vector>

#include "context/context.hpp"

class TaskTimeline;

class Data;

/** Set of priorities for the tasks */
#define PRIORITIES 3
enum Priority { LOW = 2, NORMAL = 1, HIGH = 0 };

/** Access mode for the data used by the task scheduler.
 */
enum toyRT_AccessMode {
  toyRT_READ,
  toyRT_WRITE,
  toyRT_READ_WRITE,
  toyRT_REDUX,
  toyRT_UNDEFINED
};

typedef std::vector<std::pair<const void*, toyRT_AccessMode> > toyRT_DepsArray;

class Task {
  friend class TaskScheduler;

 public:
  static void execute(Task* t, TaskTimeline* timeline = NULL);
  bool isReady() const;
  /** Return extra data.

      The data returned have to be formatted as JSON, to be used by the timeline
      JSON output.
   */
  virtual std::string extraData() const { return std::string("{}"); }

 private:
  toyRT_DepsArray params;
  int index;
  void* submittingContext;  // Node where the task is created (and probably
                            // submitted)

 protected:
  /*! \brief Tells if the "post-execution" process must be done after this task

     It is true by default, false only for: MpiSend, MpiRecv (because in that
     case postTaskExecution() is called in processCompletedRequest() in
     mpi.cpp).
    */
  bool doPostExecution;
  /*! \brief Tells if a task is a callback

     It is false by default, true only for: MpiSend, MpiRecv, Sync, Flush,
     Deallocate. It is executed immediately when its dependencies are satisfied
     (without going in a tasks queue)
    */
  bool isCallback;
  /*! \brief Deactivate the prefetch for this task

     It is false by default (which means prefetch is ON), true only for: Flush
(of course: we don't load stuff in memory if we mean to dump them on disk
immediately after).
    */
  bool noPrefetch;

 public:
  std::string name;
  Priority priority;

 public:
  Task(std::string _name = "Task")
      : index(-1),
        submittingContext(trace::Node::currentReference()),
        doPostExecution(true),
        isCallback(false),
        noPrefetch(false),
        name(_name),
        priority(NORMAL) {}
  virtual ~Task() {}
  std::string description() const;

 protected:
  virtual void call() = 0;
};

typedef Task* TaskPtr;
