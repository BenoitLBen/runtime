/*! \file
  \ingroup HMatrix
  \brief Context manager used for the tracing functionnality provided by MPF.
*/
#ifndef _CONTEXT_HPP
#define _CONTEXT_HPP

#include "config.h"

#if defined(__GNUC__)
#define MPF_FUNC __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
#define MPF_FUNC __FUNCTION__ // or perhaps __FUNCDNAME__ + __FUNCSIG__
#else
#define MPF_FUNC __func__
#endif

#if (__cplusplus > 199711L) || defined(HAVE_CPP11)
#include <chrono>
#include <vector>
#include <fstream>
#include <unordered_map>

#ifdef HAVE_STARPU
#include <starpu.h>
#endif

/** New StarPU-aware tracing contexts.

    A tracing tree root is associated with each parallel worker, plus one for the
    rest of the code (outside the parallel regions).
 */
namespace trace {
  typedef std::chrono::time_point<std::chrono::high_resolution_clock> Time;

  class NodeData {
  public:
    unsigned int n;
    int64_t totalTime; // ns
    int64_t totalFlops;
    int totalBytesSent;
    int totalBytesReceived;
    int64_t totalCommTime;
    Time lastEnterTime;
    Time lastCommInitiationTime;
  };

#ifdef HAVE_STARPU
#define MAX_ROOTS (STARPU_NMAXWORKERS + 1)
#else
  // 128 workers ought to be enough for anybody... or not
#define MAX_ROOTS 128
#endif

  /** Set the function tasked with returning the current root index.

      The function set this way is used to determine to which tree the current
      tracing context belongs to. By default, a single context is assumed, and
      this behavior can be restored by setting the fuction to NULL.

      \param nodeIndexFunc a function returning a worker index, which is:
        - 0 in non-parallel parts of the code
        - between 1 and n_workers (included) in parallel regions
   */
  void setNodeIndexFunction(int (*nodeIndexFunc)());

  class Node {
  public:
    /// True if the tracing is enabled. True by default.
    static bool enabled;
  private:
    /// Unique name for the context.
    const char* name;
    /// Tracing data associated with this node.
    NodeData data;
    /// Parent node. NULL for a root.
    Node* parent;
    /// Ordered list of children nodes.
    std::vector<Node*> children;
    /// List of trace trees.
    static std::unordered_map<void*, Node*> currentNodes[MAX_ROOTS]; // TODO: padding to avoid false sharing ?
    static void* enclosingContext[MAX_ROOTS]; // TODO: padding to avoid false sharing ?

  public:
    /** Enter a context noted by a name.
     */
    static void enterContext(const char* name);
    /** Leave the current context.
     */
    static void leaveContext();
    /** Get a unique reference to the current context. */
    static void* currentReference();
    /** Set the current as being relative to a given enclosong one.

        The enclosing context is identified through the pointer returned by \a
        currentReference().
     */
    static void setEnclosingContext(void* enclosing);
    static void enable() {enabled = true;}
    static void disable() {enabled = false;}
    static void incrementFlops(int64_t flops);
    static void startComm();
    static void endComm();
    /** Dumps the trace trees to a JSON file.
     */
    static void jsonDump(const char* filename);

  private:
    Node(const char* _name, Node* _parent);
    ~Node();
    Node* findChild(const char* name) const;
    void dump(std::ofstream& f) const;
    static Node* currentNode();
  };
}


class DisableContextInBlock {
private:
  bool enabled;
public:
  DisableContextInBlock() {
    enabled = trace::Node::enabled;
    trace::Node::disable();
  }
  ~DisableContextInBlock() {
    trace::Node::enabled = enabled;
  }
};

#define DISABLE_CONTEXT_IN_BLOCK DisableContextInBlock dummyDisableContextInBlock

#define tracing_set_worker_index_func(f) trace::setNodeIndexFunction(f)
#define enter_context(x) trace::Node::enterContext(x)
#define leave_context() trace::Node::leaveContext()
#define increment_flops(x) trace::Node::incrementFlops(x)
#define tracing_dump(x) trace::Node::jsonDump(x)

#else // C++11
#define tracing_set_worker_index_func(f) do {} while (0)
#define enter_context(x) do {} while(0)
#define leave_context()  do {} while(0)
#define increment_flops(x) do { (void)(x); } while(0)
#define tracing_dump(x) do {} while(0)
#define DISABLE_CONTEXT_IN_BLOCK do {} while (0)
#endif

/*! \brief Simple wrapper around enter/leave_context() to avoid
having to put leave_context() before each return statement. */
class Context {
public:
  Context(const char* name) {
    enter_context(name);
  }
  ~Context() {
    leave_context();
  }
};
#define DECLARE_CONTEXT Context __reserved_ctx((MPF_FUNC))
#endif
