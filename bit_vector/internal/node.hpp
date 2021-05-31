#ifndef BV_SIMPLE_NODE_HPP
#define BV_SIMPLE_NODE_HPP

#include <cstdint>
#include <cstring>
#include <cassert>

namespace bv {

template <class leaf_type, class dtype, uint64_t leaf_size, uint8_t branches>
class simple_node {
   protected:
    uint8_t meta_data_;  // meta_data_ & 0x80 if children are leaves
    dtype child_sizes_[branches];
    dtype child_sums_[branches];
    void* children_[branches];  // pointers to leaf_type or simple_node elements
    void* allocator_;           // This will be the allocator

    static_assert((leaf_size & (leaf_size - 1)) == 0,
                  "leaf size needs to be a power of 2");
    static_assert((leaf_size % 64) == 0,
                  "leaf size needs to be divisible by 64");
    static_assert(leaf_size < 0xffffff, "leaf size must fit in 24 bits");
    static_assert((branches == 8) || (branches == 16) || (branches == 32) ||
                      (branches == 64) || (branches == 128),
                  "branching factor needs to be a reasonable power of 2");

   public:
    simple_node(void* allocator) {
        meta_data_ = 0;
        allocator_ = allocator;
        for (uint8_t i = 0; i < branches; i++) {
            child_sizes_[i] = (~dtype(0)) >> 1;
            child_sums_[i] = (~dtype(0)) >> 1;
        }
    }

    void has_leaves(bool leaves) {
        meta_data_ = leaves ? meta_data_ | 0b10000000 : meta_data_ & 0b01111111;
    }

    bool has_leaves() const { return meta_data_ >> 7; }

    bool at(dtype index) const {
        uint8_t child_index = find_size(index + 1);
        index -= child_index == 0 ? 0 : child_sizes_[child_index - 1];
        if (has_leaves()) {
            [[unlikely]] return reinterpret_cast<leaf_type*>(
                children_[child_index])
                ->at(index);
        } else {
            return reinterpret_cast<simple_node*>(children_[child_index])
                ->at(index);
        }
    }

    dtype set(dtype index, bool v) {
        uint8_t child_index = find_size(index + 1);
        index -= child_index == 0 ? 0 : child_sizes_[child_index - 1];
        dtype change = 0;
        if (has_leaves()) {
            leaf_type* child =
                reinterpret_cast<leaf_type*>(children_[child_index]);
            [[unlikely]] change = child->set(index, v);
        } else {
            simple_node* child =
                reinterpret_cast<simple_node*>(children_[child_index]);
            change = child->set(index, v);
        }
        uint8_t c_count = child_count();
        for (uint8_t i = child_index; i < c_count; i++) {
            child_sums_[i] += change;
        }
        return change;
    }

    dtype rank(dtype index) const {
        uint8_t child_index = find_size(index);
        dtype res = 0;
        if (child_index != 0) {
            res = child_sums_[child_index - 1];
            [[likely]] index -= child_sizes_[child_index - 1];
        }
        if (has_leaves()) {
            leaf_type* child =
                reinterpret_cast<leaf_type*>(children_[child_index]);
            [[unlikely]] return res + child->rank(index);
        } else {
            simple_node* child =
                reinterpret_cast<simple_node*>(children_[child_index]);
            return res + child->rank(index);
        }
    }

    dtype select(dtype count) const {
        uint8_t child_index = find_sum(count);
        dtype res = 0;
        if (child_index != 0) {
            res = child_sizes_[child_index - 1];
            [[likely]] count -= child_sums_[child_index - 1];
        }
        if (has_leaves()) {
            leaf_type* child =
                reinterpret_cast<leaf_type*>(children_[child_index]);
            [[unlikely]] return res + child->select(count);
        } else {
            simple_node* child =
                reinterpret_cast<simple_node*>(children_[child_index]);
            return res + child->select(count);
        }
    }

    template <class allocator>
    void deallocate() {
        uint8_t c_count = child_count();
        allocator* a = reinterpret_cast<allocator*>(allocator_);
        if (has_leaves()) {
            leaf_type** children = reinterpret_cast<leaf_type**>(children_);
            for (uint8_t i = 0; i < c_count; i++) {
                leaf_type* l = children[i];
                a->deallocate_leaf(l);
            }
        } else {
            simple_node** children = reinterpret_cast<simple_node**>(children_);
            for (uint8_t i = 0; i < c_count; i++) {
                simple_node* n = children[i];
                n->template deallocate<allocator>();
                a->deallocate_node(n);
            }
        }
    }

