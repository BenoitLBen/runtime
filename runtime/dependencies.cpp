#include <cassert>
#include <fstream>
#include <set>
#include <limits>
#include <fstream>

#include "dependencies.hpp"
#include "data.hpp"
#include "mpi.hpp"
#include "disk.hpp"

#include <mpi.h>


/** Task used to add synchronization points.

    This task doesn't do anything (call() is empty).
 */
class SyncTask : public Task {
  /** Dummy data used to add a synchronization point.

      Warning: these data are not compatible with MPI and OOC.
   */
  class DummyData : public Data {
  public:
    DummyData() : Data() {
      swappable = false;
    }
    ssize_t pack(void** ptr) {return 0;}
    void unpack(void* ptr, ssize_t count) {}
    void deallocate() {}
    size_t size() {return 0;}
  };

public:
  Data* d;

  void call() {}
  SyncTask() : Task("Sync"), d(new DummyData()) {
    isCallback = true;
  }
};


TaskScheduler::TaskScheduler() : dataAccess(), deps(), availableTasks(NULL),
                                 succ(), postTaskExecutionMutex(), tasksLeft(0),
                                 callback(), percentageFrequency(-1),
                                 nextTaskCountWakeup(0), conditionMutex(),
                                 condition(),
                                 maxMemorySize(std::numeric_limits<size_t>::max()),
                                 totalTasks(0) {
  // availableTasks = new EagerScheduler(&recorder);
  availableTasks = new PriorityScheduler(&recorder);
}


static
int commCountForRank(const std::vector<std::pair<Data*, AccessMode> >& params,
                     int rank) {
  MpiRequestPool& mpi = MpiRequestPool::getInstance();
  int comms = 0;
  for (const auto& p : params) {
    Data* d = p.first;
    if ((d->rank != rank)) {
      if (mpi.cache.isValidOnNode(d, rank)) {
        comms += (p.second == WRITE ? 1 : 0);
      } else {
        // 2 for the write back to the reference node
        comms += (p.second == WRITE ? 2 : 1);
      }
    }
  }
  return comms;
}

static
int findExecuteeNode(const std::vector<std::pair<Data*, AccessMode> >& params,
                     int size) {
  int node = params[0].first->rank;
  for (const auto& p : params) {
    if (p.second == WRITE) {
      node = p.first->rank;
      break;
    }
  }
  // Look for the node that requires the less number of MPI messages.
  int minComms = commCountForRank(params, node);
  for (int rank = 0; rank < size; rank++) {
    int comms = commCountForRank(params, rank);
    if (comms < minComms) {
      minComms = comms;
      node = rank;
    }
  }
  return node;
}


