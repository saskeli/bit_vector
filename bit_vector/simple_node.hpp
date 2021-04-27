#ifndef BV_SIMPLE_NODE_HPP
#define BV_SIMPLE_NODE_HPP

#include <cstring>
#include <cstdint>

#define CHILDREN 32

template <class data_type, class leaf_type, class allocator_type, data_type leaf_size>
class simple_node {
  private:
    uint8_t meta_data_;
    data_type child_sizes_[CHILDREN];
    data_type child_sums_[CHILDREN];
    void* children_[CHILDREN];
    allocator_type* allocator_;

  public:
    simple_node(allocator_type* allocator) {
        meta_data_ = 0x80;
        allocator_ = allocator;
        memset(&child_sizes_, 0xff, sizeof(data_type) * CHILDREN);
        memset(&child_sums_, 0xff, sizeof(data_type) * CHILDREN);
    }
    
    ~simple_node() {
        allocator_.deallocate_node(this);
    }

  private:
    uint8_t branchless(int* arr, int q) {
        constexpr uint32_t MASK = uint32_t(1) << 31;
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