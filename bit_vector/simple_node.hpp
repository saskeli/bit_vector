#ifndef BV_SIMPLE_NODE_HPP
#define BV_SIMPLE_NODE_HPP

#include <cstring>
#include <cstdint>

#define CHILDREN 64

template <class leaf_type, uint64_t leaf_size>
class simple_node {
  private:
    uint8_t meta_data_; // meta_data_ & 0x80 if children are leaves
    uint64_t child_sizes_[CHILDREN];
    uint64_t child_sums_[CHILDREN];
    void* children_[CHILDREN]; // pointers to leaf_type or simple_node elements
    void* allocator_; //This will be the allocator

    typedef simple_node<leaf_type, leaf_size> node;

    uint8_t find_size(uint64_t q) const {
        constexpr uint64_t SIGN_BIT = uint64_t(1) << 63;
        uint8_t idx = (uint8_t(1) << 5) - 1;
        idx ^= (uint64_t((child_sizes_[idx] - q) & SIGN_BIT) >> 58) | (uint8_t(1) << 4);
        idx ^= (uint64_t((child_sizes_[idx] - q) & SIGN_BIT) >> 59) | (uint8_t(1) << 3);
        idx ^= (uint64_t((child_sizes_[idx] - q) & SIGN_BIT) >> 60) | (uint8_t(1) << 2);
        idx ^= (uint64_t((child_sizes_[idx] - q) & SIGN_BIT) >> 61) | (uint8_t(1) << 1);
        idx ^= (uint64_t((child_sizes_[idx] - q) & SIGN_BIT) >> 62) | uint8_t(1);
        return idx ^ (uint64_t((child_sizes_[idx] - q) & SIGN_BIT) >> 63); 
    }

    uint8_t find_sum() const {
        constexpr uint64_t SIGN_BIT = uint64_t(1) << 63;
        uint8_t idx = (uint8_t(1) << 5) - 1;
        idx ^= (uint64_t((child_sums_[idx] - q) & SIGN_BIT) >> 58) | (uint8_t(1) << 4);
        idx ^= (uint64_t((child_sums_[idx] - q) & SIGN_BIT) >> 59) | (uint8_t(1) << 3);
        idx ^= (uint64_t((child_sums_[idx] - q) & SIGN_BIT) >> 60) | (uint8_t(1) << 2);
        idx ^= (uint64_t((child_sums_[idx] - q) & SIGN_BIT) >> 61) | (uint8_t(1) << 1);
        idx ^= (uint64_t((child_sums_[idx] - q) & SIGN_BIT) >> 62) | uint8_t(1);
        return idx ^ (uint64_t((child_sums_[idx] - q) & SIGN_BIT) >> 63); 
    }

  public:
    simple_node(void* allocator) {
        meta_data_ = 0;
        allocator_ = allocator;
        memset(&child_sizes_, 0xff, sizeof(uint64_t) * CHILDREN);
        memset(&child_sums_, 0xff, sizeof(uint64_t) * CHILDREN);
    }

    void has_leaves(bool leaves) const {
        meta_data_ = leaves ? meta_data_ | 0b10000000 : meta_data_ & 0b01111111;
    }

    bool has_leaves() const {
        return metadata >> 7;
    }
    
    template<class allocator>
    node* split() {
        node* sibling = reinterpret_cast<allocator*>(allocator_)->allocate_node();
        if (has_leaves()) node->has_leaves(true);
        for (size_t i = CHILDREN >> 1; i < CHILDREN; i++) {
            node->append_child(children_[i]);
        }
        memset(&child_sizes_[CHILDREN >> 1], 0xff, sizeof(uint64_t) * (CHILDREN >> 1));
        memset(&child_sums_[CHILDREN >> 1], 0xff, sizeof(uint64_t) * (CHILDREN >> 1));
        meta_data_ ^= 0b01100000;
        return sibling;
    }

    uint8_t child_count() const {
        return meta_data_ & 0b01111111;
    }

    template<class child>
    void append_child(child* new_child) {
        if (child_count() == 0) {
            child_sizes_[0] = new_child->size();
            child_sums_[0] = new_child->p_sum();
            [[unlikely]] children_[0] = new_child;
        } else {
            uint8_t count = child_count();
            child_sizes_[count] = child_sizes_[count - 1] + new_child->size();
            child_sums_[count] = child_sums_[count - 1] + new_child->p_sum();
            children_[count] = new_child;
        }
        meta_data_++;
    }

    node* child(uint64_t i) {
        return children_[i];
    }

    template<class allocator>
    void insert(uint64_t index, bool value) {
        uint8_t child_index = find_child(index);
        if (has_leaves()) {
            leaf_type* child = reinterpret_cast<leaf_type*>(children_[child_index]);

            if (child->need_realloc()) {
                if (child->size() >= leaf_size) {
                    leaf_type* new_child = child->split<allocator>(allocator_);
                    for (size_t i = child_count(); i > child_index; i--) {
                        child_sizes_[i] = child_sizes_[i - 1];
                        child_sums_[i] = child_sums_[i - 1];
                        children_[i] = children_[i - 1];
                    }
                    if (child_index == 0) {
                        child_sizes_[0] = new_child->size();
                        [[unlikely]] child_sums_[0] = new_child->p_sum();
                    } else {
                        child_sizes_[child_index] = child_sizes_[child_index - 1] + new_child->size();
                        child_sums_[child_index] = child_sums_[child_index - 1] + new_child->p_sum();
                    }
                    if (child_sizes_[child_index] >= index) {
                        child = new_child;
                    } else {
                        child_index++;
                    }
                    [[unlikely]] children_[child_index] = new_child;
                } else {
                    uint64_t cap = child->capacity();
                    child = reinterpret_cast<allocator*>(allocator_)->reallocate_leaf<leaf_type>(child, cap, cap * 2);
                    children_[child_index] = child;
                }
                [[unlikely]] ((void)0);
            }
            if (child_index != 0) {
                [[likely]] index -= child_sizes_[child_index - 1];
            }
            child->insert(index, value);
        } else {
            node* child = reinterpret_cast<node*>(children_[child_index]);
            if (child->child_count() == 64) {
                node* new_child = child->split();
                for (size_t i = child_count(); i > child_index; i--) {
                    child_sizes_[i] = child_sizes_[i - 1];
                    child_sums_[i] = child_sums_[i - 1];
                    children_[i] = children_[i - 1];
                }
                children_[child_index + 1] = new_child;
                if (child_index == 0) {
                    child_sizes_[0] = child->size();
                    child_sums_[0] = child->p_sum();
                } else {
                    child_sizes_[child_index] = child_sizes_[child_index - 1] + child->size();
                    child_sums_[child_index] = child_sums_[child_index - 1] + child->p_sum();
                }
                if (child_sizes_[child_index] < index) {
                    child = new_child;
                    child_index++;
                }
                [[unlikely]] ((void)0);
            }
            if (child_index != 0) {
                [[likely]] index -= child_sizes_[child_index - 1];
            }
            child->insert(index, value);
            for (size_t i = child_index; i < child_count(); i++) {
                child_sizes_[i]++;
                child_sums_[i] += value;
            }
        }
        
    }

    template<class allocator>
    void remove(uint64_t index) {
        uint8_t child_index = find_child(index);
        if (has_leaves()) {
            leaf_type* child = reinterpret_cast<leaf_type*>(children_[child_index]);
        }

    }
};
#endif