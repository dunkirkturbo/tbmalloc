#pragma once

#include <iostream>
#include <mutex>
#include <sys/mman.h>

template <typename T>
class object_pool {
public:
    T* _alloc() {
        T* ptr = nullptr;
        if (_freelist) {
            void* next = *(void**)_freelist;    // get the first 32/64 bits where _freelist points (depend on os)
            ptr = (T*)_freelist;
            _freelist = next;
        } else {
            size_t size = sizeof(T) >= sizeof(void*) ? sizeof(T) : sizeof(void*);
            if (remain_bytes < size) {
                remain_bytes = 0x20000;
                _mem = (char*)mmap(NULL, remain_bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                if (_mem == (void*)-1) {
                    throw std::bad_alloc();
                }
            }
            ptr = (T*)_mem;
            _mem += size;
            remain_bytes -= size;
        }
        new(ptr) T; // default T constructor
        return ptr;
    }

    void _free(T* ptr) {
        if (ptr) {
            ptr->~T();
            *(void**)ptr = _freelist;
            _freelist = ptr;
        }
    }

private:
    char* _mem = nullptr;
    void* _freelist = nullptr;  // released memory
    size_t remain_bytes = 0;

public:
    std::mutex _mutex;
};