/* 
 * File:   main.cpp
 * Author: npujari
 *
 */

#include <cstdlib>
#include "file_cache_impl.h"
#include <thread>
#include <assert.h>
#include <iostream>
#include <stdio.h>
#include <string.h>

using namespace std;

const char* file1 = "file1";
const char* file1_data = "abababab";
const char* file2 = "file2";
const char* file2_data = "cdcdcdcd";
const char* file3 = "file3";
const char* file3_data = "efefefef";
const char* file4 = "file4";
const char* file4_data = "ghghghgh";


void threadfunc(const std::map<std::string, std::string>& files_info,
                 std::shared_ptr<FileCache> fc)
{
    
    std::vector<std::string> file_vec;
    for (const auto& f : files_info) {
        file_vec.push_back(f.first);
    }
    fc->PinFiles(file_vec);
    for (auto f : files_info) {
        char *file_wbuf = fc->MutableFileData(f.first);
        strncpy(file_wbuf, f.second.c_str(), f.second.size());
    }   
    fc->UnpinFiles(file_vec);
    fc->PinFiles(file_vec);
    for (const auto& f : files_info) {
        const char *file_rbuf = fc->FileData(f.first);
        assert(strncmp(file_rbuf, f.second.c_str(), f.second.size() + 1));
    }
    fc->UnpinFiles(file_vec);
    return;
}

/*
 * S_IRUSR
 */
int main(int argc, char** argv) {
    
    //Create a cache of size 2
    auto fc = std::make_shared<FileCacheImpl>(2);
    std::map<std::string, std::string> file_map1;
    file_map1.insert(std::make_pair(file1, file1_data));
    file_map1.insert(std::make_pair(file2, file2_data));
    std::map<std::string, std::string> file_map2;
    file_map2.insert(std::make_pair(file3, file3_data));
    file_map2.insert(std::make_pair(file4, file4_data));
    std::thread t1(threadfunc, std::ref(file_map1), fc);
    std::thread t2(threadfunc, std::ref(file_map2), fc);
    t1.join();
    t2.join();
    cout << "Finished successfully" << endl;
    return 0;
}

