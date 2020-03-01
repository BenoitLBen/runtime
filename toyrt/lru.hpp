#pragma once

#include <map>
#include <unordered_map>

/** Limited LRU (Least Recently Used) data structure.

    This data structure allows one to track the least recently used item in a
    collection. Note that contrary to the usual LRU caches, there is no
    distinction between the key and the value, and we only store pointers.

    The asympotic complexity of the operations are :
    - Insertion / refresh : O(1) + O(log(N)) for N items in the LRU.
    - remove: O(1) + O(log(N))
    - removeOldest: O(1)

    @warning this class is not thread-safe, don't do insertions / removal at the
    same time from different threads.
*/
template <typename V>
class Lru {
 private:
  int64_t current;
  std::unordered_map<V*, int64_t> toSequence;
  std::map<int64_t, V*> fromSequence;

 public:
  Lru() : current(0), toSequence(), fromSequence() {}

 private:
  void put(V* v, bool oldest) {
    // If the value is already here with a different sequence number, remove the
    // mapping with the old sequence number.
    auto toIt = toSequence.find(v);
    if (toIt != toSequence.end()) {
      auto fromIt = fromSequence.find(toIt->second);
      fromSequence.erase(fromIt);
    }
    int64_t sign = oldest ? -1 : 1;
    int64_t sequence = sign * current;
    toSequence[v] = sequence;
    fromSequence[sequence] = v;
    ++current;
  }

 public:
  /** Insert or refresh a data in the cache.

      If a value is not already in the cache, add it as the most recently used
      value. Otherwise, mark it as the most recently used value.

      @param v the value to insert/refresh
   */
  void put(V* v) { put(v, false); }

  /** Mark a data as the oldest one in the LRU.

      If a value is not already in the cache, add it as the least recently used
      value. Otherwise, mark it as the least recently used value.

      @param v the value to insert/refresh
   */
  void putAsOldest(V* v) { put(v, true); }
  /** Remove a value from the LRU, if it exists.

      @param v value to remove
      @return true if the value exists in the LRU, false otherwise
   */
  bool remove(V* v) {
    auto toIt = toSequence.find(v);
    if (toIt == toSequence.end()) {
      return false;
    }
    fromSequence.erase(fromSequence.find(toIt->second));
    toSequence.erase(toIt);
    return true;
  }
  /** Return true if v is in the LRU.
   */
  bool contains(V* v) const { return toSequence.find(v) != toSequence.end(); }
  /** Remove and return the oldest value, if one exists.

      @return the value if the LRU is not empty, NULL otherwise.
  */
  V* removeOldest() {
    auto it = fromSequence.begin();
    if (it == fromSequence.end()) {
      return NULL;
    }
    V* result = it->second;
    toSequence.erase(it->second);
    fromSequence.erase(it->first);
    return result;
  }
};
