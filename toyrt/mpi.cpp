#include "config.h"

#include <mpi.h>
#include <list>
#include <mutex>

#include "context/memory_instrumentation.hpp"
#include "data.hpp"
#include "dependencies.hpp"
#include "mpi.hpp"

#include <iostream>

namespace {
class DeallocateDataTask : public Task {
 private:
  Data* d;

 public:
  DeallocateDataTask(Data* d) : Task("Deallocate"), d(d) { isCallback = true; }
  void call() { d->deallocate(); }
};
}  // namespace

MpiDataCache::iterator_t MpiDataCache::find(Data* d) {
  iterator_t it = validOnNode.find(d);
  if (it == validOnNode.end()) {
    auto tmp = std::vector<bool>(size, false);
    // The data is always valid on its parent node
    tmp[d->rank] = true;
    validOnNode[d] = tmp;
    it = validOnNode.find(d);
  }
  return it;
}

MpiDataCache::MpiDataCache() : validOnNode(), enabled(true) {
  rank = TaskScheduler::getInstance().getMpiRank();
  size = TaskScheduler::getInstance().getMpiSize();
}

void MpiDataCache::eraseData(Data* d) {
  iterator_t it = validOnNode.find(d);
  if (it != validOnNode.end()) {
    validOnNode.erase(it);
  }
}

void MpiDataCache::sendData(Data* d, int from, int to) {
  (void)from;
  auto it = find(d);
  assert(it->second[from]);  // Check that the data is valid on 'from'
  it->second[to] = true;     // Mark the data as valid on 'to'
}

void MpiDataCache::invalidateData(Data* d, int exceptOnNode) {
  auto it = find(d);
  if (it->second[rank] && (rank != d->rank) && (rank != exceptOnNode)) {
    TaskScheduler::getInstance().insertTask(new DeallocateDataTask(d),
                                            {{d, toyRT_WRITE}}, Priority::HIGH);
  }
  bool tmp;
  if (exceptOnNode != -1) {
    tmp = it->second[exceptOnNode];
  }
  it->second.assign(size, false);
  it->second[d->rank] = true;
  if (exceptOnNode != -1) {
    it->second[exceptOnNode] = tmp;
  }
}

void MpiDataCache::invalidateAll() {
  for (auto p : validOnNode) {
    invalidateData((Data*)p.first);
  }
}

bool MpiDataCache::isValid(Data* d) { return isValidOnNode(d, rank); }

bool MpiDataCache::isValidOnNode(Data* d, int node) {
  auto it = find(d);
  return it->second[node];
}

void MpiRequestPool::pushSend(MpiSendTask* task) {
  std::lock_guard<std::mutex> lock(pendingMutex);
  assert(task->d->tag != 0);
  if (TaskScheduler::getInstance().verbose())
    printf("%s push %s\n",
           TaskScheduler::getInstance().getLocalization().c_str(),
           task ? task->description().c_str() : "NULL");
  pending.push_front(new Request(SEND, task));
  sleepConditionMPI.notify_one();
}

void MpiRequestPool::pushRecv(MpiRecvTask* task) {
  std::lock_guard<std::mutex> lock(pendingMutex);
  assert(task->d->tag != 0);
  if (TaskScheduler::getInstance().verbose())
    printf("%s push %s\n",
           TaskScheduler::getInstance().getLocalization().c_str(),
           task ? task->description().c_str() : "NULL");
  pending.push_front(new Request(RECV, task));
  sleepConditionMPI.notify_one();
}

void MpiRequestPool::displayDetachedRequests() const {
  for (Request* r : detached) {
    std::cout << (r->type == SEND ? "SEND" : "RECV") << "(" << r->ptr << ", "
              << r->count << ", fromto = " << r->from << ", tag = " << r->d->tag
              << ")" << std::endl;
  }
}

std::list<MpiRequestPool::Request*>::iterator
MpiRequestPool::processCompletedRequest(std::list<Request*>::iterator it) {
  Request* r = *it;

  switch (r->type) {
    case RECV: {
      MpiRecvTask* t = (MpiRecvTask*)r->task;
      // 2 cases: received the size or the data.
      if (!r->sizeReqDone) {
        r->sizeReqDone = true;
        assert(r->d->tag != 0);
        recvData.record(r->count);
        // Only the size has been received, do a new MPI_Irecv() for the data.
        // The size is now in t->count;
        r->ptr = calloc(1, r->count);
        REGISTER_ALLOC(r->ptr, r->count);
        // std::cout << "RECV(" << r->count << ", from = " << r->from
        //           << ", tag = " << r->d->tag << ")" << std::endl;
        int ierr =
            MPI_Irecv(r->ptr, (int)r->count, MPI_BYTE, r->from, r->d->tag,
                      TaskScheduler::getInstance().getMpiComm(), &r->req);
        assert(!ierr);
        // Don't remove the query.
        // TODO: should we bump the iterator ? If we don't, this very query will
        // be tested right away.
        ++it;
      } else {
        assert(t->d->tag != 0);
        // We received the data, need to unpack and free the deps.
        r->d->unpack(r->ptr, r->count);
        REGISTER_FREE(r->ptr, r->count);
        free(r->ptr);
        TaskScheduler& s = TaskScheduler::getInstance();
        s.postTaskExecution(t);
        it = detached.erase(it);
        delete r;
      }
    } break;
    case SEND: {
      MpiSendTask* t = (MpiSendTask*)r->task;
      if (!r->sizeReqDone) {
        r->sizeReqDone = true;
        ssize_t count = r->d->pack(&r->ptr);
        (void)count;
        assert(count == r->count);
        REGISTER_ALLOC(r->ptr, r->count);
        sentData.record(r->count);
        int ierr =
            MPI_Isend(r->ptr, (int)r->count, MPI_BYTE, r->to, r->d->tag,
                      TaskScheduler::getInstance().getMpiComm(), &r->req);
        assert(!ierr);
        ++it;
      } else {
        REGISTER_FREE(r->ptr, r->count);
        free(r->ptr);
        TaskScheduler& s = TaskScheduler::getInstance();
        s.postTaskExecution(t);
        it = detached.erase(it);
        // Mark that this (to, tag) pair is now free
        auto p = std::make_pair(r->to, r->d->tag);
        sendsInFlight.erase(p);
        // If there is a request waiting, process it now.
        auto it = waiting.find(p);
        if (it != waiting.end()) {
          Request* r = it->second.front();  // What if r is NULL ?
          it->second.pop_front();
          pushDetachedRequest(r);
        }
        delete r;
      }
    } break;
  }
  return it;
}