void TaskScheduler::insertMpiTask(Task* task,
                                  const std::vector<std::pair<Data*, AccessMode> >& params,
                                  int node, Priority priority) {
  if (size == 0) {
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
  }
  if (size == 1) {
    insertTask(task, params, priority);
    return;
  }
  MpiRequestPool& mpi = MpiRequestPool::getInstance();
  int me = rank;
  for (auto p : params) {
    assert(p.first->tag != 0);
  }
  if (node == -1) {
    node = findExecuteeNode(params, size);
  }
  assert(node != -1);
  Data* dummy = NULL;

  // Before posting the recv() of the foreign deps, make sure that all the local
  // dependencies are available. To do that, we create a dummy task which has
  // all the foreign dependencies as deps, and a dummy marker data which is a
  // dep of all the recv() calls.
  if (node == me) {
    std::vector<std::pair<Data*, AccessMode> > localDeps;
    for (const auto& p : params) {
      Data* d = p.first;
      if (d->rank == me) {
        localDeps.push_back(p);
      }
    }
    if ((localDeps.size() != 0) && (localDeps.size() != params.size())) {
      SyncTask* syncTask = new SyncTask();
      dummy = syncTask->d;
      localDeps.push_back(std::make_pair(dummy, WRITE));
      insertTask(syncTask, localDeps);
    }
  }

  // Before the task:
  // - If we are executing the task: receive the data we don't have
  // - Otherwise: Send the data we have to the relevant node'
  for (const auto& p : params) {
    Data* d = p.first;
    assert(d->tag != 0);
    // If we own the data and are not the node executing the task, we send it
    // to this node.
    if ((d->rank == me) && (node != me)) {
      if (!mpi.cache.isValidOnNode(d, node)) {
        insertTask(new MpiSendTask(d, node), DEPS(1, DEP(d, READ)) , priority);
      }
    } else if ((d->rank != me) && (node == me)) {
      if (!mpi.cache.isValid(d)) {
        if (dummy) {
          insertTask(new MpiRecvTask(d, d->rank),
                     DEPS(2, DEP(d, WRITE), DEP(dummy, READ)), priority);
        } else {
          insertTask(new MpiRecvTask(d, d->rank),
                     DEPS(1, DEP(d, WRITE)), priority);
        }
      }
    }
    mpi.cache.sendData(d, d->rank, node);
  }
  // Avoid cluttering the dataAccess hash table.
  if (dummy) {
    unregisterData(dummy);
    delete dummy;
  }
  // We execute it.
  if (node == me) {
    insertTask(task, params, priority);
  }
  // After the task:
  // - If we are executing: send the data we wrote to that we don't own back to their owner
  // - Otherwise: receive the data we own that the task wrote to
  for (const auto& p : params) {
    Data* d = p.first;
    if (p.second != WRITE) {
      continue;
    }
    // If we own the data and are not the node executing the task, get it back.
    if ((d->rank == me) && (node != me)) {
      insertTask(new MpiRecvTask(d, node), DEPS(1, DEP(d, WRITE)), priority);
    } else if ((d->rank != me) && (node == me)) {
      insertTask(new MpiSendTask(d, d->rank), DEPS(1, DEP(d, READ)), priority);
    }
    mpi.cache.sendData(d, node, d->rank);
    mpi.cache.invalidateData(d, node);
  }
  // Free all the data we don't own right away.
  if (!mpi.cache.enabled) {
    for (const auto& p : params) {
      mpi.cache.invalidateData(p.first);
    }
  }
}


void TaskScheduler::getDataOnNode(Data* d, int node) {
  // TODO: Integrate this with the MPI Communication cache, to note that the
  // data was received and to avoid useless comms.
  if (size == 1) return;
  if (node == rank) {
    if (d->rank != rank) {
      insertTask(new MpiRecvTask(d, d->rank), DEPS(1, DEP(d, WRITE)));
    }
  } else if (d->rank == rank) {
    insertTask(new MpiSendTask(d, node), DEPS(1, DEP(d, READ)));
  }
}


void TaskScheduler::insertTask(Task* task,
                               const std::vector<std::pair<Data*, AccessMode> >& params,
                               Priority priority) {
  totalTasks++;
  task->priority = priority;
  task->index = (int) tasks.size();
  assert(succ.size() == tasks.size());
  tasks.push_back(task);
  succ.push_back((TaskSuccessors) {0, std::deque<int>()});

  // To avoid duplicate dependencies
  std::set<std::pair<int, int> > localDeps;
  task->params.clear();
  task->params = params;
  for (auto& p : params) {
    Data* param = p.first;
    AccessMode mode = p.second;
    assert(param);
    auto& access = dataAccess[param];

    if (param->oldSize == 0) {
      param->oldSize = param->size();
      dataSize += param->oldSize;
    }

    switch (mode) {
    case READ:
      if (access.lastWrite != -1) {
        // Add a dependencies between the write and the read
        localDeps.insert(std::make_pair(access.lastWrite, task->index));
      }
      access.lastReads.push_back(task->index);
      break;
    case WRITE:
      if (access.lastWrite != -1) {
        // Add a dependency between the two writes
        localDeps.insert(std::make_pair(access.lastWrite, task->index));
      }
      for (auto t : access.lastReads) {
        // Add a dependency between the reads and the write
        localDeps.insert(std::make_pair(t, task->index));
      }
      access.lastReads.clear();
      access.lastWrite = task->index;
      break;
    }
  }
  for (auto& dep : localDeps) {
    deps.push_back(dep);
  }
}

