#include "file_cache_impl.h"
#include <stdexcept>
#include <errno.h>
#include <sstream>
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>


/* Notes:
 * Using system calls open(), read(), write() for file I/O to 
 * keep it unbuffered because we have our own buffers
 * in the cache and the individual reads/writes are 10k sized. 
 * Using std io library calls would cost us an extra memory copy. 
 */

/* Possible improvements
 * 1) Implementing the a LRU priority queue associated with the cache entries 
 *    so that the oldest of the eviction candidates are evicted from the cache when 
 *    the need arises. The timestamp/priority for each element is bumped up with 
 *    a read/write access to that element.
 * 2) Possibly access to the FileData and MutableFileData functions could be controlled 
 *    via shared_lock(mutex) mechanism because they do not change the composition of the 
 *    cache and PinFiles and UnpinFiles could be guarded by unique_lock(mutex). This would 
 *    form a reader/writer lock mechanism which allows shared access for readers. (Prima facie,
 *    this contradicts with the LRU improvement because readers would still need exclusive access 
 *    to update the priority queue).
 * 3) Better error monitoring and logging. I would like to add more public functions to the class which
 *    test for / provide querying ability for success/failure of the pin/unpin operations in relation to  
 *    the associated file system calls and enable the clients to verify if all their pin/unpin/flush 
 *    requests were satisfied and if some were not what were the errors for each failed file operation.  
 *    
 */

const char *
FileCacheImpl::FileData(const std::string& file_name)
{
    std::lock_guard<std::mutex> lock(m_);
    auto fitr = file_cache_.find(file_name);
    if (fitr == file_cache_.end()) {
        return nullptr;
    }
    return fitr->second.file_buf_.get();
}

char *
FileCacheImpl::MutableFileData(const std::string& file_name)
{
    std::lock_guard<std::mutex> lock(m_);
    auto fitr = file_cache_.find(file_name);
    if (fitr == file_cache_.end()) {
        return nullptr;
    }
    //Mark the cache as dirty
    fitr->second.dirty_ = true;
    return fitr->second.file_buf_.get();
}

/*evict_cache_entries
 * Input: Number of empty cache entries being sought for pinning new files by evicting 
 *        existing cache entries
 * Output: Number of cache entries actually evicted
 */
uint32_t
FileCacheImpl::evict_cache_entries(int num_cache_entries)
{
    /*Need to evict some entries from the cache
    * 1) The entries which are not dirty and not pinned can be just erased
    * 2) The entries which are dirty and not pinned need to be written 
    *    and then erased from the cache
    */     
    auto cache_entries_evicted = 0;
    for (auto fitr = file_cache_.begin(); fitr != file_cache_.end();) {
        if (fitr->second.pin_count_ == 0) {
            if (fitr->second.dirty_) {
                ::lseek(fitr->second.fd_, 0, SEEK_SET);
                int nbytes = ::write(fitr->second.fd_, 
                        fitr->second.file_buf_.get(), FILE_SIZE);
                if (nbytes < 0) {
                    //File write failed
                    std::ostringstream err_str;
                    err_str << "Error writing file " << fitr->first
                            << " : " << strerror(errno);
                    fprintf(stderr, "%s\n", err_str.str().c_str());
                }
            }
            
            file_cache_.erase(fitr++);
            cache_entries_evicted++;
            if (cache_entries_evicted == num_cache_entries) {
               //Enough entries evicted
                break;
            }
        } else {
            ++fitr;
        }
    }
    return cache_entries_evicted;
}

/*add_cache_entry
 * Input: filename to be added to the cache
 */
void
FileCacheImpl::add_cache_entry(const std::string& file_name)
{
    int fd = ::open(file_name.c_str(), O_RDWR | O_CREAT, 0777);
    if (fd < 0) {            
        //file open failed
        std::ostringstream err_str;            
        err_str << "Error opening file " << file_name
        << " : " << strerror(errno);
        fprintf(stderr, "%s\n", err_str.str().c_str());
        return;
    } else {
        //Read from the file and create a cache entry
        std::shared_ptr<char> buf(new char[FILE_SIZE]());
        memset(buf.get(), '0', FILE_SIZE);
        ::lseek(fd, 0, SEEK_SET);
        int nbytes = ::read(fd, buf.get(), FILE_SIZE);
        if (nbytes < 0) {
           //File read failed
           std::ostringstream err_str;
           err_str << "Error reading file " << file_name
                   << " : " << strerror(errno);
           fprintf(stderr, "%s\n", err_str.str().c_str());
           ::close(fd);
           return;
        } else {
           file_cache_.insert(std::make_pair(file_name, CacheEntry(buf, 1, fd)));
        }
    }
    return;
}

