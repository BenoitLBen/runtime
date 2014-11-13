#ifndef _DISK_HPP
#define _DISK_HPP
#include <mutex>
#include <condition_variable>
#include <list>
#include <map>

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
  std::condition_variable sleepCondition;
  IoBackend* backend;

public:
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


void flushToDisk(Data* d);
#endif