    template <class allocator>
    simple_node* split() {
        allocator* alloc = reinterpret_cast<allocator*>(allocator_);
        simple_node* sibling = alloc->template allocate_node<simple_node>();
        if (has_leaves()) {
            sibling->has_leaves(true);
            leaf_type** children = reinterpret_cast<leaf_type**>(children_);
            for (size_t i = branches >> 1; i < branches; i++) {
                sibling->template append_child<leaf_type>(children[i]);
            }
        } else {
            simple_node** children = reinterpret_cast<simple_node**>(children_);
            for (size_t i = branches >> 1; i < branches; i++) {
                sibling->template append_child<simple_node>(children[i]);
            }
        }
        for (uint8_t i = branches >> 1; i < branches; i++) {
            child_sizes_[i] = (~dtype(0)) >> 1;
            child_sums_[i] = (~dtype(0)) >> 1;
        }
        meta_data_ &= 0b10000000;
        meta_data_ |= branches >> 1;
        return sibling;
    }

    uint8_t child_count() const { return meta_data_ & 0b01111111; }

    void** children() { return children_; }

    dtype* child_sizes() { return child_sizes_; }

    dtype* child_sums() { return child_sums_; }

    dtype size() const { return child_sizes_[child_count() - 1]; }

    dtype p_sum() const { return child_sums_[child_count() - 1]; }

    template <class child>
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

    void* child(uint8_t i) { return children_[i]; }

    template <class allocator>
    void insert(dtype index, bool value) {
        if (has_leaves()) {
            leaf_insert<allocator>(index, value);
        } else {
            [[likely]] node_insert<allocator>(index, value);
        }
    }

    template <class allocator>
    bool remove(dtype index) {
        if (has_leaves()) {
            return leaf_remove<allocator>(index);
        } else {
            [[likely]] return node_remove<allocator>(index);
        }
    }

    void clear_first(uint8_t elems) {
        dtype o_size = child_sizes_[elems - 1];
        dtype o_sum = child_sums_[elems - 1];
        uint8_t size = child_count();
        for (uint8_t i = 0; i < size - elems; i++) {
            children_[i] = children_[i + elems];
            child_sizes_[i] = child_sizes_[i + elems] - o_size;
            child_sums_[i] = child_sums_[i + elems] - o_sum;
        }
        for (uint8_t i = size - elems; i < size; i++) {
            child_sizes_[i] = (~dtype(0)) >> 1;
            child_sums_[i] = (~dtype(0)) >> 1;
        }
        meta_data_ -= elems;
    }

    void transfer_append(simple_node* other, uint8_t elems) {
        void** o_children = other->children();
        dtype* o_sizes = other->child_sizes();
        dtype* o_sums = other->child_sums();
        uint8_t local_index = child_count();
        uint8_t high_index = local_index - 1;
        for (uint8_t i = 0; i < elems; i++) {
            children_[local_index] = o_children[i];
            child_sizes_[local_index] = child_sizes_[high_index] + o_sizes[i];
            child_sums_[local_index] = child_sums_[high_index] + o_sums[i];
            local_index++;
        }
        meta_data_ += elems;
        other->clear_first(elems);
    }

    void clear_last(uint8_t elems) {
        uint8_t size = child_count();
        for (uint8_t i = size - elems; i < size; i++) {
            child_sizes_[i] = (~dtype(0)) >> 1;
            child_sums_[i] = (~dtype(0)) >> 1;
        }
        meta_data_ -= elems;
    }

    void transfer_prepend(simple_node* other, uint8_t elems) {
        void** o_children = other->children();
        dtype* o_sizes = other->child_sizes();
        dtype* o_sums = other->child_sums();
        uint8_t size = child_count();
        uint8_t o_size = other->child_count();
        memmove(children_ + elems, children_, size * sizeof(void*));
        memmove(child_sums_ + elems, child_sums_, size * sizeof(dtype));
        memmove(child_sizes_ + elems, child_sizes_, size * sizeof(dtype));
        for (uint8_t i = 0; i < elems; i++) {
            children_[i] = o_children[o_size - elems + i];
            child_sums_[i] =
                o_sums[o_size - elems + i] - o_sums[o_size - elems - 1];
            child_sizes_[i] =
                o_sizes[o_size - elems + i] - o_sizes[o_size - elems - 1];
        }
        for (uint8_t i = elems; i < elems + size; i++) {
            child_sums_[i] += child_sums_[elems - 1];
            child_sizes_[i] += child_sizes_[elems - 1];
        }
        meta_data_ += elems;
        other->clear_last(elems);
    }