/*fill_up_cache fill all available cache entries 
 * Input: set of filenames not yet pinned to be added to the cache
 * As each entry entry from the input set gets pinned, it is removed from
 * the set
 */
void
FileCacheImpl::fill_up_cache(std::set<std::string>& files_not_pinned)
{
    int empty_cache_entries = max_cache_entries_ - file_cache_.size();
    //Fill up the cache    
    for (auto fnpitr = files_not_pinned.begin(); 
            (fnpitr != files_not_pinned.end()) && (empty_cache_entries > 0);) {
        add_cache_entry(*fnpitr);
        empty_cache_entries--; 
        files_not_pinned.erase(fnpitr++);
        if (files_not_pinned.empty()) {
            //all done,
            return;
        }
    }
    
}

void
FileCacheImpl::PinFiles(const std::vector<std::string>& file_vec)
{
    /* unique_lock needs to be used instead of lock_guard because
     * we may need to wait on condition variable
     */
    std::unique_lock<std::mutex> lock(m_);
    if (file_vec.size() > max_cache_entries_) {
        throw std::runtime_error("Number of files being pinned exceed cache size");
    }
    
    //Gather files not yet pinned from file_vec in this set
    std::set<std::string> files_not_pinned;
    //for files already pinned just increase the pin count
    for (auto file_name : file_vec) {
        auto fitr = file_cache_.find(file_name);
        if (fitr != file_cache_.end()) {
            fitr->second.pin_count_++;
        } else {
            files_not_pinned.insert(file_name);
        }
    }
    
    //Fill up the cache, if there are any entries available
    fill_up_cache(files_not_pinned);
    if (files_not_pinned.empty()) {
        //All done
        return;
    }
    
    //Cache full, need to evict some entries to proceed
    while (!files_not_pinned.empty()) {        
        while (!cache_entries_evictable()) {
            cv_.wait(lock);
        }
        //Check if any of the files we wish to pin got pinned while we were blocked
        for (auto fnpitr = files_not_pinned.begin(); 
                fnpitr != files_not_pinned.end();) {
            auto fitr = file_cache_.find(*fnpitr);
            if (fitr != file_cache_.end()) {
                fitr->second.pin_count_++;
                files_not_pinned.erase(fnpitr++);
            } else {
                ++fnpitr;
            }
        }
        //Try to pin the remaining ones
        if (!files_not_pinned.empty()) {
            auto cache_entries_evicted = evict_cache_entries(files_not_pinned.size());       
            assert(cache_entries_evicted <= files_not_pinned.size());
            fill_up_cache(files_not_pinned);
        }
    }
}

void
FileCacheImpl::UnpinFiles(const std::vector<std::string>& file_vec)
{
    std::lock_guard<std::mutex> lock(m_);
    bool cache_entry_evictable = false;
    for (auto file_name : file_vec) {
        auto fitr = file_cache_.find(file_name);
        if (fitr != file_cache_.end()) {
            // Deduct from the pin count, remove if it is 0
            fitr->second.pin_count_--;
            if (fitr->second.pin_count_ == 0) {
                cache_entry_evictable = true;
            }
        }
    }
    if (cache_entry_evictable) {
        //Wake up threads waiting to pin other files
        cv_.notify_all();
    }
}


FileCacheImpl::CacheEntry::~CacheEntry()
{ 
    if (pin_count_ == 0) {
        if (dirty_) {
            ::lseek(fd_, 0, SEEK_SET);
            int nbytes = ::write(fd_, file_buf_.get(), FILE_SIZE);
            if (nbytes < 0) {
                //File write failed
                std::ostringstream err_str;
                err_str << "Error writing file " 
                        << " : " << strerror(errno);
                fprintf(stderr, "%s\n", err_str.str().c_str());
            }
        }   
        //Cache entry is being flushed, close the fd if the pincount is zero
    
        /* We dont want to close fd_ without checking pin_count_ because the 
         * destructors for these objects can called for temporaries being constructed
         * on the fly
         */  
        ::close(fd_);
    }
}
