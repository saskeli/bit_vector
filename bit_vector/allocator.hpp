#ifndef BV_ALLOCATOR_HPP
#define BV_ALLOCATOR_HPP

#include <cstdint>
#include <new>
#include <cstring>
#include <cstdlib>
#include <iostream>

class malloc_alloc {
  private:
    uint64_t allocations_;

  public:
    malloc_alloc() {
        allocations_ = 0;
    }

    template <class node_type>
    node_type* allocate_node() {
        allocations_++;
        void* nd = malloc(sizeof(node_type));
        return new(nd) node_type(this);
    }

    template <class node_type>
    void deallocate_node(node_type* node) {
        allocations_--;
        free(node);
    }

    template <class leaf_type>
    leaf_type* allocate_leaf(uint64_t size) {
        allocations_++;
        void* leaf = malloc(sizeof(leaf_type) + size * sizeof(uint64_t));
        uint8_t* data_ptr = reinterpret_cast<uint8_t*>(leaf)+sizeof(leaf_type);
        memset(data_ptr, 0, size * sizeof(uint64_t));
        return new(leaf) leaf_type(size, reinterpret_cast<uint64_t*>(data_ptr));
    }

    template <class leaf_type>
    void deallocate_leaf(leaf_type* leaf) {
        allocations_--;
        free(leaf);
    }

    template <class leaf_type>
    leaf_type* reallocate_leaf(leaf_type* leaf, uint64_t old_size, uint64_t new_size) {
#pragma GCC diagnostic ignored "-Wclass-memaccess"
        leaf_type* n_leaf = reinterpret_cast<leaf_type*>(realloc(leaf, sizeof(leaf_type) + new_size * sizeof(uint64_t)));
#pragma GCC diagnostic pop
        uint8_t* data_ptr = reinterpret_cast<uint8_t*>(n_leaf)+sizeof(leaf_type);
        memset(data_ptr + sizeof(uint64_t) * old_size, 0, sizeof(uint64_t) * (new_size - old_size));
        n_leaf->set_data_ptr(reinterpret_cast<uint64_t*>(data_ptr));
        n_leaf->capacity(new_size);
        return n_leaf;
    }

    uint64_t live_allocations() const {
        return allocations_;
    }
};
#endif