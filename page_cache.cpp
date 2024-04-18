#include <cassert>
#include <sys/mman.h>
#include "page_cache.hpp"

page_cache* page_cache::instance = nullptr;
std::mutex page_cache::_mutex;

block* page_cache::new_block(size_t k, size_t sub_size) {
    assert(k > 0);
    // std::unique_lock<std::shared_mutex> ulock(page_mutex);
    std::unique_lock<std::mutex> ulock(page_mutex);

    if (k > pc_max_pages) { // case 0
        size_t page_size = sysconf(_SC_PAGE_SIZE);
        void* ptr = mmap(NULL, k * page_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (ptr == (void*)-1) {
            throw std::bad_alloc();
        }
        // block* _block = new block;
        block* _block = block_pool._alloc();
        _block->page_id = ((size_t)ptr) / page_size;
        _block->n = k;
        // page_id_to_block[_block->page_id] = _block;
        // page_id_to_block[_block->page_id + _block->n - 1] = _block;
        page_id_to_block.set(_block->page_id, _block);
        page_id_to_block.set(_block->page_id + _block->n - 1, _block);
        _block->is_used = true;
        _block->sub_size = sub_size;
        return _block;
    }

    if (!_block_lists[k - 1].empty()) { // case 1
        block* _block = _block_lists[k - 1].pop_front();
        for (size_t j = _block->page_id; j < _block->page_id + _block->n; j++) {
            // page_id_to_block[j] = _block;
            page_id_to_block.set(j, _block);
        }
        _block->is_used = true;
        _block->sub_size = sub_size;
        return _block;
    }
    for (int i = k; i < pc_max_pages; i++) {    // case 2
        if (!_block_lists[i].empty()) {
            block* _block = _block_lists[i].pop_front();

            // block* k_block = new block; // split
            block* k_block = block_pool._alloc();
            k_block->page_id = _block->page_id;
            k_block->n = k;
            _block->page_id += k;
            _block->n -= k;
            _block_lists[_block->n - 1].insert(_block_lists[_block->n - 1].begin(), _block);
            // page_id_to_block[_block->page_id] = _block;
            // page_id_to_block[_block->page_id + _block->n - 1] = _block;
            page_id_to_block.set(_block->page_id, _block);
            page_id_to_block.set(_block->page_id + _block->n - 1, _block);
            for (size_t j = k_block->page_id; j < k_block->page_id + k_block->n; j++) {
                // page_id_to_block[j] = k_block;
                page_id_to_block.set(j, k_block);
            }
            k_block->is_used = true;
            k_block->sub_size = sub_size;
            return k_block;
        }
    }
    // case 3
    size_t page_size = sysconf(_SC_PAGE_SIZE);
    void* ptr = mmap(NULL, pc_max_pages * page_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == (void*)-1) {
        throw std::bad_alloc();
    }
    // block* _block = new block;
    block* _block = block_pool._alloc();
    _block->page_id = ((size_t)ptr) / page_size;
    _block->n = pc_max_pages;

    if (k == pc_max_pages) { // case 3 -> case 1
        for (size_t j = _block->page_id; j < _block->page_id + _block->n; j++) {
            // page_id_to_block[j] = _block;
            page_id_to_block.set(j, _block);
        }
        _block->is_used = true;
        _block->sub_size = sub_size;
        return _block;
    }
    // case 3 -> case 2
    // block* k_block = new block; // split
    block* k_block = block_pool._alloc();
    k_block->page_id = _block->page_id;
    k_block->n = k;
    _block->page_id += k;
    _block->n -= k;
    _block_lists[_block->n - 1].insert(_block_lists[_block->n - 1].begin(), _block);
    // page_id_to_block[_block->page_id] = _block;
    // page_id_to_block[_block->page_id + _block->n - 1] = _block;
    page_id_to_block.set(_block->page_id, _block);
    page_id_to_block.set(_block->page_id + _block->n - 1, _block);
    for (size_t j = k_block->page_id; j < k_block->page_id + k_block->n; j++) {
        // page_id_to_block[j] = k_block;
        page_id_to_block.set(j, k_block);
    }
    k_block->is_used = true;
    k_block->sub_size = sub_size;
    return k_block;

}

block* page_cache::get_block_ptr(void* mem) {
    // std::shared_lock<std::shared_mutex> slock(page_mutex);
    
    size_t page_id = ((size_t)mem) / sysconf(_SC_PAGE_SIZE);
    // auto it = page_id_to_block.find(page_id);
    // if (it != page_id_to_block.end()) {
    //     return it->second;
    // } else {
    //     return nullptr; // non-exist
    // }
    return (block*)(page_id_to_block.get(page_id));
}

void page_cache::release_from_cc(block* _block) {
    // std::unique_lock<std::shared_mutex> ulock(page_mutex);
    std::unique_lock<std::mutex> ulock(page_mutex);

    if (_block->n > pc_max_pages) {
        size_t page_size = sysconf(_SC_PAGE_SIZE);
        void* ptr = (void*)(_block->page_id * page_size);
        munmap(ptr, _block->n * page_size);
        // delete _block;
        block_pool._free(_block);
        return;
    }

    while (true) {
        // auto it = page_id_to_block.find(_block->page_id - 1);
        // if (it == page_id_to_block.end()) {
        //     break;
        // }
        // block* left_block = it->second;
        block* left_block = (block*)(page_id_to_block.get(_block->page_id - 1));
        if (left_block == nullptr) {
            break;
        }
        if (left_block->is_used || left_block->n + _block->n > pc_max_pages) {
            break;
        }
        _block->page_id = left_block->page_id;
        _block->n += left_block->n;

        _block_lists[left_block->n - 1].erase(left_block);
        // delete left_block;
        block_pool._free(left_block);
    }
    while (true) {
        // auto it = page_id_to_block.find(_block->page_id + _block->n);
        // if (it == page_id_to_block.end()) {
        //     break;
        // }
        // block* right_block = it->second;
        block* right_block = (block*)(page_id_to_block.get(_block->page_id + _block->n));
        if (right_block == nullptr) {
            break;
        }
        if (right_block->is_used || right_block->n + _block->n > pc_max_pages) {
            break;
        }
        _block->n += right_block->n;

        _block_lists[right_block->n - 1].erase(right_block);
        // delete right_block;
        block_pool._free(right_block);
    }

    _block_lists[_block->n - 1].insert(_block_lists[_block->n - 1].begin(), _block);
    _block->is_used = false;
    // page_id_to_block[_block->page_id] = _block;
    // page_id_to_block[_block->page_id + _block->n - 1] = _block;
    page_id_to_block.set(_block->page_id, _block);
    page_id_to_block.set(_block->page_id + _block->n - 1, _block);

}