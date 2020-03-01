#pragma once
#include <condition_variable>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

#include "task.hpp"

class DiskReadTask;
class DiskWriteTask;

/** Abstract Base class for an IO backend.
 */
class IoBackend {
 public:
  IoBackend() = default;
  virtual ~IoBackend() = default;
  virtual void writeData(Data* d) = 0;
  virtual void readData(Data* d) = 0;
  virtual void deleteData(Data* d) = 0;
};

class IoThread {
 private:
  enum RequestType { READ, WRITE, DELETE };
  struct Request {
    RequestType type;
    Data* d;
    // No task associated with the request: for swapping requests
    Request(RequestType type, Data* d) : type(type), d(d) {}
  };

  std::list<std::unique_ptr<Request>> requests;
  std::mutex requestsMutex;
  /** Used to sleep if no request is to be processed by the IO thread. */
  std::condition_variable sleepConditionIO;
  std::unique_ptr<IoBackend> backend;

 public:
  // thread id of the io thread
  std::thread::id myId;

  void pushSwap(Data* d);
  void pushPrefetch(Data* d);
  void mainLoop();
  void pleaseStop();
  static IoThread& getInstance() {
    static IoThread io;
    return io;
  }

 private:
  void enqueueRequest(std::unique_ptr<Request> r);
  void processRequest(Request* r);
  IoThread();
  IoThread(const IoThread&) = delete;
  ~IoThread() = default;
};

/*! \brief Put d in the position of being the next data evicted out of memory
 * when room is needed.
 */
void flushToDisk(Data* d);
