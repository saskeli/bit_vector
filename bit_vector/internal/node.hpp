#ifndef BV_SIMPLE_NODE_HPP
#define BV_SIMPLE_NODE_HPP

#include <cassert>
#include <cstdint>
#include <cstring>

#ifndef CACHE_LINE
#define CACHE_LINE 64
#endif

namespace bv {

template <class leaf_type, class dtype, uint64_t leaf_size, uint8_t branches>
class simple_node {
   protected:
    uint8_t meta_data_;  // meta_data_ & 0x80 if children are leaves
    dtype child_sizes_[branches];
    dtype child_sums_[branches];
    void* children_[branches];  // pointers to leaf_type or simple_node elements
    void* allocator_;           // This will be the allocator

    static_assert(leaf_size >= 256,
                  "leaf size needs to be a at least 256");
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
        index -= child_index != 0 ? child_sizes_[child_index - 1] : 0;
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

    uint8_t child_count() const { return meta_data_ & 0b01111111; }

    void** children() { return children_; }

    dtype* child_sizes() { return child_sizes_; }

    dtype* child_sums() { return child_sums_; }

    dtype size() const {
        return child_count() > 0 ? child_sizes_[child_count() - 1] : 0;
    }

    dtype p_sum() const {
        return child_count() > 0 ? child_sums_[child_count() - 1] : 0;
    }

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
            // std::cout << "Then this" << std::endl;
            leaf_insert<allocator>(index, value);
        } else {
            // std::cout << "First this" << std::endl;
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
        dtype base_size = local_index == 0 ? 0 : child_sizes_[local_index - 1];
        dtype base_sum = local_index == 0 ? 0 : child_sums_[local_index - 1];
        for (uint8_t i = 0; i < elems; i++) {
            children_[local_index] = o_children[i];
            child_sizes_[local_index] = base_size + o_sizes[i];
            child_sums_[local_index] = base_sum + o_sums[i];
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
        uint64_t ret = sizeof(simple_node) * 8;
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

    uint64_t validate() const {
        uint64_t ret = 1;
        uint64_t child_s_sum = 0;
        uint64_t child_p_sum = 0;
        if (has_leaves()) {
            leaf_type* const* children =
                reinterpret_cast<leaf_type* const*>(children_);
            for (uint8_t i = 0; i < child_count(); i++) {
                child_s_sum += children[i]->size();
                assert(child_sizes_[i] == child_s_sum);
                child_p_sum += children[i]->p_sum();
                assert(child_sums_[i] == child_p_sum);
                assert(children[i]->capacity() * WORD_BITS <= leaf_size);
                assert(children[i]->size() <= leaf_size);
                ret += children[i]->validate();
            }
        } else {
            simple_node* const* children =
                reinterpret_cast<simple_node* const*>(children_);
            for (uint8_t i = 0; i < child_count(); i++) {
                child_s_sum += children[i]->size();
                assert(child_sizes_[i] == child_s_sum);
                child_p_sum += children[i]->p_sum();
                assert(child_sums_[i] == child_p_sum);
                ret += children[i]->validate();
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
        constexpr dtype lines = CACHE_LINE / sizeof(dtype);
        for (dtype i = 0; i < branches; i += lines) {
            __builtin_prefetch(child_sizes_ + i);
        }
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
#ifdef DEBUG
        if (q > child_sums_[child_count() - 1]) {
            std::cerr << "Invalid select " << q << " > "
                      << child_sums_[child_count() - 1] << std::endl;
            assert(q <= child_sums_[child_count() - 1]);
        }
#endif
        constexpr dtype SIGN_BIT = ~((~dtype(0)) >> 1);
        constexpr dtype num_bits = sizeof(dtype) * 8;
        constexpr dtype lines = CACHE_LINE / sizeof(dtype);
        for (dtype i = 0; i < branches; i += lines) {
            __builtin_prefetch(child_sums_ + i);
        }
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
    void rebalance_leaf(uint8_t index, leaf_type* leaf) {
        uint32_t l_cap = 0;
        if (index > 0) {
            l_cap = child_sizes_[index - 1];
            l_cap -= index > 1 ? child_sizes_[index - 2] : 0;
            [[likely]] l_cap = leaf_size - l_cap;
        }
        uint32_t r_cap = 0;
        if (index < child_count() - 1) {
            r_cap = child_sizes_[index + 1];
            r_cap -= child_sizes_[index];
            [[likely]] r_cap = leaf_size - r_cap;
        }
        allocator* a = reinterpret_cast<allocator*>(allocator_);
        if (l_cap < 2 * leaf_size / 9 && r_cap < 2 * leaf_size / 9) {
            leaf_type* a_child;
            leaf_type* b_child;
            leaf_type* new_child;
            if (index == 0) {
                dtype n_elem = child_sizes_[1] / 3;
                dtype n_cap = (n_elem + 128) / 64;
                n_cap += n_cap % 2;
                n_cap = n_cap * 64 > leaf_size ? leaf_size / 64 : n_cap;
                a_child = reinterpret_cast<leaf_type*>(children_[0]);
                new_child = a->template allocate_leaf<leaf_type>(n_cap);
                b_child = reinterpret_cast<leaf_type*>(children_[1]);
                new_child->transfer_append(b_child, b_child->size() - n_elem);
                new_child->transfer_prepend(a_child, a_child->size() - n_elem);
                [[unlikely]] index++;
            } else {
                a_child = reinterpret_cast<leaf_type*>(children_[index - 1]);
                b_child = reinterpret_cast<leaf_type*>(children_[index]);
                dtype n_elem = (a_child->size() + b_child->size()) / 3;
                dtype n_cap = (n_elem + 128) / 64;
                n_cap += n_cap % 2;
                n_cap = n_cap * 64 > leaf_size ? leaf_size / 64 : n_cap;
                new_child = a->template allocate_leaf<leaf_type>(n_cap);
                new_child->transfer_append(b_child, b_child->size() - n_elem);
                new_child->transfer_prepend(a_child, a_child->size() - n_elem);
            }
            for (uint8_t i = child_count(); i > index; i--) {
                child_sizes_[i] = child_sizes_[i - 1];
                child_sums_[i] = child_sums_[i - 1];
                children_[i] = children_[i - 1];
            }
            if (index == 1) {
                child_sizes_[0] = a_child->size();
                [[unlikely]] child_sums_[0] = a_child->p_sum();
            } else {
                child_sizes_[index - 1] =
                    child_sizes_[index - 2] + a_child->size();
                child_sums_[index - 1] =
                    child_sums_[index - 2] + a_child->p_sum();
            }
            child_sizes_[index] = child_sizes_[index - 1] + new_child->size();
            child_sums_[index] = child_sums_[index - 1] + new_child->p_sum();
            children_[index] = new_child;
            [[unlikely]] meta_data_++;
        } else if (r_cap > l_cap) {
            leaf_type* sibling =
                reinterpret_cast<leaf_type*>(children_[index + 1]);
            uint32_t n_size = sibling->size() + r_cap / 2;
            if (sibling->capacity() * WORD_BITS < n_size) {
                n_size = n_size / WORD_BITS + 1;
                n_size += n_size % 2;
                n_size = n_size * WORD_BITS <= leaf_size
                             ? n_size
                             : leaf_size / WORD_BITS;
                children_[index + 1] = a->template reallocate_leaf(
                    sibling, sibling->capacity(), n_size);
                sibling = reinterpret_cast<leaf_type*>(children_[index + 1]);
            }
            sibling->transfer_prepend(leaf, r_cap / 2);
            child_sizes_[index] = index != 0
                                      ? child_sizes_[index - 1] + leaf->size()
                                      : leaf->size();
            child_sums_[index] = index != 0
                                     ? child_sums_[index - 1] + leaf->p_sum()
                                     : leaf->p_sum();
        } else {
            leaf_type* sibling =
                reinterpret_cast<leaf_type*>(children_[index - 1]);
            uint32_t n_size = sibling->size() + l_cap / 2;
            if (sibling->capacity() * WORD_BITS < n_size) {
                n_size = n_size / WORD_BITS + 1;
                n_size += n_size % 2;
                n_size = n_size * WORD_BITS <= leaf_size
                             ? n_size
                             : leaf_size / WORD_BITS;
                children_[index - 1] = a->template reallocate_leaf(
                    sibling, sibling->capacity(), n_size);
                sibling = reinterpret_cast<leaf_type*>(children_[index - 1]);
            }
            sibling->transfer_append(leaf, l_cap / 2);
            child_sizes_[index - 1] =
                index > 1 ? child_sizes_[index - 2] + sibling->size()
                          : sibling->size();
            child_sums_[index - 1] =
                index > 1 ? child_sums_[index - 2] + sibling->p_sum()
                          : sibling->p_sum();
            [[likely]] (void(0));
        }
    }

    template <class allocator>
    void leaf_insert(dtype index, bool value) {
        uint8_t child_index = find_size(index);
        leaf_type* child = reinterpret_cast<leaf_type*>(children_[child_index]);
#ifdef DEBUG
        if (child->size() > leaf_size) {
            std::cerr << child->size() << " > " << leaf_size << "\n";
            std::cerr << "invalid leaf size" << std::endl;
            assert(child->size() <= leaf_size);
        }
#endif
        if (child->need_realloc()) {
            if (child->size() >= leaf_size) {
                rebalance_leaf<allocator>(child_index, child);
            } else {
                dtype cap = child->capacity();
                children_[child_index] =
                    reinterpret_cast<allocator*>(allocator_)
                        ->reallocate_leaf(child, cap, cap + 2);
            }
            return leaf_insert<allocator>(index, value);
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
    void rebalance_node(uint8_t index) {
        uint32_t l_cap = 0;
        if (index > 0) {
            [[likely]] l_cap =
                branches - reinterpret_cast<simple_node*>(children_[index - 1])
                               ->child_count();
        }
        uint32_t r_cap = 0;
        if (index < child_count() - 1) {
            [[likely]] r_cap =
                branches - reinterpret_cast<simple_node*>(children_[index + 1])
                               ->child_count();
        }
        allocator* a = reinterpret_cast<allocator*>(allocator_);
        simple_node* a_node;
        simple_node* b_node;
        if (l_cap <= 1 && r_cap <= 1) {
            if (index == 0) {
                a_node = reinterpret_cast<simple_node*>(children_[0]);
                b_node = reinterpret_cast<simple_node*>(children_[1]);
                [[unlikely]] index++;
            } else {
                a_node = reinterpret_cast<simple_node*>(children_[index - 1]);
                b_node = reinterpret_cast<simple_node*>(children_[index]);
            }
            simple_node* new_child = a->template allocate_node<simple_node>();
            new_child->has_leaves(a_node->has_leaves());
            new_child->transfer_append(b_node, branches / 3);
            new_child->transfer_prepend(a_node, branches / 3);
            for (size_t i = child_count(); i > index; i--) {
                child_sizes_[i] = child_sizes_[i - 1];
                child_sums_[i] = child_sums_[i - 1];
                children_[i] = children_[i - 1];
            }
            if (index == 1) {
                child_sizes_[0] = a_node->size();
                [[unlikely]] child_sums_[0] = a_node->p_sum();
            } else {
                child_sizes_[index - 1] =
                    child_sizes_[index - 2] + a_node->size();
                child_sums_[index - 1] =
                    child_sums_[index - 2] + a_node->p_sum();
            }
            child_sizes_[index] = child_sizes_[index - 1] + new_child->size();
            child_sums_[index] = child_sums_[index - 1] + new_child->p_sum();
            children_[index] = new_child;
            meta_data_++;
            [[unlikely]] return;
        } else if (l_cap > r_cap) {
            a_node = reinterpret_cast<simple_node*>(children_[index - 1]);
            b_node = reinterpret_cast<simple_node*>(children_[index]);
            a_node->transfer_append(b_node, l_cap / 2);
            index--;
        } else {
            a_node = reinterpret_cast<simple_node*>(children_[index]);
            b_node = reinterpret_cast<simple_node*>(children_[index + 1]);
            b_node->transfer_prepend(a_node, r_cap / 2);
        }
        if (index == 0) {
            child_sizes_[0] = a_node->size();
            [[unlikely]] child_sums_[0] = a_node->p_sum();
        } else {
            child_sizes_[index] = child_sizes_[index - 1] + a_node->size();
            child_sums_[index] = child_sums_[index - 1] + a_node->p_sum();
        }
    }

    template <class allocator>
    void node_insert(dtype index, bool value) {
        uint8_t child_index = find_size(index);
        simple_node* child =
            reinterpret_cast<simple_node*>(children_[child_index]);
#ifdef DEBUG
        if (child_index >= child_count()) {
            std::cout << int(child_index) << " >= " << int(child_count())
                      << std::endl;
            assert(child_index < child_count());
        }
#endif
        if (child->child_count() == branches) {
            rebalance_node<allocator>(child_index);
            child_index = find_size(index);
            [[unlikely]] child =
                reinterpret_cast<simple_node*>(children_[child_index]);
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
        dtype addition = (b->size() - leaf_size / 3) / 2;
        if (a_cap * 64 < a->size() + addition) {
            dtype n_cap = 1 + (a->size() + addition) / 64;
            n_cap += n_cap % 2;
            n_cap =
                n_cap * WORD_BITS <= leaf_size ? n_cap : leaf_size / WORD_BITS;
            a = reinterpret_cast<allocator*>(allocator_)
                    ->reallocate_leaf(a, a_cap, n_cap);
            children_[0] = a;
        }
        a->transfer_append(b, addition);
        child_sizes_[0] = a->size();
        child_sums_[0] = a->p_sum();
    }

    template <class allocator>
    void rebalance_leaves_left(leaf_type* a, leaf_type* b, uint8_t idx) {
        dtype b_cap = b->capacity();
        dtype addition = (a->size() - leaf_size / 3) / 2;
        if (b_cap * 64 < b->size() + addition) {
            dtype n_cap = 1 + (b->size() + addition) / 64;
            n_cap += n_cap % 2;
            n_cap =
                n_cap * WORD_BITS <= leaf_size ? n_cap : leaf_size / WORD_BITS;
            b = reinterpret_cast<allocator*>(allocator_)
                    ->reallocate_leaf(b, b_cap, n_cap);
            children_[idx + 1] = b;
        }
        b->transfer_prepend(a, addition);
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
            dtype n_cap = 1 + (a->size() + b->size()) / 64;
            n_cap += n_cap % 2;
            n_cap =
                n_cap * WORD_BITS <= leaf_size ? n_cap : leaf_size / WORD_BITS;
            a = reinterpret_cast<allocator*>(allocator_)
                    ->reallocate_leaf(a, a_cap, n_cap);
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