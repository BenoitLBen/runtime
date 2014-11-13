#ifdef HAVE_RUNTIME
#ifndef _DEPENDENCIES_HPP
#define _DEPENDENCIES_HPP
#include <vector>
#include <unordered_map>
#include <deque>
#include <mutex>
#include <functional>
#include <condition_variable>
#include <thread>

#include "task.hpp"
#include "scheduler.hpp"
#include "lru.hpp"
#include "worker.hpp"
#include "../common/data_recorder.hpp"

class Data;

/** Some compilers (for instance, ICPC 13.0) don't support initializer lists.

    This hacky workaround allows the runtime to compile with it.

    To use the runtime with such a compiler, a list of dependencies has to be
    declared with:

    DEPS(3, DEP(d, READ), DEP(d2, WRITE), DEP(d3, READ))

    Otherwise one can just use the syntax:

    {{d, READ}, {d2, WRITE}, {d3, READ}}
*/
#if defined(__INTEL_COMPILER) && __INTEL_COMPILER <= 1300
#define NO_CPP11_INITIALIZER_LISTS
#endif

#ifdef NO_CPP11_INITIALIZER_LISTS
#define DEP(d, mode) std::make_pair(d, mode)
#define DEPS(n, ...) (std::pair<Data*, AccessMode>[]) {__VA_ARGS__}, n
#else
#define DEP(d, mode) {d, mode}
#define DEPS(n, ...) {__VA_ARGS__}
#endif

/** Main class for the runtime.

    This class is a singleton, the only instance can be accessed using
    TaskScheduler::getInstance().
*/
class TaskScheduler {
  /// Tasks successors
  struct TaskSuccessors {
    int count;
    std::deque<int> successors;
  };
  /// Tracker for the last read and write dependency on some data.
  struct AccessTracker {
    int lastWrite;
    std::deque<int> lastReads;

