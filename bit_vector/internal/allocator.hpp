#ifndef BV_ALLOCATOR_HPP
#define BV_ALLOCATOR_HPP

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>

namespace bv {

/**
 * @brief Simple malloc based allocation for internal nodes and leaves.
 * 
 * Malloc and free are directly used to allocate/deallocate internal nodes.
 * 
 * For leaves, a block is (re)allocated for housing both the leaf "struct" and 
 * the associated data.
 */
class malloc_alloc {
   protected:
    uint64_t allocations_; ///< Number of objects currently allocated.

   public:
    malloc_alloc() { allocations_ = 0; }

    /**
     * @brief Allocate new internal node.
     * 
     * Simply uses `malloc(sizeof(node_type))`.
     * 
     * @tparam node_type Internal node type. Typically some kind of bv::node.
     */
    template <class node_type>
    node_type* allocate_node() {
        allocations_++;
        void* nd = malloc(sizeof(node_type));
        return new (nd) node_type();
    }

    /**
     * @brief Deallocate internal node.
     * 
     * Simply does `free(node)`.
     * 
     * @tparam node_type Internal node type. Typically some kind of bv::node.
     * 
     * @param node Internal node to deallocate.
     */
    template <class node_type>
    void deallocate_node(node_type* node) {
        allocations_--;
        free(node);
    }

    /**
     * @brief Allocates a new leaf with space for storing `size * 64` bits.
     * 
     * Uses malloc to allocate `sizeof(leaf_type) + 8 * size` bytes, and 
     * initializes the first `sizeof(leaf_type)` bytes of the allocated block
     * using the default constructor of `leaf_type`.
     * 
     * @tparam leaf_type Bit vector leaf type. Typically some kind of bv::leaf.
     * 
     * @param size Number of 64-bit words to reserve for data storage.
     */
    template <class leaf_type>
    leaf_type* allocate_leaf(uint64_t size) {
        allocations_++;
        constexpr size_t leaf_bytes = sizeof(leaf_type) + sizeof(leaf_type) % 8;
        void* leaf = malloc(leaf_bytes + size * sizeof(uint64_t));
        uint8_t* data_ptr =
            reinterpret_cast<uint8_t*>(leaf) + leaf_bytes;
        memset(data_ptr, 0, size * sizeof(uint64_t));
        return new (leaf)
            leaf_type(size, reinterpret_cast<uint64_t*>(data_ptr));
    }

    /**
     * @brief Deallocates leaf node.
     * 
     * Simply calls `free(leaf)`.
     * 
     * @tparam leaf_type Bit vector leaf type. Typically some kind of bv::leaf.
     */
    template <class leaf_type>
    void deallocate_leaf(leaf_type* leaf) {
        allocations_--;
        free(leaf);
    }

    /**
     * @brief Reallocator for leaf nodes.
     * 
     * Reallocates the block for a leaf and associated data.
     * 
     * @tparam leaf_type Bit vector leaf type. Typically some kind of bv::leaf.
     * 
     * @param leaf     Pointer to leaf_type to reallocate.
     * @param old_size Size of leaf data block. (in 64 bit words.)
     * @param new_size New size for leaf data block. (in 64 bit words.)
     */
    template <class leaf_type>
    leaf_type* reallocate_leaf(leaf_type* leaf, uint64_t old_size,
                               uint64_t new_size) {
        constexpr size_t leaf_bytes = sizeof(leaf_type) + sizeof(leaf_type) % 8;
#pragma GCC diagnostic ignored "-Wclass-memaccess"
        leaf_type* n_leaf = reinterpret_cast<leaf_type*>(
            realloc(leaf, leaf_bytes + new_size * sizeof(uint64_t)));
#pragma GCC diagnostic pop
        uint8_t* data_ptr =
            reinterpret_cast<uint8_t*>(n_leaf) + leaf_bytes;
        if (old_size < new_size) {
            memset(data_ptr + sizeof(uint64_t) * old_size, 0,
                sizeof(uint64_t) * (new_size - old_size));
        }
        n_leaf->set_data_ptr(reinterpret_cast<uint64_t*>(data_ptr));
        n_leaf->capacity(new_size);
        return n_leaf;
    }

    /**
     * @brief Get the number of blocks currenty allocated by this allocator instance.
     * 
     * @return Number of blocks currently allocated by this allocator instance.
     */
    uint64_t live_allocations() const { return allocations_; }
};
}  // namespace bv
#endif