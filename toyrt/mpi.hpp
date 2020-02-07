#pragma once

#include <mpi.h>

#include <list>
#include <cassert>
#include <unordered_map>
#include <map>
#include <condition_variable>
#include <deque>
#include <set>
#include <thread>

#include "task.hpp"
#include "context/data_recorder.hpp"


/** Submit a MPI Send().
 */
class MpiSendTask : public Task {
public:
  ssize_t count;
  int to;
  Data* d;

public:
  MpiSendTask(Data* d, int to) : Task("MpiSend"), count(0), to(to), d(d) {
    doPostExecution = false;
    isCallback = true;
  }
  void call();
};


/** Submit a MPI Recv().
 */
class MpiRecvTask : public Task {
  friend class MpiRequestPool;
private:
  int from;
  Data* d;

public:
  MpiRecvTask(Data* d, int from) : Task("MpiRecv"), from(from), d(d) {
    doPostExecution = false;
    isCallback = true;
  }
  void call();
};


/** MPI Data cache.

    This class is not the cache, it is just the tracking of which data are
    present in memory, and up to date.
    It is not a singleton, even though only one instance exists at a time.
 */
class MpiDataCache {
private:
  /** Rank in GlobalParallelSettings::getMpiComm() and size of it. */
  int rank, size;
  /** validOnNode[data][node] */
  std::unordered_map<Data*, std::vector<bool> > validOnNode;
  typedef std::unordered_map<Data*, std::vector<bool> >::iterator iterator_t;

public:
  bool enabled;

public:
  /** Create the cache tracking structures.
   */
  MpiDataCache();
  /** Remove a Data pointer from the map validOnNode

      This function must be called before deallocating the Data in unregister()
      or further call to invalidateData() might crash.

      @param d Data to remove
   */
  void eraseData(Data* d) ;
  /** Note that some data was sent to a node.

      This function must be called on all nodes.

      @param d Data that is sent
      @param from source node
      @param to destination node
   */
  void sendData(Data* d, int from, int to);
  /** Invalidate some data.

      Note that the data is invalid, except on two nodes:
      - The home node
      - The exceptOnNode node, if != -1

      This function also inserts a new task to free the memory
      (DeallocateDataTask) on the node that have a valid copy of \a d when this
      function is called.

      @param d the Data instance
      @param exceptOnNode Node that will keep the Data* as valid, if not -1
   */
  void invalidateData(Data* d, int exceptOnNode = -1);
  /** Invalidate all the data known to the cache.

      This function calls \a MpiDataCache::invalidateData(), so it also inserts
      the deallocation tasks.
   */
  void invalidateAll();
  /** Do we have a valid copy of some data locally ?

      @param d
   */
  bool isValid(Data* d);
  /** Does a node have a valid copy of some data ?

      @param d
      @param node the node we are interested in
   */
  bool isValidOnNode(Data* d, int node);

private:
  iterator_t find(Data* d);
};


/** MPI request pool and thread.

    All the MPI requests submitted by the runtime go through this class. This is
    required with some MPI implementations, as a single thread can issue MPI
    calls at a time.
*/
class MpiRequestPool {
private:
  /** MPI requests types */
  enum RequestType {SEND, RECV};
  /** MPI request caused by a task. */
  struct Request {
    RequestType type; ///< request type
    Task* task; ///< Task that caused it
    Data* d;
    void* ptr; ///< Pointer to the serialized data
    size_t count; ///< Size of the serialized data
    union {
      int from;
      int to;
    };
    MPI_Request req; ///< MPI request to test
    bool sizeReqDone; ///< Has the first request (size) already been done ?

    Request(RequestType type, Task* task) : type(type), task(task), ptr(NULL),
                                            req(), sizeReqDone(false) {
      switch (type) {
      case SEND:
        {
          MpiSendTask* t = static_cast<MpiSendTask*>(task);
          d = t->d;
          count = t->count;
          to = t->to;
        }
        break;
      case RECV:
        {
          MpiRecvTask* t = static_cast<MpiRecvTask*>(task);
          d = t->d;
          count = 0;
          from = t->from;
        }
        break;
      }
    }
  };

  /** List of currently running requests */
  std::list<Request*> detached;
  /** Locks the \a pending list. */
  std::mutex pendingMutex;
  /** Used to sleep if no request is to be processed by the MPI thread. */
  std::condition_variable sleepConditionMPI;
  /** List of requests to be submitted to MPI */
  std::list<Request*> pending;

  // Tracking of the in-flight requests.  These structures are only touched by
  // the MPI thread, thus requiring no synchonization.
  typedef std::pair<int, int> RequestId; // to, tag
  /** Current send requests in \a detached */
  std::set<RequestId> sendsInFlight;
  /** Requests that are waiting for a send to complete before being issued. */
  std::map<RequestId, std::deque<Request*> > waiting;

  /** rank and size in/of GlobalParallelSettings::getMpiComm() */
  int rank, size;

  /** Record the volume of sent and received data */
  TimedDataRecorder<size_t> sentData;
  TimedDataRecorder<size_t> recvData;

public:
  /** Cache mechanism */
  MpiDataCache cache;
  // thread id of the communication thread
  std::thread::id myId;

public:
  /** Enqueue a send.

      This is called by \a MpiSendTask::call().

      @param task
   */
  void pushSend(MpiSendTask* task);
  /** Enqueue a receive.

      This is called by \a MpiRecvTask::call().

      @param task
   */
  void pushRecv(MpiRecvTask* task);

private:
  /** Process a completed request.

      @param it an iterator inside the \a detached list.
   */
  std::list<Request*>::iterator
  processCompletedRequest(std::list<Request*>::iterator it);
  /** Test all the requests in the \a detached list.

      Calls \a processCompletedRequest() on completed requests.
   */
  void testDetachedRequests();
  /** Submit a request to MPI and put it into \a detached.
   */
  void pushDetachedRequest(Request* r);

public:
  /** MPI thread entry point.
   */
  void mainLoop();
  /** Ask the thread to stop (after finishing detached and pending requests).
   */
  void pleaseStop();

  /** Get the instance.
   */
  static MpiRequestPool& getInstance() {
    static MpiRequestPool mpi;
    return mpi;
  }

  /** Only for debugging.
   */
  void displayDetachedRequests() const;

private:
  MpiRequestPool();
  MpiRequestPool(const MpiRequestPool&) {} // No copy
  ~MpiRequestPool();
};