void TaskScheduler::graphvizOutput(const char* filename) const {
  std::ofstream f(filename);
  static const int nColors = 14;
  static const char* colors[] = {"green", "red", "blue", "gold",
                                 "purple", "magenta", "cyan",
                                 "deeppink", "darkslategray4",
                                 "darksalmon", "gray66", "lavender",
                                 "lightslateblue", "turquoise"};
  f << "digraph tasks {\n";
  for (auto& dep : deps) {
    f << dep.first << " -> " << dep.second << ";\n";
  }
  std::map<std::string, int> nameToColors;
  int colorIndex = 0;
  for (auto t : tasks) {
    if (nameToColors.find(t->name) == nameToColors.end()) {
      nameToColors[t->name] = colorIndex++;
    }
    f << t->index << " [label=\"" << t->name
      << "\",style=filled,fillcolor=\""
      << colors[nameToColors[t->name] % nColors]<< "\"];\n";
  }
  f << "}" << std::endl;
}

void TaskScheduler::prepare() {
  for (auto dep : deps) {
    assert(dep.first < succ.size());
    assert(dep.second < succ.size());
    // Le premier a un successeur de plus
    succ[dep.first].successors.push_back(dep.second);
    // Le second a un predecesseur de plus
    succ[dep.second].count++;
  }

  for (int i = 0; i < (int) succ.size(); i++) {
    if (succ[i].count == 0) {
      startPrefetch(tasks[i]);
      availableTasks->push(tasks[i]);
    }
  }
  tasksLeft = succ.size();
  // For the notifications
  if (percentageFrequency > 0.) {
    nextTaskCountWakeup = (1. - (percentageFrequency / 100.)) * tasksLeft;
  }
  // The dependencies are no longer needed.
  deps.clear();
}

void TaskScheduler::stopAllWorkers() {
  int n = (int) workerThreads.size();
  for (int i = 0; i < n; i++) {
    availableTasks->push(NULL);
  }
 }

void TaskScheduler::postTaskExecution(Task* task) {
  std::vector<Task*> callbacks;
  postTaskExecutionInternal(task, callbacks);
  for (auto t : callbacks) {
    Task::execute(t);
  }
}

void TaskScheduler::startPrefetch(Task* t) {
  if (t->noPrefetch) return;
  std::lock_guard<std::mutex> guard(lruMutex);
  for (const auto& p : t->params) {
    Data* d = p.first;
    d->refCount++;
    lru.remove(d);
    if ((!d->prefetchInFlight) && d->swapped) {
      d->prefetchInFlight = true;
      IoThread::getInstance().pushPrefetch(d);
      dataSize += d->oldSize;
      readDataRecorder.record(d->oldSize);
    }
  }
}

void TaskScheduler::evict() {
  // Quick return to avoid taking the mutex on the LRU if it is not required.
  if (dataSize <= maxMemorySize) {
    return;
  }
  std::lock_guard<std::mutex> guard(lruMutex);
  Data* d = NULL;
  while ((dataSize > maxMemorySize) && (d = lru.removeOldest())) {
    dataSize -= d->oldSize;
    d->prefetchInFlight = false;
    // Don't release the mutex. This assumes that pushSwap() is fast enough.
    // Be careful though: this means that we are holding several mutexes here.
    IoThread::getInstance().pushSwap(d);
    writtenDataRecorder.record(d->oldSize);
  }
}

void TaskScheduler::postTaskExecutionInternal(Task* task,
                                              std::vector<Task*>& callbacks) {
  std::lock_guard<std::mutex> guard(postTaskExecutionMutex);
  if (!task->noPrefetch) {
    for (const auto& p : task->params) {
      Data* d = p.first;
      AccessMode mode = p.second;
      assert(d->refCount > 0);
      assert(!d->swapped);
      d->refCount--;
      if (mode == AccessMode::WRITE) {
        d->dirty = true;
        dataSize -= d->oldSize;
        d->oldSize = d->size();
        dataSize += d->oldSize;
      }
    }
    dataSizeRecorder.record(dataSize);
  }

  tasksLeft--;
  for (int successor : succ[task->index].successors) {
    if (--succ[successor].count == 0) {
      Task* s = tasks[successor];
      startPrefetch(s);
      if (s->isCallback) {
        callbacks.push_back(s);
      } else {
        availableTasks->push(s);
      }
    }
  }
  // Re-insert in the LRU only now to avoid removing and re-inserting things
  // that will be used in one of the just-pushed task. We only insert data that
  // is not going to be used right away in the LRU.
  if (!task->noPrefetch) {
    std::lock_guard<std::mutex> guard(lruMutex);
    for (const auto& p : task->params) {
      Data* d = p.first;
      if ((d->refCount == 0) && d->swappable) {
        lru.put(d);
      }
    }
  }
  evict();
  tasks[task->index] = NULL;
  delete task;
  if (!tasksLeft) {
    // We are processing the last task post-execution hook. This means that no
    // other tasks are waiting for execution / post-execution, and we can safely
    // tell the workers to stop.
    stopAllWorkers();
  }
  notifyProgress();
}