    void append_all(simple_node* other) {
        void** o_children = other->children();
        dtype* o_sizes = other->child_sizes();
        dtype* o_sums = other->child_sums();
        uint8_t size = child_count();
        uint8_t o_size = other->child_count();
        for (uint8_t i = 0; i < o_size; i++) {
            children_[size + i] = o_children[i];
            child_sums_[size + i] = o_sums[i] + child_sums_[size - 1];
            child_sizes_[size + i] = o_sizes[i] + child_sizes_[size - 1];
        }
        meta_data_ += o_size;
    }

    uint64_t bits_size() const {
        uint64_t ret = sizeof(this) * 8;
        if (has_leaves()) {
            leaf_type* const* children =
                reinterpret_cast<leaf_type* const*>(children_);
            for (uint8_t i = 0; i < child_count(); i++) {
                ret += children[i]->bits_size();
            }
        } else {
            simple_node* const* children =
                reinterpret_cast<simple_node* const*>(children_);
            for (uint8_t i = 0; i < child_count(); i++) {
                ret += children[i]->bits_size();
            }
        }
        return ret;
    }

    void print(bool internal_only) const {
        std::cout << "{\n\"type\": \"node\",\n"
                  << "\"has_leaves\": " << has_leaves() << ",\n"
                  << "\"child_count\": " << int(child_count()) << ",\n"
                  << "\"size\": " << size() << ",\n"
                  << "\"child_sizes\": [";
        for (uint8_t i = 0; i < branches; i++) {
            std::cout << child_sizes_[i];
            if (i != branches - 1) {
                std::cout << ", ";
            }
        }
        std::cout << "],\n"
                  << "\"p_sum\": " << p_sum() << ",\n"
                  << "\"child_sums\": [";
        for (uint8_t i = 0; i < branches; i++) {
            std::cout << child_sums_[i];
            if (i != branches - 1) {
                std::cout << ", ";
            }
        }
        std::cout << "],\n"
                  << "\"children\": [\n";
        if (has_leaves()) {
            if (!internal_only) {
                leaf_type* const* children =
                    reinterpret_cast<leaf_type* const*>(children_);
                for (uint8_t i = 0; i < child_count(); i++) {
                    children[i]->print(internal_only);
                    if (i != child_count() - 1) {
                        std::cout << ",";
                    }
                    std::cout << "\n";
                }
            }
        } else {
            simple_node* const* children =
                reinterpret_cast<simple_node* const*>(children_);
            for (uint8_t i = 0; i < child_count(); i++) {
                children[i]->print(internal_only);
                if (i != child_count() - 1) {
                    std::cout << ",";
                }
                std::cout << "\n";
            }
        }
        std::cout << "]}";
    }

   protected:
    uint8_t find_size(dtype q) const {
        constexpr dtype SIGN_BIT = ~((~dtype(0)) >> 1);
        constexpr dtype num_bits = sizeof(dtype) * 8;
        uint8_t idx;
        if constexpr (branches == 128) {
            idx = (uint8_t(1) << 6) - 1;
            idx ^=
                (dtype((child_sizes_[idx] - q) & SIGN_BIT) >> (num_bits - 7)) |
                (uint8_t(1) << 5);
            idx ^=
                (dtype((child_sizes_[idx] - q) & SIGN_BIT) >> (num_bits - 6)) |
                (uint8_t(1) << 4);
            idx ^=
                (dtype((child_sizes_[idx] - q) & SIGN_BIT) >> (num_bits - 5)) |
                (uint8_t(1) << 3);
            idx ^=
                (dtype((child_sizes_[idx] - q) & SIGN_BIT) >> (num_bits - 4)) |
                (uint8_t(1) << 2);
        } else if constexpr (branches == 64) {
            idx = (uint8_t(1) << 5) - 1;
            idx ^=
                (dtype((child_sizes_[idx] - q) & SIGN_BIT) >> (num_bits - 6)) |
                (uint8_t(1) << 4);
            idx ^=
                (dtype((child_sizes_[idx] - q) & SIGN_BIT) >> (num_bits - 5)) |
                (uint8_t(1) << 3);
            idx ^=
                (dtype((child_sizes_[idx] - q) & SIGN_BIT) >> (num_bits - 4)) |
                (uint8_t(1) << 2);
        } else if constexpr (branches == 32) {
            idx = (uint8_t(1) << 4) - 1;
            idx ^=
                (dtype((child_sizes_[idx] - q) & SIGN_BIT) >> (num_bits - 5)) |
                (uint8_t(1) << 3);
            idx ^=
                (dtype((child_sizes_[idx] - q) & SIGN_BIT) >> (num_bits - 4)) |
                (uint8_t(1) << 2);
        } else if constexpr (branches == 16) {
            idx = (uint8_t(1) << 3) - 1;
            idx ^=
                (dtype((child_sizes_[idx] - q) & SIGN_BIT) >> (num_bits - 4)) |
                (uint8_t(1) << 2);
        } else {
            idx = (uint8_t(1) << 2) - 1;
        }
        idx ^= (dtype((child_sizes_[idx] - q) & SIGN_BIT) >> (num_bits - 3)) |
               (uint8_t(1) << 1);
        idx ^= (dtype((child_sizes_[idx] - q) & SIGN_BIT) >> (num_bits - 2)) |
               uint8_t(1);
        return idx ^
               (dtype((child_sizes_[idx] - q) & SIGN_BIT) >> (num_bits - 1));
    }

