#ifndef _DATA_RECORDER_HPP
#define _DATA_RECORDER_HPP
#include <chrono>
#include <algorithm>
#include <deque>
#include <string>
#include <mutex>
#include <fstream>

template<typename T, typename U>
std::ostream& operator<<(std::ostream& os, const std::pair<T, U>& p) {
  os << p.first << " " << p.second;
  return os;
}

/** Helper class to record arbitrary timed data to a file.

    Data points are provided through the \a record() method adding a timestamped
    record to an in-memory array. It can then be dumped to a text file, provided
    that the data point has a suitable operator<<() defined.
    The user can also record tags, which are timestamped strings, saved to a
    different file.

    The time delta are relative to the first record() call.
 */
template<typename T> class TimedDataRecorder {
  typedef std::chrono::time_point<std::chrono::high_resolution_clock> TimePoint;
  typedef std::pair<TimePoint, T> DataPoint;
  typedef std::pair<TimePoint, std::string> Tag;
  std::deque<DataPoint> data;
  std::deque<Tag> tags;
  std::mutex mutex;

public:
  /** Add a data point to the record.

      \param value data point
   */
  void record(const T& value) {
    TimePoint now = std::chrono::high_resolution_clock::now();
    data.push_back(std::make_pair(now, value));
  }

  /** Synchronized version of \a TimedDataRecorder::record().
   */
  void recordSynchronized(const T& value) {
    std::lock_guard<std::mutex> guard(mutex);
    record(value);
  }

  void tag(const std::string& tag) {
    TimePoint now = std::chrono::high_resolution_clock::now();
    tags.push_back(std::make_pair(now, tag));
  }

  /** Synchronized version of \a TimedDataRecorder::tag().
   */
  void tagSynchronized(const T& value) {
    std::lock_guard<std::mutex> guard(mutex);
    tag(value);
  }

  /** Dump the timestamped records to a file.

      The format is:
      timestamp_in_ns<delimiter>record\n

      \param filename Name of the output file. The markers are put in filename.tags
      \param delimiter field delimiter, space by default
   */
  void toFile(const char* filename, const char* delimiter=" ") {
    typedef std::chrono::nanoseconds nanos;
    if (data.size() == 0 && tags.size() == 0) return;
    bool hasData = data.size() > 0;
    bool hasTags = tags.size() > 0;
    auto origin = TimePoint();
    if (hasData && hasTags) {
      origin = std::min(tags[0].first, data[0].first);
    } else if (hasData) {
      origin = data[0].first;
    } else if (hasTags) {
      origin = tags[0].first;
    }
    {
      std::ofstream file(filename);
      for (auto it : data) {
        size_t offsetNano =
          std::chrono::duration_cast<nanos>((it.first - origin)).count();
        file << std::scientific << offsetNano << delimiter << it.second << "\n";
      }
    }
    {
      std::string name(filename);
      name += ".tags";
      std::ofstream file(name.c_str());
      for (auto it : tags) {
        size_t offsetNano =
          std::chrono::duration_cast<nanos>((it.first - origin)).count();
        file << std::scientific << offsetNano << delimiter << it.second << "\n";
      }
    }
  }
};
#endif
