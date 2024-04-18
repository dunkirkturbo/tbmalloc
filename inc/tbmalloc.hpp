#pragma once

#include "thread_cache.hpp"
#include "page_cache.hpp"
#include "object_pool.hpp"

void* tballoc(size_t size) {
    void* ptr = nullptr;
    if (size > tc_max_bytes) {
        size_t size_align = size_util::round_up(size);
        size_t page_size = sysconf(_SC_PAGE_SIZE);
        size_t k = size_align / page_size;
        block* _block = page_cache::get_instance()->new_block(k, size_align);
        ptr = (void*)(_block->page_id * page_size);
    } else {
        // std::cout << std::this_thread::get_id() << ": " << tc << std::endl;
        if (tc == nullptr) {
            // tc = new thread_cache();
            static object_pool<thread_cache> tc_pool;
            tc_pool._mutex.lock();
            tc = tc_pool._alloc();
            tc_pool._mutex.unlock();
        }
        ptr = tc->alloc(size);
    }
    return ptr;
}

void tbfree(void* ptr) {
    if (ptr) {
        block* _block = page_cache::get_instance()->get_block_ptr(ptr);
        size_t size = _block->sub_size;
        if (size > tc_max_bytes) {
            page_cache::get_instance()->release_from_cc(_block);
        } else {
            tc->dealloc(ptr, size);
        }
    }
}