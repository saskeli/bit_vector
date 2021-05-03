#ifndef BV_SIMPLE_NODE_HPP
#define BV_SIMPLE_NODE_HPP

#include <cstring>
#include <cstdint>

#define CHILDREN 32

template <class data_type, class leaf_type, data_type leaf_size>
class simple_node {
  private:
    uint8_t meta_data_; // meta_data_ & 0x80 if children are leaves
    data_type child_sizes_[CHILDREN];
    data_type child_sums_[CHILDREN];
    void* children_[CHILDREN]; // pointers to leaf_type or simple_node elements
    void* allocator_; //This will be the allocator

  public:
    simple_node(void* allocator, bool has_leaves) {
        meta_data_ = has_leavs ? 0x80 : 0x00;
        allocator_ = allocator;
        memset(&child_sizes_, 0xff, sizeof(data_type) * CHILDREN);
        memset(&child_sums_, 0xff, sizeof(data_type) * CHILDREN);
    }
    
  private:
    uint8_t find_child(data_type q) {
        constexpr data_type MASK = data_type(1) << (sizeof(data_type) * 8 - 1);
        uint8_t idx = (uint8_t(1) << 5) - 1;
        idx ^= (((arr[idx] - q) & MASK) >> 26) | (uint8_t(1) << 4);
        idx ^= (((arr[idx] - q) & MASK) >> 27) | (uint8_t(1) << 3);
        idx ^= (((arr[idx] - q) & MASK) >> 28) | (uint8_t(1) << 2);
        idx ^= (((arr[idx] - q) & MASK) >> 29) | (uint8_t(1) << 1);
        idx ^= (((arr[idx] - q) & MASK) >> 30) | uint8_t(1);
        return idx ^ (((arr[idx] - q) & MASK) >> 31); 
    }
};
#endif