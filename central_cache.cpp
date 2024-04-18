#include <cassert>
#include "central_cache.hpp"
#include "page_cache.hpp"

central_cache* central_cache::instance = nullptr;
std::mutex central_cache::_mutex;

size_t central_cache::fetch_to_tc(void*& start, void*& end, size_t batch_num, size_t size) {    // range: [start, end]
    size_t freelist_idx = size_util::index(size);   // size has been aligned

    std::unique_lock<std::mutex> ulock(_block_lists[freelist_idx]._mutex);
        
    block* _block = get_block(_block_lists[freelist_idx], size);    // after `get_block`, _mutex must be still locked
    assert(_block && _block->_freelist);
    start = end = _block->_freelist;
    size_t act_num = 1;
    for (size_t i = 0; i < batch_num - 1; i++) {
        if (freelist::get_next(end)) {
            end = freelist::get_next(end);
            act_num++;
        } else {
            break;
        }
    }
    _block->_freelist = freelist::get_next(end);
    _block->use_count += act_num;   // fetch `act_num` memory to tc
    freelist::get_next(end) = nullptr;

    return act_num;
}

block* central_cache::get_block(block_list& list, size_t size) {
    for (block* it = list.begin(); it != list.end(); it = it->next) {
        if (it->_freelist) {    // case 1
            return it;
        }
    }
    // case 2: get block from page_cache
    list._mutex.unlock();
    size_t k = size_util::page_num_move(size);  // k pages
    block* _block = page_cache::get_instance()->new_block(k, size);
    // divide _block
    size_t page_size = sysconf(_SC_PAGE_SIZE);
    char* start = (char*)(page_size * _block->page_id);
    char* end = start + page_size * _block->n;
    
    _block->_freelist = start;
    char* next = start + size;
    while (next < end) {
        freelist::get_next(start) = next;
        start = next;
        next += size;
    }
    freelist::get_next(start) = nullptr;

    list._mutex.lock(); // unlock in `fetch_to_tc`
    list.insert(list.begin(), _block);

    return _block;
}

void central_cache::release_from_tc(void* start, size_t size) {
    size_t freelist_idx = size_util::index(size);

    std::unique_lock<std::mutex> ulock(_block_lists[freelist_idx]._mutex);
    page_cache* pc = page_cache::get_instance();
    while (start) {
        void* next = freelist::get_next(start);
        
        block* _block = pc->get_block_ptr(start);
        freelist::get_next(start) = _block->_freelist;
        _block->_freelist = start;  // insert into pc->get_block_ptr(start)
        
        _block->use_count--;
        if (_block->use_count == 0) {
            _block_lists[freelist_idx].erase(_block);
            ulock.unlock();
            _block->_freelist = nullptr;    // _freelist etc. are useless when released
            _block->next = nullptr;
            _block->prev = nullptr;
            pc->release_from_cc(_block);
            ulock.lock();
        }

        start = next;
    }
}