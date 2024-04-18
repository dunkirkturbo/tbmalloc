#pragma once

#include <cassert>
#include <thread>
#include "common.hpp"

class thread_cache {
public:
    void* alloc(size_t size);
    void dealloc(void* ptr, size_t size);
    void* fetch_from_cc(size_t freelist_idx, size_t size_align);

private:
    freelist _freelists[freelist_num];
};

static thread_local thread_cache* tc = nullptr; // tls