    AccessTracker() : lastWrite(-1), lastReads() {}
  };

private:
  /** Live and dead pointers to the tasks. This is used to match a task index to
      a Task* */
  std::vector<Task*> tasks;
  /** Last accesses to the data. */
  std::unordered_map<Data*, AccessTracker> dataAccess;
  /** task index -> task index dependencies. */
  std::vector<std::pair<int, int> > deps;
  /** Record the available tasks. */
  TimedDataRecorder<int> recorder;
  /** Scheduler. This is a pointer to be able to dynamically swap the actual
      scheduler type. */
  Scheduler* availableTasks;
  /** For each (referenced by its index), record the in-degree and the out
      edges. */
  std::vector<TaskSuccessors> succ;
  /** Serialize the modifications to succ in
      TaskScheduler::postTaskExecutionInternal().
      This mutex protects \a succ and \a tasksLeft. */
  std::mutex postTaskExecutionMutex;
  /** Number of tasks left to execute.*/
  int tasksLeft;
  /** Callback for the progress notifications. */
  std::function<void(int, int)> callback;
  /** Frequency of the progress notifications, in %. */
  double percentageFrequency;
  /** Next value of tasksLeft where a notification will occur. */
  int nextTaskCountWakeup;
  /** mutex associated with \a condition. */
  std::mutex conditionMutex;
  /** Condition variable used to wait for the progress notification wakeup. */
  std::condition_variable condition;
  /** Worker index -> thread Id */
  std::vector<std::thread::id> workerIds;
  /** Workers */
  std::vector<Worker> workers;
  /** Worker threads. */
  std::vector<std::thread> workerThreads;
  /** MPI rank in MPI_COMM_WORLD and size of the communicator. */
  int rank, size;

public:
  /** Maximum total data size before starting to swap. Defaults to unlimited. */
  size_t maxMemorySize;
  /** Least recently used Data instances */
  Lru<Data> lru;
  /** Mutex protecting the accesses to the LRU. */
  std::mutex lruMutex;

private:
  /** Total size of all the known data. */
  size_t dataSize;
  TimedDataRecorder<size_t> dataSizeRecorder;
  TimedDataRecorder<size_t> writtenDataRecorder;
  TimedDataRecorder<size_t> readDataRecorder;

public:
  /** Total number of inserted tasks. */
  int totalTasks;

public:
  ~TaskScheduler() {
    delete availableTasks;
    // We always record this, in the same file.
    // TODO: Make the recording optional (and disabled by default)
    recorder.toFile("tasks.txt");
    dumpTimeline("timeline.json");
  }
  /** Insert a task in an MPI cluster.

      The Task ownership is transfered to the TaskScheduler instance.

      @param task task to execute
      @param params Parameters and access modes
      @param node node to execute the task on
      @param priority priority of the task
   */
  void insertMpiTask(Task* task,
                     const std::vector<std::pair<Data*, AccessMode> >& params,
                     int node = -1, Priority priority = Priority::NORMAL);
#ifdef NO_CPP11_INITIALIZER_LISTS
  void insertMpiTask(Task* task,
                     const std::pair<Data*, AccessMode> params[], int n,
                     int node = -1, Priority priority = Priority::NORMAL) {
    std::vector<std::pair<Data*, AccessMode> > deps(params, params + n);
    insertMpiTask(task, deps, node, priority);
  }
#endif
  /** Insert a task in shared memory.

      @warning Do not use on an MPI cluster

      The Task ownership is transfered to the TaskScheduler instance.

      @param task task to execute
      @param params Parameters and access mode
      @param priority priority of the task
   */
  void insertTask(Task* task,
                  const std::vector<std::pair<Data*, AccessMode> >& params,
                  Priority priority = Priority::NORMAL);
#ifdef NO_CPP11_INITIALIZER_LISTS
  void insertTask(Task* task,
                  const std::pair<Data*, AccessMode> params[], int n,
                  Priority priority = Priority::NORMAL) {
    std::vector<std::pair<Data*, AccessMode> > deps(params, params + n);
    insertTask(task, deps, priority);
  }
#endif
  /** Submit an asynchronous data request for a data on a node.

      @param d Data to get
      @param node node to get it on.
   */
  void getDataOnNode(Data* d, int node);
  /** Dump a DAG representation to a Graphviz file.

      @param filename Output file name.
   */
  void graphvizOutput(const char* filename) const;
  /** Update the internal structures at the end of a task, and free it.

      @param task
   */
  // TODO: Should this function really be public ?
  void postTaskExecution(Task* task);
  /** Launch the execution.

      This call blocks until all the tasks are done.

      @param n Number of worker threads.
   */
  void go(int n);
  /** Set a progress callback function.

      The callback will be executed in the main thread (and not a worker) at a
      tunable frequency, in percentage of total completion.

      @param _callback the callback
      @param _percentageFrequency the frequency in % of the total number of tasks
   */
  void setProgressCallback(std::function<void(int, int)> _callback,
                           double _percentageFrequency) {
    callback = _callback;
    percentageFrequency = _percentageFrequency;
  };
  /** Return the index of the current worker thread. */
  int currentId() const;
  /** Write the timeline to a JSON file.

   @param filename The file name.
  */
  void dumpTimeline(const char* filename) const;

public:
  /** Get the instance of TaskScheduler. */
  static TaskScheduler& getInstance() {
    static TaskScheduler s;
    return s;
  }

private:
  /** Prepare the dependencies and the initial available tasks.
   */
  void prepare();
  /** Wake up the sleeping thread if necessary for the progress notifications.
   */
  void notifyProgress();
  void startPrefetch(Task* t);
  void evict();
  /** Private constructor, construction is not allowed. */
  TaskScheduler();
  TaskScheduler(const TaskScheduler&); // No copy
  /** Stop all the workers. */
  void stopAllWorkers();
  /** Real (synchronized) work for postTaskExecution().

      @param task
      @param callbacks
   */
  void postTaskExecutionInternal(Task* task, std::vector<Task*>& callbacks);
public:
  // Remove a Data from the data tracking.
  void unregisterData(Data* d);
};
#endif
#endif