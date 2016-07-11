#ifndef _DATA_HPP
#define _DATA_HPP

class Data {
public:
  // Don't touch these
  int rank;
  int tag;
  size_t oldSize;
  // OOC
  /*! \brief Number of usage of this data in the tasks allready posted & ready for execution
     */
  int refCount;

  /*! \brief true if the data is on disk or if a write request has been pushed, false if the data is incore.
     */
  bool swapped;

  /*! \brief true if the version of this data currently stored on disk is garbage, false if it is simiral to the version incore.
     */
  bool dirty;

  /*! \brief true if a read requesthas been posted, false otherwise.
     */
  bool prefetchInFlight;

  // You can touch these
  /*! \brief  Can the runtime offload this data to disk
     */
  bool swappable;
public:
  Data() : rank(-1), tag(-1), oldSize(0), refCount(0), swapped(false),
           dirty(true), prefetchInFlight(false), swappable(false) {}
  virtual ~Data() {}
  /** Put the data into a contiguous buffer.

      There are two ways to call this function:

      - If ptr == NULL, then no packing is done, and only the size of the buffer
        required to hold the data is returned.
      - Otherwise, a new buffer of size count is allocated using malloc(), the
        data are copied into it, and its size is returned.

      @param ptr Pointer to a buffer allocated by the function.
      @return count Buffer size in bytes
   */
  virtual ssize_t pack(void** ptr) = 0;
  /** Deserialize the data from the buffer \a ptr.

      @param ptr Buffer containing the data
      @param count Buffer size in bytes
   */
  virtual void unpack(void* ptr, ssize_t count) = 0;
  /** Deallocate the data on this node.

      This does not destroy the data.
   */
  virtual void deallocate() = 0;
  /** Return the size of the data in bytes.
   */
  virtual size_t size() = 0;
};
#endif
