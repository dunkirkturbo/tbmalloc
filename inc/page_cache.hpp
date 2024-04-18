#pragma once

#include <unordered_map>
#include "common.hpp"
#include "object_pool.hpp"
#include "radix_tree.hpp"

class page_cache {
public:
    static page_cache* get_instance() {
        if (instance == nullptr) {
            std::unique_lock<std::mutex> ulock(_mutex);
            if (instance == nullptr) {  // double check lock
                instance = new page_cache();
            } else {
                ulock.unlock();
            }
        }
        return instance;
    }

    block* new_block(size_t k, size_t sub_size); // block of k pages (divided by `sub_size`-byte)
    block* get_block_ptr(void* mem);
    void release_from_cc(block* _block);

private:
    block_list _block_lists[pc_max_pages];
    object_pool<block> block_pool;
    // std::shared_mutex page_mutex;
    std::mutex page_mutex;
    // std::unordered_map<size_t, block*> page_id_to_block;
    radix_tree page_id_to_block;

    static page_cache* instance;
    static std::mutex _mutex;  // just for singleton

    page_cache() {}
    page_cache(const page_cache&) = delete;
    page_cache& operator=(const page_cache&) = delete;
};