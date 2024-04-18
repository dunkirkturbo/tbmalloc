#pragma once

#include <iostream>
#include "object_pool.hpp"

struct radix_node {
    radix_node* childs[16] = {nullptr}; // 2^4
    void* ptr;
};

class radix_tree {
public:
    static const int page_bit = 52;  // for 64-bit OS, 4KB/per page

    explicit radix_tree() {
        root = new radix_node;
    }

    void set(size_t page_id, void* ptr) {
        radix_node* cur = root;
        for (int i = page_bit - 4; i >= 0; i -= 4) {
            int index = (page_id >> i) & 0x0f;
            if (cur->childs[index] == nullptr) {
                static object_pool<radix_node> node_pool;
                cur->childs[index] = node_pool._alloc();
            }
            cur = cur->childs[index];
        }
        cur->ptr = ptr;
    }

    void* get(size_t page_id) {
        if ((page_id >> page_bit) > 0) {
            return nullptr;
        }
        radix_node* cur = root;
        for (int i = page_bit - 4; i >= 0; i -= 4) {
            int index = (page_id >> i) & 0x0f;
            if (cur->childs[index] == nullptr) {
                return nullptr;
            }
            cur = cur->childs[index];
        }
        return cur->ptr;
    }

private:
    radix_node* root = nullptr;
};