void MpiRequestPool::testDetachedRequests() {
  MPI_Status status;
  auto it = detached.begin();
  while (it != detached.end()) {
    Request* r = *it;
    int flag;
    int ierr = MPI_Test(&r->req, &flag, &status);
    assert(!ierr);
    // Request has completed
    if (flag) {
      it = processCompletedRequest(it);
    } else {
      ++it;
    }
  }
}

void MpiRequestPool::pushDetachedRequest(Request* r) {
  int ierr;
  r->sizeReqDone = false;
  switch (r->type) {
    case SEND: {
      assert(r->d->tag != 0);
      // Only permit one concurrent send to the same (to, tag) pair. Additionnal
      // requests are put on a waiting queue.
      auto p = std::make_pair(r->to, r->d->tag);
      if (sendsInFlight.find(p) != sendsInFlight.end()) {
        waiting[p].push_back(r);
      } else {
        sendsInFlight.insert(p);
        // std::cout << "SEND(" << r->count << ", to = " << r->to << ", tag = "
        //           << r->d->tag << ")" << std::endl;
        ierr =
            MPI_Issend(&r->count, 1, MPI_UNSIGNED_LONG_LONG, r->to, r->d->tag,
                       TaskScheduler::getInstance().getMpiComm(), &r->req);
        assert(!ierr);
      }
    } break;
    case RECV: {
      // std::cout << "RECV(" << ", from = " << r->from << ", tag = "
      //           << r->d->tag << ")" << std::endl;
      ierr = MPI_Irecv(&r->count, 1, MPI_UNSIGNED_LONG_LONG, r->from, r->d->tag,
                       TaskScheduler::getInstance().getMpiComm(), &r->req);
      assert(!ierr);
    } break;
  }
  detached.push_back(r);
}

void MpiRequestPool::mainLoop() {
  myId = std::this_thread::get_id();
  {
    DECLARE_CONTEXT;
    int initialized;
    MPI_Initialized(&initialized);
    if (!initialized) {
      int provided;
      MPI_Init_thread(NULL, NULL, MPI_THREAD_SERIALIZED, &provided);
    }

    bool shouldStop = false;  // Moves to true when a NULL task is poped
    bool shouldReallyStop = shouldStop;
    while (!shouldReallyStop) {
      std::unique_lock<std::mutex> lock(pendingMutex);
      bool nothingToDo = pending.empty() && detached.empty();
      if (nothingToDo && (!shouldStop)) {
        sleepConditionMPI.wait(lock);
      }
      shouldReallyStop = shouldStop && nothingToDo;
      while (!pending.empty()) {
        Request* r = pending.back();
        pending.pop_back();
        if (!r) {
          // The "stop" sentinel should be the last request
          assert(pending.empty());
          shouldStop = true;
        }
        lock.unlock();
        if (r) {
          pushDetachedRequest(r);
        }
        lock.lock();
      }
      lock.unlock();
      testDetachedRequests();
    }
  }
  myId = static_cast<std::thread::id>(0);
}

void MpiRequestPool::pleaseStop() {
  // Convention: If the pushed request is NULL, we should stop.
  std::unique_lock<std::mutex> lock(pendingMutex);
  pending.push_front(NULL);
  sleepConditionMPI.notify_one();
}

MpiRequestPool::MpiRequestPool() {
  rank = TaskScheduler::getInstance().getMpiRank();
  size = TaskScheduler::getInstance().getMpiSize();
}

MpiRequestPool::~MpiRequestPool() {
  char filename[256];
  sprintf(filename, "recv-%03d.txt", rank);
  recvData.toFile(filename);
  sprintf(filename, "send-%03d.txt", rank);
  sentData.toFile(filename);
}

void MpiSendTask::call() {
  assert(d->tag != 0);
  count = d->pack(NULL);
  MpiRequestPool::getInstance().pushSend(this);
}

void MpiRecvTask::call() {
  assert(d->tag != 0);
  MpiRequestPool::getInstance().pushRecv(this);
}
