
#ifndef _FILE_CACHE_IMPL_H_
#define _FILE_CACHE_IMPL_H_


#include<memory>
#include<mutex>
#include<map>
#include <set>
#include<condition_variable>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include"file_cache.h"

//Constant file size 10 * 1024 bytes
#define FILE_SIZE 10240


class FileCacheImpl : public FileCache {
public:
    FileCacheImpl(int max_cache_entries) : FileCache(max_cache_entries)
    {}
    void PinFiles(const std::vector<std::string>& file_vec);
    void UnpinFiles(const std::vector<std::string>& file_vec);
    const char *FileData(const std::string& file_name);
    char *MutableFileData(const std::string& file_name);
private:
    struct CacheEntry {
        CacheEntry(std::shared_ptr<char> file_buf,
                   uint32_t pin_count,
                   int fd) : file_buf_(file_buf),
                             pin_count_(pin_count), 
                             dirty_(false),
                             fd_(fd)
        {}
        ~CacheEntry();
        std::shared_ptr<char> file_buf_;
        uint32_t pin_count_;
        bool dirty_;
        int fd_;
    };
    std::map<std::string, CacheEntry> file_cache_;
    std::mutex m_;  
    std::condition_variable cv_;
    
    bool cache_entries_evictable()             
    {
        for (const auto& ce : file_cache_) {
            if (ce.second.pin_count_ == 0) {
                return true;
            }
        }
        return false;
    }
    uint32_t evict_cache_entries(int num_cache_entries);
    void add_cache_entry(const std::string& file_name);
    void fill_up_cache(std::set<std::string>& files_not_pinned);
};

#endif // _FILE_CACHE_IMPL_H_
