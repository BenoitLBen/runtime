/*! \file
  \ingroup HMatrix
  \brief Memory Allocation tracking.
*/
#pragma once
// We use things from C++11, so test for the standard level.
#if (__cplusplus > 199711L) || defined(HAVE_CPP11)
#include <cstdint>
#include <string>

/*! \brief Memory Tracking.

  This system is only suited for the specific purpose of the \a HMatrix code.
 */
namespace mem_instr {
  /*! \brief Add an allocation to the tracking.

    \param size positive or negative integer, size in bytes.
   */
  void addAlloc(void* ptr, int64_t size);
  /*! \brief Dumps the data to filename.
   */
  void toFile(const std::string& filename);

  void enable();
  void disable();
}

#define REGISTER_ALLOC(ptr, size) mem_instr::addAlloc(ptr, +(size))
#define REGISTER_FREE(ptr, size) mem_instr::addAlloc(ptr, -(size))
#define MEMORY_INSTRUMENTATION_TO_FILE(filename) mem_instr::toFile(filename)
#define MEMORY_INSTRUMENTATION_ENABLE mem_instr::enable()
#define MEMORY_INSTRUMENTATION_DISABLE mem_instr::disable()
#else
#define REGISTER_ALLOC(ptr, size) do {} while (0)
#define REGISTER_FREE(ptr, size) do {} while (0)
#define MEMORY_INSTRUMENTATION_TO_FILE(filename) do {} while (0)
#define MEMORY_INSTRUMENTATION_ENABLE do {} while (0)
#define MEMORY_INSTRUMENTATION_DISABLE do {} while (0)
#endif // __cplusplus > 199711L
