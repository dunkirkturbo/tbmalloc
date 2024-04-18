#include "thread_cache.hpp"
#include "central_cache.hpp"

void* thread_cache::alloc(size_t size) {
    assert(size <= tc_max_bytes);

    size_t size_align = size_util::round_up(size);
    size_t freelist_idx = size_util::index(size);
    if (!_freelists[freelist_idx].empty()) {
        return _freelists[freelist_idx].pop();
    } else {
        return fetch_from_cc(freelist_idx, size_align);
    }

}

void thread_cache::dealloc(void* ptr, size_t size) {
    if (ptr) {
        assert(size <= tc_max_bytes);
        size_t freelist_idx = size_util::index(size);
        _freelists[freelist_idx].push(ptr);

        if (_freelists[freelist_idx].size() >= _freelists[freelist_idx].max_size()) {
            size_t release_num = _freelists[freelist_idx].max_size();
            /* `release_num` must ensure the possibility of invoking page_cache::release_from_cc
               , i.e., _freelists[freelist_idx] needs to be potentially empty */

            void* start = _freelists[freelist_idx].pop(release_num);
            central_cache::get_instance()->release_from_tc(start, size);
        }
    }
}

void* thread_cache::fetch_from_cc(size_t freelist_idx, size_t size_align) {
    size_t batch_num = 0;
    {
        size_t& cur_max = _freelists[freelist_idx].max_size();   // low start control
        batch_num = std::min(cur_max, size_util::num_move(size_align));
        if (batch_num == cur_max) {
            cur_max++;
        }
    }
    // std::cout << "batch_num: " << batch_num << std::endl;
    
    void* start = nullptr, *end = nullptr;
    size_t act_num = central_cache::get_instance()->fetch_to_tc(start, end, batch_num, size_align); // act_num <= batch_num
    assert(act_num >= 1);
    if (act_num == 1) {
        assert(start == end);
    } else {
        _freelists[freelist_idx].push(freelist::get_next(start), end, act_num - 1);
    }
    return start;
}