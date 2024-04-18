#pragma once

#include <thread>
#include "common.hpp"

class central_cache {
public:
    static central_cache* get_instance() {
        if (instance == nullptr) {
            std::unique_lock<std::mutex> ulock(_mutex);
            if (instance == nullptr) {  // double check lock
                instance = new central_cache();
            } else {
                ulock.unlock();
            }
        }
        return instance;
    }

    size_t fetch_to_tc(void*& start, void*& end, size_t batch_num, size_t size);
    void release_from_tc(void* start, size_t size); // get released memory from tc

private:
    block_list _block_lists[freelist_num];
    static central_cache* instance;
    static std::mutex _mutex;

    central_cache() {}
    central_cache(const central_cache&) = delete;
    central_cache& operator=(const central_cache&) = delete;

    block* get_block(block_list& list, size_t size);
};