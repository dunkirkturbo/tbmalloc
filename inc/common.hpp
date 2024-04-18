#pragma once

#include <iostream>
#include <mutex>
// #include <shared_mutex>
#include <unistd.h>

/*
 * size=(      0,      128]   ->        8-align  ->  freelist[  0,  16)
 * size=(    128,      256]   ->       16-align  ->  freelist[ 16,  24)     (worst case: 129/144\approx 89.6%)
 * size=(    256,      512]   ->       32-align  ->  freelist[ 24,  32)     (worst case: 257/288\approx 89.2%)
 * ...
 * size=(128*1024, 256*1024]  ->  16*1024-align  ->  freelist[96, 104)      (worst case: \approx 88.9%)
 */

static const size_t freelist_num = 104;
static const size_t tc_max_bytes = 256 * 1024;  // alloc_max once
static const int freelist_nums[12] = { 0, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 96 };
static const size_t pc_max_pages = 128;

class size_util {
public:
    static size_t round_up(size_t size) {
        if (size <= 0x80) {
            return _round_up(size, 8);
        } else if (size <= 0x40000) {
            size_t tmp = (size - 1) >> 3, align = 1;
            while (tmp >>= 1) {
                align <<= 1;
            }
            return _round_up(size, align);
        } else {
            return _round_up(size, (size_t)sysconf(_SC_PAGE_SIZE));
        }
    }

    static size_t index(size_t size) {
        if (size <= 0x80) {
            return _index(size, 3);
        } else if (size <= 0x40000) {   // 256*1024
            size_t tmp = (size - 1) >> 3, align_shift = 0;
            while (tmp >>= 1) {
                align_shift++;
            }
            return _index(size - (1 << (align_shift + 3)), align_shift) + freelist_nums[align_shift - 3];
        } else {
            return -1;
        }
    }

    static size_t num_move(size_t size) {   // number of memory with `size` are moved once
        static const size_t num_move_max = 512;
        static const size_t num_move_min = 2;
        
        if (size > 0) {
            size_t num = tc_max_bytes / size;
            num = num > num_move_max ? num_move_max : (num < num_move_min ? num_move_min : num);
            return num;
        } else {
            return 0;
        }
    }

    static size_t page_num_move(size_t size) {
        size_t total_size = num_move(size) * size;
        size_t page_num = total_size / (size_t)sysconf(_SC_PAGE_SIZE);  // always 4KB per page
        if (!page_num) {
            page_num = 1;
        }
        return page_num;
    }

private:
    static inline size_t _round_up(size_t size, size_t align) {
        return (size + align - 1) & (~(align - 1));
    }

    static inline size_t _index(size_t size, size_t align_shift) { // align = 1 << align_shift
        return ((size + (1 << align_shift) - 1) >> align_shift) - 1;    // freelist index (inner)
    }
};


class freelist {
public:
    static inline void*& get_next(void* ptr) {
        return *(void**)ptr;
    }

    size_t size() {
        return _size;
    }

    size_t& max_size() {    // return size_t&
        return _max_size;
    }

    bool empty() {
        return _freelist == nullptr;
    }

    void push(void* mem) {
        if (mem) {
            get_next(mem) = _freelist;
            _freelist = mem;
            _size++;
        }
    }

    void* pop() {
        void* mem = _freelist;
        _freelist = get_next(_freelist);
        _size--;
        return mem;
    }

    void push(void* start, void* end, size_t size) {    // range
        get_next(end) = _freelist;
        _freelist = start;
        _size += size;
    }

    void* pop(size_t size) {   // pop range (front `size`)
        if (_size >= size) {
            void* start = _freelist, *end = start;
            for (size_t i = 0; i < size - 1; i++) {
                end = get_next(end);
            }
            _freelist = get_next(end);
            get_next(end) = nullptr;
            _size -= size;
            return start;
        } else {
            return nullptr;
        }
    }

private:
    void* _freelist = nullptr;
    size_t _size = 0;
    size_t _max_size = 1;   // init as 1 (slow start)
};

struct block {
    size_t page_id = 0;
    size_t n = 0;   // page number
    size_t sub_size = 0;    // split size

    void* _freelist = nullptr;

    size_t use_count = 0;
    bool is_used = false;

    block* prev = nullptr;
    block* next = nullptr;
};

class block_list {  // doubly circular linked list
public:
    block_list() {
        _head = new block;
        _head->prev = _head->next = _head;
    }

    void insert(block* pos, block* _block) {
        if (_block) {
            block* pos_prev = pos->prev;
            _block->next = pos;
            pos->prev = _block;
            _block->prev = pos_prev;
            pos_prev->next = _block;
        }
    }

    void erase(block* pos) {
        if (pos && pos != _head) {
            pos->prev->next = pos->next;
            pos->next->prev = pos->prev;
            // delete pos;
        }
    }

    block* begin() {
        return _head->next;
    }

    block* end() {
        return _head;
    }

    bool empty() {
        return _head == _head->next;
    }

    block* pop_front() {    // must !empty()
        block* front = _head->next;
        erase(front);
        return front;
    }

    std::mutex _mutex;
private:
    block* _head;   // dummy node
};