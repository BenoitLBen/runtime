#include "config.h"
#include "disk.hpp"

#include <cstdio>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <cassert>
#include <cstring>
#include <sys/stat.h>

#include "data.hpp"
#include "dependencies.hpp"


/** Abstract Base class for an IO backend.
 */
class IoBackend {
public:
  IoBackend() {};
  virtual ~IoBackend() {};
  virtual void writeData(Data* d) = 0;
  virtual void readData(Data* d) = 0;
  virtual void deleteData(Data* d) = 0;
};


namespace {
class FlushTask : public Task {
private:
  Data* d;

public:
  FlushTask(Data* d) : Task("Flush"), d(d) {
    isCallback = true;
    noPrefetch = true;
  }
  void call() {
    std::lock_guard<std::mutex> guard(TaskScheduler::getInstance().lruMutex);
    TaskScheduler::getInstance().lru.putAsOldest(d);
  }
};

}

/** File IO Backend.
 */
class FileIoBackend : public IoBackend {
private:
  int index;
  char* basedirName;
  std::map<Data*, char*> dataToFilename;

private:
  void createBaseDirectory(const char* directory) {
    const char* pattern = "/toyrt_ooc_XXXXXX";
    basedirName = (char*) calloc(strlen(directory) + strlen(pattern) + 1, 1);
    assert(basedirName);
    sprintf(basedirName, "%s%s", directory, pattern);
    assert(mkdtemp(basedirName));
  }

  void cleanup() {
    // TODO: remove the directory hierarchy as well
    for (auto p : dataToFilename) {
      remove(p.second);
      free(p.second);
    }
    dataToFilename.clear();
  }

  enum FileAccessMode {READ, WRITE};
  FILE* openFile(Data* d, FileAccessMode access) {
    FILE* f = NULL;
    switch (access) {
    case READ:
      assert(dataToFilename.find(d) != dataToFilename.end());
      f = fopen(dataToFilename[d], "rb");
      break;
    case WRITE:
      {
        const int kFilesPerDir = 1000;
        auto it = dataToFilename.find(d);
        if (it == dataToFilename.end()) {
          if (index % kFilesPerDir == 0) {
            char* dirName = (char*) calloc(strlen(basedirName) + 10, 1);
            sprintf(dirName, "%s/%04d", basedirName, index / kFilesPerDir);
            int ierr = mkdir(dirName, S_IRWXU);
            assert(!ierr);
            free(dirName);
          }
          char* filename = (char*) calloc(strlen(basedirName) + 30, 1);
          assert(filename);
          sprintf(filename, "%s/%04d/%06d", basedirName,
                  index / kFilesPerDir, index);
          dataToFilename[d] = filename;
          index++;
        }
        f = fopen(dataToFilename[d], "wb");
        assert(f);
      }
      break;
    }
    return f;
  }

  void deleteData(Data* d) {
    if (dataToFilename.find(d) != dataToFilename.end()) {
      remove(dataToFilename[d]);
    }
  }

public:
  FileIoBackend(const char* directory = "/tmp") : index(0) {
    createBaseDirectory(directory);
  }
  ~FileIoBackend() {
    cleanup();
  }
  void writeData(Data* d) {
    FILE* f = openFile(d, WRITE);
    assert(f);
    void* ptr;
    // TODO: register alloc
    ssize_t size = d->pack(&ptr);
    size_t written = fwrite(ptr, 1, size, f);
    assert(written == size);
    fclose(f);
    // TODO: register free
    free(ptr);
  }
  void readData(Data* d) {
    FILE* f = openFile(d, READ);
    // Get the size
    fseek(f, 0L, SEEK_END);
    size_t size = ftell(f);
    rewind(f);
    // TODO: register alloc
    void* ptr = calloc(size, 1);
    assert(ptr);
    size_t readSize = fread(ptr, 1, size, f);
    assert(readSize == size);
    fclose(f);
    d->unpack(ptr, size);
    // TODO: register free
    free(ptr);
  }
};


void IoThread::pushSwap(Data* d) {
  std::lock_guard<std::mutex> lock(requestsMutex);
  assert(d->swappable);
  assert(!d->swapped);
  d->swapped = true;
  requests.push_front(new Request(WRITE, d));
  sleepConditionIO.notify_one();
}

void IoThread::pushPrefetch(Data* d) {
  std::lock_guard<std::mutex> lock(requestsMutex);
  assert(d->swappable);
  assert(d->swapped);
  requests.push_front(new Request(READ, d));
  sleepConditionIO.notify_one();
}

void IoThread::pleaseStop() {
  std::lock_guard<std::mutex> lock(requestsMutex);
  requests.push_front(NULL);
  sleepConditionIO.notify_one();
}

void IoThread::processRequest(Request* r) {
  switch (r->type) {
  case READ:
    {
      // Prefetch
      backend->readData(r->d);
      r->d->swapped = false;
      r->d->dirty = false;
      r->d->prefetchInFlight = false;
    }
    break;
  case WRITE:
    {
      // Swap
      if (r->d->dirty) {
        backend->writeData(r->d);
        r->d->dirty = false;
      }
      r->d->deallocate();
    }
    break;
  case DELETE:
    backend->deleteData(r->d);
    break;
  }
}

void IoThread::mainLoop() {
  myId = std::this_thread::get_id();
  {
    DECLARE_CONTEXT;

  bool shouldStop = false;
  bool shouldReallyStop = false;
  while (!shouldReallyStop) {
    std::unique_lock<std::mutex> lock(requestsMutex);
    bool nothingToDo = requests.empty();
    if (nothingToDo && (!shouldStop)) {
      sleepConditionIO.wait(lock);
    }
    shouldReallyStop = shouldStop && nothingToDo;
    while (!requests.empty()) {
      Request* r = requests.back();
      requests.pop_back();
      lock.unlock();
      if (!r) {
        // The "stop" sentinel should be the last request
        assert(requests.empty());
        shouldStop = true;
      } else {
        processRequest(r);
      }
      lock.lock();
    }
    lock.unlock();
  }
  }
  myId = static_cast<std::thread::id>(0);
}

IoThread::IoThread() {
  backend = new FileIoBackend();
}

IoThread::~IoThread() {
  delete backend;
}

void flushToDisk(Data* d) {
  TaskScheduler::getInstance().insertTask(new FlushTask(d), {{d, toyRT_WRITE}});
}
