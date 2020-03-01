/*! \file
  \ingroup HMatrix
  \brief Memory Allocation tracking.
*/
#include "memory_instrumentation.hpp"

#include "data_recorder.hpp"

#include <algorithm>

namespace mem_instr {
TimedDataRecorder<std::pair<void*, int64_t> > allocs;

/// True if the memory tracking is enabled.
static bool enabled = false;

void addAlloc(void* ptr, int64_t size) {
  if (!enabled) {
    return;
  }
  allocs.recordSynchronized(std::make_pair(ptr, size));
}

void toFile(const std::string& filename) { allocs.toFile(filename.c_str()); }

void enable() { enabled = true; }

void disable() { enabled = false; }
}  // namespace mem_instr
