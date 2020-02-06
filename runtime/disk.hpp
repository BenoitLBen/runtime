#pragma once
#include <mutex>
#include <condition_variable>
#include <list>
#include <map>
#include <thread>

#include "task.hpp"

class DiskReadTask;
class DiskWriteTask;
class IoBackend;


class IoThread {
private:
  enum RequestType {READ, WRITE, DELETE};
  struct Request {
    RequestType type;
    Data* d;
    // No task associated with the request: for swapping requests
    Request(RequestType type, Data* d) : type(type), d(d) {}
  };

  std::list<Request*> requests;
  std::mutex requestsMutex;
  /** Used to sleep if no request is to be processed by the IO thread. */
  std::condition_variable sleepConditionIO;
  IoBackend* backend;

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
  void processRequest(Request* r);
  IoThread();
  IoThread(const IoThread&) {}
  ~IoThread();
};

/*! \brief Put d in the position of being the next data evicted out of memory when room is needed.
   */
void flushToDisk(Data* d);