    uint8_t find_sum(dtype q) const {
        constexpr dtype SIGN_BIT = ~((~dtype(0)) >> 1);
        constexpr dtype num_bits = sizeof(dtype) * 8;
        uint8_t idx;
        if constexpr (branches == 128) {
            idx = (uint8_t(1) << 6) - 1;
            idx ^=
                (dtype((child_sums_[idx] - q) & SIGN_BIT) >> (num_bits - 7)) |
                (uint8_t(1) << 5);
            idx ^=
                (dtype((child_sums_[idx] - q) & SIGN_BIT) >> (num_bits - 6)) |
                (uint8_t(1) << 4);
            idx ^=
                (dtype((child_sums_[idx] - q) & SIGN_BIT) >> (num_bits - 5)) |
                (uint8_t(1) << 3);
            idx ^=
                (dtype((child_sums_[idx] - q) & SIGN_BIT) >> (num_bits - 4)) |
                (uint8_t(1) << 2);
        } else if constexpr (branches == 64) {
            idx = (uint8_t(1) << 5) - 1;
            idx ^=
                (dtype((child_sums_[idx] - q) & SIGN_BIT) >> (num_bits - 6)) |
                (uint8_t(1) << 4);
            idx ^=
                (dtype((child_sums_[idx] - q) & SIGN_BIT) >> (num_bits - 5)) |
                (uint8_t(1) << 3);
            idx ^=
                (dtype((child_sums_[idx] - q) & SIGN_BIT) >> (num_bits - 4)) |
                (uint8_t(1) << 2);
        } else if constexpr (branches == 32) {
            idx = (uint8_t(1) << 4) - 1;
            idx ^=
                (dtype((child_sums_[idx] - q) & SIGN_BIT) >> (num_bits - 5)) |
                (uint8_t(1) << 3);
            idx ^=
                (dtype((child_sums_[idx] - q) & SIGN_BIT) >> (num_bits - 4)) |
                (uint8_t(1) << 2);
        } else if constexpr (branches == 16) {
            idx = (uint8_t(1) << 3) - 1;
            idx ^=
                (dtype((child_sums_[idx] - q) & SIGN_BIT) >> (num_bits - 4)) |
                (uint8_t(1) << 2);
        } else {
            idx = (uint8_t(1) << 2) - 1;
        }
        idx ^= (dtype((child_sums_[idx] - q) & SIGN_BIT) >> (num_bits - 3)) |
               (uint8_t(1) << 1);
        idx ^= (dtype((child_sums_[idx] - q) & SIGN_BIT) >> (num_bits - 2)) |
               uint8_t(1);
        return idx ^
               (dtype((child_sums_[idx] - q) & SIGN_BIT) >> (num_bits - 1));
    }