void TaskScheduler::notifyProgress() {
  if ((tasksLeft == nextTaskCountWakeup) || tasksLeft == 0) {
    std::unique_lock<std::mutex> lock(conditionMutex);
    nextTaskCountWakeup -= std::max((int) ((percentageFrequency / 100.)
                                           * totalTasks), 1);
    condition.notify_one();
  }
}

void TaskScheduler::go(int n) {
  recorder.tag("Prepare");
  prepare();
  recorder.tag("Go");

  writtenDataRecorder.record(0);
  readDataRecorder.record(0);

  std::thread ioThread;
  std::thread mpiThread;
  if (size != 1) {
    MpiRequestPool& mpi = MpiRequestPool::getInstance();
    // std::ref to avoid a copy.
    mpiThread = std::thread(&MpiRequestPool::mainLoop, std::ref(mpi));
  }
  IoThread& io = IoThread::getInstance();
  ioThread = std::thread(&IoThread::mainLoop, std::ref(io));

  if (workers.size() != 0) {
    assert(workers.size() == n);
  } else {
    workerIds = std::vector<std::thread::id>(n);
    workers.reserve(n);
    for (int i = 0; i < n; i++) {
      workers.emplace_back(*availableTasks, *this, workerIds[i]);
    }
  }

  workerThreads.clear();
  for (int i = 0; i < n; i++) {
    workerThreads.emplace_back(&Worker::mainLoop, std::ref(workers[i]));
  }

  while (tasksLeft != 0) {
    std::unique_lock<std::mutex> lock(conditionMutex);
    condition.wait(lock);
    if (callback) {
      callback(tasksLeft, totalTasks);
    }
  }

  for (auto& t : workerThreads) {
    t.join();
  }

  if (size != 1) {
    MpiRequestPool& mpi = MpiRequestPool::getInstance();
    mpi.pleaseStop();
    mpiThread.join();
  }
  io.pleaseStop();
  ioThread.join();
  recorder.tag("Done");

  // Reset the scheduler.
  workerThreads.clear();
  dataAccess.clear();
  deps.clear();
  availableTasks->clear();
  succ.clear();
  for (auto t : tasks) {
    // Check that all tasks have been deleted.
    assert(!t);
  }
  tasks.clear();
  tasksLeft = 0;
  totalTasks = 0;

  dataSizeRecorder.toFile("data_size.txt");
  writtenDataRecorder.toFile("data_written.txt");
  readDataRecorder.toFile("data_read.txt");
}

int TaskScheduler::currentId() const {
  const int n = (int) workerIds.size();
  auto id = std::this_thread::get_id();
  for (int i = 0; i < n; i++) {
    if (id == workerIds[i]) {
      return i;
    }
  }
  return -1;
}

void TaskScheduler::dumpTimeline(const char* filename) const {
  int64_t minTime = (((uint64_t) 1) << 63) - 1;
  for (const Worker& w : workers) {
    minTime = std::min(w.timeline.minTime(), minTime);
  }

  std::ofstream f(filename);
  bool firstTime = true;
  f << "[" << std::endl;
  for (const Worker& w : workers) {
    if (!firstTime) {
      f << ", " << std::endl;
    }
    firstTime = false;
    w.timeline.jsonToStream(f, minTime);
  }
  f << "]" << std::endl;
}

void TaskScheduler::unregisterData(Data* d) {
  auto it = dataAccess.find(d);
  if (it != dataAccess.end()) {
    dataAccess.erase(it);
  }
}
