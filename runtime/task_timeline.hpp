#ifndef _TASK_TIMELINE_HPP
#define _TASK_TIMELINE_HPP
#include <deque>
#include <string>
#include <chrono>
#include <ostream>

/** Create a timeline of the task executions.
*/
class TaskTimeline {
private:
  typedef std::chrono::time_point<std::chrono::high_resolution_clock> TimePoint;
  typedef std::chrono::nanoseconds nanos;
  /** Represent a single task execution.
   */
  struct TaskExecution {
    /** Task name, task::name */
    std::string name;
    /** Start and stop time of the task execution, in ns since Epoch */
    int64_t start, stop; // Be careful ! It will overflow in 2262.
    /** Optional data about the task instance, as returned by task.extraData(). */
    std::string extraData;

    TaskExecution(const std::string& name, TimePoint _start, TimePoint _stop,
                  const std::string& _extraData)
      : name(name), extraData(_extraData) {
      start = std::chrono::duration_cast<nanos>(_start.time_since_epoch()).count();
      stop = std::chrono::duration_cast<nanos>(_stop.time_since_epoch()).count();
    };
  };

  /** Timeline */
  std::deque<TaskExecution> timeline;

public:
  /** Add a task to the timeline.

   @param name Task name
   @param start start time
   @param end end time
   @param extraData as returned by task::extraData
  */
  void addTask(const std::string name, TimePoint start, TimePoint stop, std::string extraData) {
    timeline.emplace_back(TaskExecution(name, start, stop, extraData));
  }

  /** Dump the JSON timeline data to a stream.

      The JSON is formatted as follows:
      [{"name": "TaskName", "start": startTimeInNsFromTimeOffset, "stop": ...,
        "extraData": ..., ...]

        @param os Output stream
        @param timeOffset time offset in ns since Epoch
   */
  void jsonToStream(std::ostream& os, int64_t timeOffset) const {
    os << "[" << std::endl;
    std::string prefix("");
    for (size_t i = 0; i < timeline.size(); i++) {
      const auto& task = timeline[i];
      os << prefix << "{"
         << "\"name\": \"" << task.name << "\", "
         << "\"start\": " << task.start - timeOffset << ", "
         << "\"stop\": " << task.stop - timeOffset;
      if (task.extraData.size() > 0) {
        os << ", \"extraData\": " << task.extraData;
      }
      os << "}";
      prefix = ", ";
    }
    os << "]" << std::endl;
  }

  /** Return min(t.start | t in timeline).

      This assumes that the tasks have been added in order.
   */
  int64_t minTime() const {
    return (timeline.size() == 0 ? 0 : timeline[0].start);
  }
};
#endif