    template <class allocator>
    void leaf_insert(dtype index, bool value) {
        uint8_t child_index = find_size(index);
        leaf_type* child = reinterpret_cast<leaf_type*>(children_[child_index]);
        if (child->need_realloc()) {
            if (child->size() >= leaf_size) {
                constexpr dtype n_cap = leaf_size / (64 * 2);
                allocator* a = reinterpret_cast<allocator*>(allocator_);
                leaf_type* new_child =
                    a->template allocate_leaf<leaf_type>(n_cap);
                new_child->transfer_append(child, n_cap * 64);
                for (size_t i = child_count(); i > child_index; i--) {
                    child_sizes_[i] = child_sizes_[i - 1];
                    child_sums_[i] = child_sums_[i - 1];
                    children_[i] = children_[i - 1];
                }
                children_[child_index] = new_child;
                if (child_index == 0) {
                    child_sizes_[0] = new_child->size();
                    [[unlikely]] child_sums_[0] = new_child->p_sum();
                } else {
                    child_sizes_[child_index] =
                        child_sizes_[child_index - 1] + new_child->size();
                    child_sums_[child_index] =
                        child_sums_[child_index - 1] + new_child->p_sum();
                }
                [[unlikely]] meta_data_++;
            } else {
                dtype cap = child->capacity();
                child = reinterpret_cast<allocator*>(allocator_)
                            ->reallocate_leaf(child, cap, cap * 2);
                children_[child_index] = child;
            }
            [[unlikely]] return leaf_insert<allocator>(index, value);
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

    template <class allocator>
    void node_insert(dtype index, bool value) {
        uint8_t child_index = find_size(index);
        simple_node* child =
            reinterpret_cast<simple_node*>(children_[child_index]);
        if (child_index >= child_count()) {
            print(true);
            std::cerr << "\nInvalid child index " << int(child_index) << std::endl;
            assert(child_index < child_count());
        }
        if (child->child_count() == branches) {
            simple_node* new_child = child->template split<allocator>();
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
                child_sizes_[child_index] =
                    child_sizes_[child_index - 1] + child->size();
                child_sums_[child_index] =
                    child_sums_[child_index - 1] + child->p_sum();
            }
            if (child_sizes_[child_index] < index) {
                child = new_child;
                child_index++;
            }
            [[unlikely]] meta_data_++;
        }
        if (child_index != 0) {
            [[likely]] index -= child_sizes_[child_index - 1];
        }
        for (uint8_t i = child_index; i < child_count(); i++) {
            child_sizes_[i]++;
            child_sums_[i] += value;
        }
        child->template insert<allocator>(index, value);
    }

    template <class allocator>
    void rebalance_leaves_right(leaf_type* a, leaf_type* b) {
        dtype a_cap = a->capacity();
        if (a_cap * 64 < a->size() + (b->size() - leaf_size / 3) / 2) {
            a = reinterpret_cast<allocator*>(allocator_)
                    ->reallocate_leaf(a, a_cap, 2 * a_cap);
            children_[0] = a;
        }
        a->transfer_append(b, (b->size() - leaf_size / 3) / 2);
        child_sizes_[0] = a->size();
        child_sums_[0] = a->p_sum();
    }

    template <class allocator>
    void rebalance_leaves_left(leaf_type* a, leaf_type* b, uint8_t idx) {
        dtype b_cap = b->capacity();
        if (b_cap * 64 < b->size() + (a->size() - leaf_size / 3) / 2) {
            b = reinterpret_cast<allocator*>(allocator_)
                    ->reallocate_leaf(b, b_cap, 2 * b_cap);
            children_[idx + 1] = b;
        }
        b->transfer_prepend(a, (a->size() - leaf_size / 3) / 2);
        if (idx == 0) {
            child_sizes_[0] = a->size();
            [[unlikely]] child_sums_[0] = a->p_sum();
        } else {
            child_sizes_[idx] = child_sizes_[idx - 1] + a->size();
            child_sums_[idx] = child_sums_[idx - 1] + a->p_sum();
        }
    }

    template <class allocator>
    void merge_leaves(leaf_type* a, leaf_type* b, uint8_t idx) {
        dtype a_cap = a->capacity();
        if (a_cap * 64 < a->size() + b->size()) {
            a = reinterpret_cast<allocator*>(allocator_)
                    ->reallocate_leaf(a, a_cap, 2 * a_cap);
        }
        a->append_all(b);
        reinterpret_cast<allocator*>(allocator_)->deallocate_leaf(b);
        uint8_t count = child_count();
        for (uint8_t i = idx; i < count - 1; i++) {
            child_sums_[i] = child_sums_[i + 1];
            child_sizes_[i] = child_sizes_[i + 1];
            children_[i] = children_[i + 1];
        }
        children_[idx] = a;
        child_sums_[count - 1] = (~dtype(0)) >> 1;
        child_sizes_[count - 1] = (~dtype(0)) >> 1;
        meta_data_--;
    }

    template <class allocator>
    bool leaf_remove(dtype index) {
        uint8_t child_index = find_size(index + 1);
        // std::cout << "removing " << index << ", which is at leaf " <<
        // int(child_index) << std::endl;
        leaf_type* child = reinterpret_cast<leaf_type*>(children_[child_index]);
        if (child->size() <= leaf_size / 3) {
            if (child_index == 0) {
                leaf_type* sibling = reinterpret_cast<leaf_type*>(children_[1]);
                if (sibling->size() > leaf_size * 5 / 9) {
                    rebalance_leaves_right<allocator>(child, sibling);
                } else {
                    merge_leaves<allocator>(child, sibling, 0);
                }
                [[unlikely]] ((void)0);
            } else {
                leaf_type* sibling =
                    reinterpret_cast<leaf_type*>(children_[child_index - 1]);
                if (sibling->size() > leaf_size * 5 / 9) {
                    rebalance_leaves_left<allocator>(sibling, child,
                                                     child_index - 1);
                } else {
                    merge_leaves<allocator>(sibling, child, child_index - 1);
                }
            }
            child_index = find_size(index);
            [[unlikely]] child =
                reinterpret_cast<leaf_type*>(children_[child_index]);
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

    void rebalance_nodes_right(simple_node* a, simple_node* b, uint8_t idx) {
        a->transfer_append(b, (b->child_count() - branches / 3) / 2);
        child_sizes_[idx] = a->size();
        [[unlikely]] child_sums_[idx] = a->p_sum();
    }

    void rebalance_nodes_left(simple_node* a, simple_node* b, uint8_t idx) {
        b->transfer_prepend(a, (a->child_count() - branches / 3) / 2);
        if (idx == 0) {
            child_sizes_[0] = a->size();
            [[unlikely]] child_sums_[0] = a->p_sum();
        } else {
            child_sizes_[idx] = child_sizes_[idx - 1] + a->size();
            child_sums_[idx] = child_sums_[idx - 1] + a->p_sum();
        }
    }

    template <class allocator>
    void merge_nodes(simple_node* a, simple_node* b, uint8_t idx) {
        a->append_all(b);
        reinterpret_cast<allocator*>(allocator_)->deallocate_node(b);
        uint8_t count = child_count();
        for (uint8_t i = idx; i < count - 1; i++) {
            child_sums_[i] = child_sums_[i + 1];
            child_sizes_[i] = child_sizes_[i + 1];
            children_[i] = children_[i + 1];
        }
        children_[idx] = a;
        child_sums_[count - 1] = (~dtype(0)) >> 1;
        child_sizes_[count - 1] = (~dtype(0)) >> 1;
        meta_data_--;
    }

    template <class allocator>
    bool node_remove(dtype index) {
        uint8_t child_index = find_size(index + 1);
        simple_node* child =
            reinterpret_cast<simple_node*>(children_[child_index]);
        if (child->child_count() <= branches / 3) {
            if (child_index == 0) {
                simple_node* sibling =
                    reinterpret_cast<simple_node*>(children_[1]);
                if (sibling->child_count() > branches * 5 / 9) {
                    rebalance_nodes_right(child, sibling, 0);
                } else {
                    merge_nodes<allocator>(child, sibling, 0);
                }
                [[unlikely]] ((void)0);
            } else {
                simple_node* sibling =
                    reinterpret_cast<simple_node*>(children_[child_index - 1]);
                if (sibling->child_count() > branches * 5 / 9) {
                    rebalance_nodes_left(sibling, child, child_index - 1);
                } else {
                    merge_nodes<allocator>(sibling, child, child_index - 1);
                }
            }
            child_index = find_size(index + 1);
            [[unlikely]] child =
                reinterpret_cast<simple_node*>(children_[child_index]);
        }
        if (child_index != 0) {
            [[likely]] index -= child_sizes_[child_index - 1];
        }
        bool value = child->template remove<allocator>(index);
        uint8_t c_count = child_count();
        for (uint8_t i = child_index; i < c_count; i++) {
            child_sizes_[i]--;
            child_sums_[i] -= value;
        }
        return value;
    }
};
}  // namespace bv
#endif