
// The problem is to implement a file cache in C++ that derives the interface
// given below in class FileCache. The typical usage is for a client to call
// 'PinFiles()' to pin a bunch of files in the cache and then either read or
// write to their in-memory contents in the cache. Writing to a cache entry
// makes that entry 'dirty'. Before a dirty entry can be evicted from the
// cache, it must be unpinned and has to be cleaned by writing the
// corresponding data to storage.
//
// All files are assumed to have size 10KB. If a file doesn't exist to begin
// with, it should be created and filled with zeros - the size should be 10KB.
//
// FileCache should be a thread-safe object that can be simultaneously
// accessed by multiple threads. If you are not comfortable with concurrent
// programming, then it may be single-threaded (see alternative in the
// PinFiles() comment). To implement the problem in its entirety may require
// use of third party libraries and headers. For the sake of convenience, it
// is permissible (although not preferred) to substitute external functions
// with stub implementations but, in doing so, please be clear what the
// intended behavior and side effects would be.
//


#ifndef _FILE_CACHE_H_
#define _FILE_CACHE_H_

#include <string>
#include <vector>

class FileCache {
 public:
  // Constructor. 'max_cache_entries' is the maximum number of files that can
  // be cached at any time.
  explicit FileCache(int max_cache_entries) :
    max_cache_entries_(max_cache_entries) {
  }

  // Destructor. Flushes all dirty buffers.
  virtual ~FileCache() {}

  // Pins the given files in vector 'file_vec' in the cache. If any of these
  // files are not already cached, they are first read from the local
  // filesystem. If the cache is full, then some existing cache entries may be
  // evicted. If no entries can be evicted (e.g., if they are all pinned, or
  // dirty), then this method will block until a suitable number of cache
  // entries becomes available. It is OK for more than one thread to pin the
  // same file, however the file should not become unpinned until both pins
  // have been removed.
  //
  // Is is the application's responsibility to ensure that the files may
  // eventually be pinned. For example, if 'max_cache_entries' is 5, an
  // irresponsible client may try to pin 4 files, and then an additional 2
  // files without unpinning any, resulting in the client deadlocking. The
  // implementation *does not* have to handle this.
  //
  // If you are not comfortable with multi-threaded programming or
  // synchronization, this function may be modified to return a boolean if
  // the requested files cannot be pinned due to the cache being full. However,
  // note that entries in 'file_vec' may already be pinned and therefore even a
  // full cache may add additional pins to files.
  virtual void PinFiles(const std::vector<std::string>& file_vec) = 0;

  // Unpin one or more files that were previously pinned. It is ok to unpin
  // only a subset of the files that were previously pinned using PinFiles().
  // It is undefined behavior to unpin a file that wasn't pinned.
  virtual void UnpinFiles(const std::vector<std::string>& file_vec) = 0;

  // Provide read-only access to a pinned file's data in the cache.
  //
  // It is undefined behavior if the file is not pinned, or to access the
  // buffer when the file isn't pinned.
  virtual const char *FileData(const std::string& file_name) = 0;

  // Provide write access to a pinned file's data in the cache. This call marks
  // the file's data as 'dirty'. The caller may update the contents of the file
  // by writing to the memory pointed by the returned value.
  //
  // Multiple clients may have access to the data, however the cache *does not*
  // have to worry about synchronizing the clients' accesses (you may assume
  // the application does this correctly).
  //
  // It is undefined behavior if the file is not pinned, or to access the
  // buffer when the file is not pinned.
  virtual char *MutableFileData(const std::string& file_name) = 0;

 protected:
  // Maximum number of files that can be cached at any time.
  const int max_cache_entries_;

 private:
  // Disallow copy and assign. Do *not* implement!
  FileCache(const FileCache&);
  FileCache& operator=(const FileCache&);
};

#endif  // _FILE_CACHE_H_
