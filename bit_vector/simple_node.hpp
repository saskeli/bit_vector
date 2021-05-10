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

    void* children() const {
        return children_;
    }

    uint64_t* child_sizes() const {
        return child_sizes_;
    }

    uint64_t* child_sums() const {
        return child_sums_;
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
            child_sums_[count] = child_sums_[count - 1] + nsize_t
        meta_data_++;
    }

    node* child(uint64_t i) {
        return children_[i];
    }

    template<class allocator>
    void insert(uint64_t index, bool value) {
        if (has_leaves()) {
            leaf_insert<allocator>(index, value);
        } else {
            [[likely]] node_insert<allocator>(index, value);
        }
    }

    template<class allocator>
    bool remove(uint64_t index) {
        if (has_leaves()) {
            return leaf_remove<allocator>(index);
        } else {
            [[likely]] return node_remove<allocator>(index);
        }
    }

    void clear_first(uint8_t elems) {
        uint64_t o_size = child_sizes_[elems - 1];
        uint64_t o_sum = child_sums_[elems - 1];
        uint8_t size = child_count();
        for (uint8_t i = 0; i < size - elems; i++) {
            children_[i] = children[i + elems];
            child_sizes_[i] = child_sizes_[i + elems] - o_size;
            child_sums_[i] = child_sums_[i + elems] - o_sum;
        }
        memset(child_sizes_ + (size - elems), 0xff, sizeof(uint64_t) * elems);
        memset(child_sums_ + (size - elems), 0xff, sizeof(uint64_t) * elems);
        meta_data_ -= elems;
    }

    void transfer_append(node* other, uint8_t elems) {
        void* o_children = other->children();
        uint64_t* o_sizes = other->child_sizes();
        uint64_t* o_sums = other->child_sums();
        uint8_t local_index = child_count();
        uint8_t high_index = local_index - 1;
        for (uint8_t i = 0; i < elems; i++) {
            children_[local_index] = o_children
            child_sizes_[local_index] = child_sizes_[high_index] + o_sizes[i];
            child_sums_[local_index] = child_sums_[high_index] + o_sums[i];
        }
        meta_data_ += elems;
        other->clear_first(elems);
    }

    void clear_last(uint8_t elems) {
        uint8_t size = child_count();
        memset(child_sizes_ + (size - elems), 0xff, sizeof(uint64_t) * elems);
        memset(child_sums_ + (size - elems), 0xff, sizeof(uint64_t) * elems);
        meta_data -= elems;
    }

    void transfer_prepend(node* other, uint8_t elems) {
        void* o_children = other->children();
        uint64_t* o_sizes = other->child_sizes();
        uint64_t* o_sums = other->child_sums();
        uint8_t size = child_count();
        uint8_t o_size = other->child_count();
        memmove(children_ + elems, children_, size * sizeof(void*));
        memmove(child_sums_ + elems, child_sums_, size * sizeof(void*));
        memmove(child_sizes_ + elems, child_sizes_, size * sizeof(void*));
        for (uint8_t i = 0; i < elems; i++) {
            children_[i] = o_children[o_size - elems + i];
            child_sums_[i] = o_sums[o_size - elems + i] - o_sums[o_size - elems - 1];
            child_sizes_[i] = o_sizes[o_size - elems + i] - o_sizes[o_size - elems - 1];
        }
        for (uint8_t i = elems; i < elems + size; i++) {
            child_sums_[i] += child_sums_[elems - 1];
            child_sizes_[i] += child_sizes_[elems - 1];
        }
        meta_data += elems;
        other->clear_last(elems);
    }

    void append_all(node* other) {
        void* o_children = other->children();
        uint64_t* o_sizes = other->child_sizes();
        uint64_t* o_sums = other->child_sums();
        uint8_t size = child_count();
        uint8_t o_size = other->child_count();
        for (uin8_t i = 0; i < o_size; i++) {
            children_[size + i] = o_children[i];
            child_sums_[size + i] = o_sums[i] + child_sums_[size - 1];
            child_sizes_[size + i] = o_sums[i] + child_sizes_[size - 1];
        }
        meta_data_ += o_size;
    }

  private:
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

    template<class allocator>
    void leaf_insert(uint64_t index, bool value) {
        uint8_t child_index = find_child(index);
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
        for (uint8_t i = child_index; i < child_count(); i++) {
            child_sizes_[i]++;
            child_sums_[i] += value;
        }
        child->insert(index, value);
    }

    template<class allocator>
    void node_insert(uint64_t index, bool value) {
        uint8_t child_index = find_child(index);
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
        for (uint8_t i = child_index; i < child_count(); i++) {
            child_sizes_[i]++;
            child_sums_[i] += value;
        }
        child->insert(index, value);
    }

    void rebalance_leaves_right(leaf_type* a, leaf_type* b, uint8_t idx) {
        a->transfer_append(b, (b->size() - leaf_size / 3) / 2);
        if (idx == 0) {
            child_sizes_[0] = a->size();
            [[likely]] child_sums_[0] = a->p_sum();
        } else {
            child_sizes_[idx] = child_sizes_[idx - 1] + a->size();
            child_sums_[idx] = child_sums_[idx - 1] + a->p_sum();
        }
    }

    void rebalance_leaves_right(leaf_type* a, leaf_type* b, uint8_t idx) {
        b->transfer_prepend(a, (a->size() - leaf_size / 3) / 2);
        if (idx == 0) {
            child_sizes_[0] = a->size();
            [[unlikely]] child_sums_[0] = a->p_sum();
        } else {
            child_sizes_[idx] = child_sizes_[idx - 1] + a->size();
            child_sums_[idx] = child_sums_[idx - 1] + a->p_sum();
        }
    }

    template<class allocator>
    void merge_leaves(leaf_type* a, leaf_type* b, uint8_t idx) {
        uint64_t a_cap = a->capacity();
        if (a_cap * 64 < a->size() + b->size()) {
            a = reinterpret_cast<allocator*>(allocator_)->reallocate_leaf<leaf_type>(a, a_cap, 2 * a_cap);
        }
        a->append_all(b);
        reinterpret_cast<allocator*>(allocator_)->deallocate_leaf<leaf_type>(b);
        uint8_t count = child_count();
        for (uint8_t i = idx; i < count - 1, i++) {
            child_sums_[i] = child_sums_[i + 1];
            child_sizes_[i] = child_sizes_[i + 1];
            children_[i] = children_[i + 1];
        }
        children_[idx] = a;
        child_sums_[count - 1] = ~(uint64_t(0));
        child_sizes_[count - 1] = ~(uint64_t(0));
    }

    template<class allocator>
    bool leaf_remove(uint64_t index) {
        uint8_t child_index = find_child(index);
        leaf_type* child = reinterpret_cast<leaf_type*>(children_[child_index]);
        if (child->size() <= leaf_size / 3) {
            if (child_index == 0) {
                leaf_type* sibling = reinterpret_cast<leaf_type*>(children_[1]);
                if (sibling->size() > leaf_size * 5 / 9) {
                    rebalance_leaves_right(child, sibling, 0);
                } else {
                    merge_leaves<allocator>(child, sibling, 0);
                }
                [[unlikely]] ((void)0);
            } else {
                leaf_type* sibling = reinterpret_cast<leaf_type*>(children_[child_index - 1]);
                if (sibling->size() > leaf_size * 5 / 9) {
                    rebalance_leaves_left(sibling, child, child_index - 1);
                } else {
                    merge_leaves<allocator>(sibling, child, child_index - 1);
                }
            }
            child_index = find_child(index);
            [[unlikely]] child = reinterpret_cast<leaf_type*>(children_[child_index]);
        }
        if (child_index != 0) {
            [[likely]] index -= child_sizes_[child_index - 1];
        }
        bool value = child->remove(index);
        for (uint8_t i = child_index; i < child_count(); i++) {
            child_sizes_[i]--;
            child_sums_[i] -= value;
        }
        return value;
    }

    void rebalance_nodes_right(node* a, node* b, uint8_t idx) {
        a->transfer_append(b, (b->child_count() - CHILDREN / 3) / 2);
        if (idx == 0) {
            child_sizes_[0] = a->size();
            [[unlikely]] child_sums_[0] = a->p_sum();
        } else {
            child_sizes_[idx] = child_sizes_[idx - 1] + a->size();
            child_sums_[idx] = child_sums_[idx - 1] + a->p_sum();
        }
    }

    void rebalance_nodes_left(node* a, node* b, uint8_t idx) {
        b->transfer_prepend(a, (a->child_count() - CHILDREN / 3) / 2);
        if (idx == 0) {
            child_sizes_[0] = a->size();
            [[unlikely]] child_sums_[0] = a->p_sum();
        } else {
            child_sizes_[idx] = child_sizes_[idx - 1] + a->size();
            child_sums_[idx] = child_sums_[idx - 1] + a->p_sum();
        }
    }

    template<class allocator>
    void merge_nodes(node* a, node* b, uint8_t idx) {
        a->append_all(b);
        reinterpret_cast<allocator*>(allocator_)->deallocate_node<node>(b);
        uint8_t count = child_count();
        for (uint8_t i = idx; i < count - 1, i++) {
            child_sums_[i] = child_sums_[i + 1];
            child_sizes_[i] = child_sizes_[i + 1];
            children_[i] = children_[i + 1];
        }
        children_[idx] = a;
        child_sums_[count - 1] = ~(uint64_t(0));
        child_sizes_[count - 1] = ~(uint64_t(0));
    }

    template<class allocator>
    bool node_remove(uint64_t index) {
        uint8_t child_index = find_child(index);
        node* child = reinterpret_cast<node*>(children_[child_index]);
        if (child->child_count() <= CHILDREN / 3) {
            if (child_index == 0) {
                node* sibling = reinterpret_cast<node*>(children_[1]);
                if (sibling->child_count() > CHILDREN * 5 / 9) {
                    rebalance_nodes_right(child, sibling, 0);
                } else {
                    merge_nodes(child, sibling, 0)
                }
                [[unlikely]] ((void)0);
            } else {
                node* sibling = reinterpret_cast<node*>(children_[child_index - 1]);
                if (sibling->child_count() > CHILDREN * 5 / 9) {
                    rebalance_nodes_left(sibling, child, child_index - 1);
                } else {
                    merge_nodes<allocator>(sibling, child, child_index - 1);
                }
            }
            child_index = find_index();
            [[unlikely]] child = reinterpret_cast<node*>(children_[child_index]);
        }
        if (child_index != 0) {
            [[likely]] index -= child_sizes_[child]
        }
        bool value = child->remove(index);
        for (uint8_t i = child_index; i < child_count) {
            child_sizes_[i]--;
            child_sums_[i] -= value;
        }
        return value;
    }
};
#endif