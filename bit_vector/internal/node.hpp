#ifndef BV_NODE_HPP
#define BV_NODE_HPP

#include <cassert>
#include <cstdint>
#include <cstring>

#ifndef CACHE_LINE
// Apparently the most common cache line size is 64.
#define CACHE_LINE 64
#endif

namespace bv {

/**
 * @brief Internal node for use with bit vector b-tree structures.
 *
 * Intended for use with bv::bit_vector and bv::leaf, to support the dynamic
 * b-tree bit vector structure.
 *
 * ### Practical limitations
 *
 * The maximum logical size of the bit vector is \f$2^{b - 1} - 1\f$ where b is
 * the number of bits available in dtype. This is due to the "sign bit" being
 * used to speed up branching. So for 64-bit words the maximum data structure
 * size is 9223372036854775807 and for 32-bot words the limit is 2147483647.
 *
 * The internal nodes need to keep track of leaf sizes, since splitting, merging
 * and balancing is done based on a top-down approach, where an insertion or
 * removal already submitted to a leaf **cannot** cause any rebalancing.
 *
 * The maximum leaf size practically needs to be divisible by 128 due to the
 * practical allocation scheme allocating an even amount of 64-bit integers for
 * leaf storage. In addition a minimum size of 128 causes edge cases in
 * balancing that can result in undefined behaviour. Thus a minimum leaf size of
 * 256 and divisibility of maximum leaf size by 128 is enforced by static
 * assertions.
 *
 * Buffered bv::leaf instances can not be bigger than \f$2^{24} - 1\f$.
 *
 * The branching factor for the internal nodes need to be 8, 16, 32, 64 or 128
 * due to how the branchless binary search is written. Further limited by the 7
 * bits available in the child counter.
 *
 * ### Cache line size
 *
 * Branch selection uses binary search, which significantly benefits from
 * agressive cache prefetching. It is suggested that a definition of
 * `CACHLE_LINE` is given with the value of the cache line size for the
 * (running) architecture. Make attempts to retrieve this information and
 * passing the correct value to the compiler with the `-DCACHE_LINE` flag.
 *
 * If the prefetching fails significantly due to prefetching being unavailable
 * or the wrong cache line size, the current binary search based branch
 * selection will likely be noticeably slower than a naive linear search.
 *
 * @tparam leaf_type Type of leaf to use in the tree structure (E.g. bv::leaf).
 * @tparam dtype     Integer type to use for indexing (uint32_t or uint64_t).
 * @tparam leaf_size Maximum size of a leaf node.
 * @tparam branches  Maximum branching factor of internal nodes.
 */
template <class leaf_type, class dtype, uint64_t leaf_size, uint8_t branches>
class node {
   protected:
    /**
     * @brief Bit indicating whether the nodes children are laves or nodes.
     */
    uint8_t meta_data_;  
    /**
     * @brief Number of active children.
     */
    uint8_t child_count_; // Bad word alignment. betewen 16 and 48 dead bits...
    /**
     * @brief Cumulative child sizes and `(~0) >> 1` for non-existing children.
     */
    dtype child_sizes_[branches];
    /**
     * @brief Cumulative child sums and `(~0) >> 1` for non-existint
     * children.
     */
    dtype child_sums_[branches];
    void* children_[branches];  ///< pointers to leaf_type or node children.

    static_assert(leaf_size >= 256, "leaf size needs to be a at least 256");
    static_assert((leaf_size % 128) == 0,
                  "leaf size needs to be divisible by 128");
    static_assert(leaf_size < 0xffffff, "leaf size must fit in 24 bits");
    static_assert((branches == 8) || (branches == 16) || (branches == 32) ||
                      (branches == 64) || (branches == 128),
                  "branching factor needs to be a reasonable power of 2");

   public:
    /**
     * @brief Constructor
     */
    node() {
        meta_data_ = 0;
        child_count_ = 0;
        for (uint8_t i = 0; i < branches; i++) {
            child_sizes_[i] = (~dtype(0)) >> 1;
            child_sums_[i] = (~dtype(0)) >> 1;
        }
    }

    template <class qds>
    void generate_query_structure(qds* qs) {
        if (has_leaves()) {
            leaf_type** children = reinterpret_cast<leaf_type**>(children_);
            for (size_t i = 0; i < child_count_; i++) {
                children[i]->commit();
                qs->append(children[i]);
            }
        } else {
            node** children = reinterpret_cast<node**>(children_);
            for (size_t i = 0; i < child_count_; i++) {
                children[i]->generate_query_structure(qs);
            }
        }
    }

    /**
     * @brief Set whether the children of the node are leaves or internal nodes
     *
     * Intended to be set according to the status of siblings following
     * allocation of the node.
     *
     * @param leaves Boolean value indicating if the children of this node are
     * leaves.
     */
    void has_leaves(bool leaves) {
        meta_data_ = leaves ? meta_data_ | 0b10000000 : meta_data_ & 0b01111111;
    }

    /**
     * @brief Get the type of children for the node.
     *
     * @return True if the children of this node are leaves.
     */
    bool has_leaves() const { return meta_data_ >> 7; }

    /**
     * @brief Access the value of the index<sup>th</sup> element of the logical
     * structure.
     *
     * Recurses to children based on cumulative sizes.
     *
     * @param index Index to access.
     */
    bool at(dtype index) const {
        uint8_t child_index = find_size(index + 1);
        index -= child_index != 0 ? child_sizes_[child_index - 1] : 0;
        if (has_leaves()) {
            [[unlikely]] return reinterpret_cast<leaf_type*>(
                children_[child_index])
                ->at(index);
        } else {
            return reinterpret_cast<node*>(children_[child_index])->at(index);
        }
    }

    /**
     * @brief Set the value of the logical index<sup>th</sup> element to v.
     *
     * Recurses to children based on cumulative sizes and updates partial sums
     * based on return value. Change is returned so parents can update partial
     * sums as well.
     *
     * @param index Index of element to set value of
     * @param v     Value to set element to.
     *
     * @return Change to data structure sum triggered by the set operation.
     */
    int set(dtype index, bool v) {
        uint8_t child_index = find_size(index + 1);
        index -= child_index == 0 ? 0 : child_sizes_[child_index - 1];
        dtype change = 0;
        if (has_leaves()) {
            leaf_type* child =
                reinterpret_cast<leaf_type*>(children_[child_index]);
            [[unlikely]] change = child->set(index, v);
        } else {
            node* child = reinterpret_cast<node*>(children_[child_index]);
            change = child->set(index, v);
        }
        uint8_t c_count = child_count_;
        for (uint8_t i = child_index; i < c_count; i++) {
            child_sums_[i] += change;
        }
        return change;
    }

    /**
     * @brief Counts number of one bits up to the index<sup>th</sup> logical
     * element.
     *
     * Recurses to children based on cumulative sizes and returns subtree rank
     * based on the result of the recurrence and local cumulative sums.
     *
     * @param index Number of elements to sum.
     *
     * @return \f$\sum_{i = 0}^{\mathrm{index - 1}} \mathrm{bv}[i]\f$.
     */
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
            node* child = reinterpret_cast<node*>(children_[child_index]);
            return res + child->rank(index);
        }
    }

    /**
     * @brief Calculates the index of the count<sup>tu</sup> 1-bit
     *
     * Recurses to children based on cumulative sums and returns subtree Select
     * based on the results of the recurrence and local cumulative sizes.
     *
     * @param count Number to sum up to
     *
     * @return \f$\underset{i \in [0..n)}{\mathrm{arg min}}\left(\sum_{j = 0}^i
     * \mathrm{bv}[j]\right) =  \f$ count.
     */
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
            node* child = reinterpret_cast<node*>(children_[child_index]);
            return res + child->select(count);
        }
    }

    /**
     * @brief Recursively deallocates all children.
     *
     * A separate allocator is needed in addition to `~node()` since binding
     * deallocation of children to the default destructor could lead to
     * deallocation of nodes that are still in use with other nodes.
     *
     * @tparam allocator Type of `alloc`.
     * @param alloc Allocator instance to use for deallocation.
     */
    template <class allocator>
    void deallocate(allocator* alloc) {
        if (has_leaves()) {
            leaf_type** children = reinterpret_cast<leaf_type**>(children_);
            for (uint8_t i = 0; i < child_count_; i++) {
                leaf_type* l = children[i];
                alloc->deallocate_leaf(l);
            }
        } else {
            node** children = reinterpret_cast<node**>(children_);
            for (uint8_t i = 0; i < child_count_; i++) {
                node* n = children[i];
                n->deallocate(alloc);
                alloc->deallocate_node(n);
            }
        }
    }

    /**
     * @brief Get the number of children of this node.
     *
     * @return Number of children.
     */
    uint8_t child_count() const { return child_count_; }

    /**
     * @brief Get pointer to the children of this node.
     *
     * Intended for efficient acces when balancing nodes.
     *
     * @returns Address to the array of children of this node.
     */
    void** children() { return children_; }

    /**
     * @brief Get pointer to the cumulative child sizes.
     *
     * Intended for efficient acces when balancing nodes.
     *
     * @returns Address of the array of cumulative child sizes.
     */
    dtype* child_sizes() { return child_sizes_; }

    /**
     * @brief Get pointer to the cumulative child sums.
     *
     * Intended for efficient acces when balancing nodes.
     *
     * @return Address of the array of cumulative child sums.
     */
    dtype* child_sums() { return child_sums_; }

    /**
     * @brief Logical number of elements stored in the subtree.
     *
     * @return Number of elements in subtree.
     */
    dtype size() const {
        return child_count_ > 0 ? child_sizes_[child_count_ - 1] : 0;
    }

    /**
     * @brief Logical number of 1-bits stored in the subtree.
     *
     * @return Number of 1-bits in subtree.
     */
    dtype p_sum() const {
        return child_count_ > 0 ? child_sums_[child_count_ - 1] : 0;
    }

    /**
     * @brief Add child to the subtree.
     *
     * Mainly intended for use by bv::bit_vector for splitting root nodes.
     *
     * @tparam child Type of child (node or leaf_type).
     *
     * @param new_child Child to append.
     */
    template <class child>
    void append_child(child* new_child) {
        if (child_count_ == 0) {
            child_sizes_[0] = new_child->size();
            child_sums_[0] = new_child->p_sum();
            [[unlikely]] children_[0] = new_child;
        } else {
            child_sizes_[child_count_] = child_sizes_[child_count_ - 1] + new_child->size();
            child_sums_[child_count_] = child_sums_[child_count_ - 1] + new_child->p_sum();
            children_[child_count_] = new_child;
        }
        child_count_++;
    }

    /**
     * @brief Get i<sup>th</sup> child of the node.
     *
     * Mainly intended for use by bv::bit_vector for decreasing tree height.
     *
     * @param i Index of child to return.
     *
     * @return Pointer to the i<sup>th</sup> child.
     */
    void* child(uint8_t i) { return children_[i]; }

    /**
     * @brief Insert "value" at "index".
     *
     * Inserts into the logical bit vector whlie ensuring that structural
     * invariants hold. Children will be rebalanced or split as necessary.
     *
     * @tparam allocator Type of `alloc`.
     *
     * @param index Location for insertion.
     * @param value Boolean indicating the value for the new element.
     * @param alloc Instance of allocator to use for allocation and
     * reallocation.
     */
    template <class allocator>
    void insert(dtype index, bool value, allocator* alloc) {
        if (has_leaves()) {
            leaf_insert(index, value, alloc);
        } else {
            [[likely]] node_insert(index, value, alloc);
        }
    }

    /**
     * @brief Remove the index<sup>th</sup> element.
     *
     * Removes element at "index" while ensuring that strctural invariants hold.
     * Children will be rebalanced and merger as necessary.
     *
     * @tparam allocator Type of `alloc`.
     *
     * @param index Location for removal.
     * @param alloc Allocator to use for reallocation and deallocation.
     */
    template <class allocator>
    bool remove(dtype index, allocator* alloc) {
        if (has_leaves()) {
            return leaf_remove(index, alloc);
        } else {
            [[likely]] return node_remove(index, alloc);
        }
    }

    /**
     * @brief Remove the first "elems" elements form this node.
     *
     * Internal cumulative sizes and sums will be updated, but removed nodes
     * will not be deallocated, as it is assumed that this is triggered after
     * copying elements to the "left" sibling.
     *
     * @param elems Number of elements to remove.
     */
    void clear_first(uint8_t elems) {
        dtype o_size = child_sizes_[elems - 1];
        dtype o_sum = child_sums_[elems - 1];
        for (uint8_t i = 0; i < child_count_ - elems; i++) {
            children_[i] = children_[i + elems];
            child_sizes_[i] = child_sizes_[i + elems] - o_size;
            child_sums_[i] = child_sums_[i + elems] - o_sum;
        }
        for (uint8_t i = child_count_ - elems; i < child_count_; i++) {
            child_sizes_[i] = (~dtype(0)) >> 1;
            child_sums_[i] = (~dtype(0)) >> 1;
        }
        child_count_ -= elems;
    }

    /**
     * @brief Moves the fist "elems" elements from "other" to the end of "this".
     *
     * Elements are first copied to this node, after which `clear_firs(elems)`
     * will be called on "other". Cumulative sizes and sums will be updated
     * accordingly.
     *
     * @param other Node to transfer elements from.
     * @param elems Number of elements to transfer.
     */
    void transfer_append(node* other, uint8_t elems) {
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
        child_count_ += elems;
        other->clear_first(elems);
    }

    /**
     * @brief Remove the last "elems" elemets from the node.
     *
     * Internal cumulative sizes and sums will be updated, but removed nodes
     * will not be deallocated, as it is assumed that this is triggered after
     * copying elements to the "right" sibling.
     *
     * @param elems Number of elements to remove
     */
    void clear_last(uint8_t elems) {
        for (uint8_t i = child_count_ - elems; i < child_count_; i++) {
            child_sizes_[i] = (~dtype(0)) >> 1;
            child_sums_[i] = (~dtype(0)) >> 1;
        }
        child_count_ -= elems;
    }

    /**
     * @brief Moves the last "elems" elements from "other" to the start of
     * "this".
     *
     * Elements are first copied to this node, after which `clear_firs(elems)`
     * will be called on "other". Cumulative sizes and sums will be updated
     * accordingly.
     *
     * @param other Node to transfer elements from.
     * @param elems Number of elements to transfer.
     */
    void transfer_prepend(node* other, uint8_t elems) {
        void** o_children = other->children();
        dtype* o_sizes = other->child_sizes();
        dtype* o_sums = other->child_sums();
        uint8_t o_size = other->child_count();
        memmove(children_ + elems, children_, child_count_ * sizeof(void*));
        memmove(child_sums_ + elems, child_sums_, child_count_ * sizeof(dtype));
        memmove(child_sizes_ + elems, child_sizes_, child_count_ * sizeof(dtype));
        for (uint8_t i = 0; i < elems; i++) {
            children_[i] = o_children[o_size - elems + i];
            child_sums_[i] =
                o_sums[o_size - elems + i] - o_sums[o_size - elems - 1];
            child_sizes_[i] =
                o_sizes[o_size - elems + i] - o_sizes[o_size - elems - 1];
        }
        for (uint8_t i = elems; i < elems + child_count_; i++) {
            child_sums_[i] += child_sums_[elems - 1];
            child_sizes_[i] += child_sizes_[elems - 1];
        }
        child_count_ += elems;
        other->clear_last(elems);
    }

    /**
     * @brief Copies all children from "other" to the end of this node.
     *
     * Internal cumulative sizes and sums will be updated for this node but not
     * for other. This is used when merging 2 nodes, where the "right" sibling
     * will be deallocated after copying.
     *
     * @param other Node to copy elements from.
     */
    void append_all(node* other) {
        void** o_children = other->children();
        dtype* o_sizes = other->child_sizes();
        dtype* o_sums = other->child_sums();
        uint8_t o_size = other->child_count();
        for (uint8_t i = 0; i < o_size; i++) {
            children_[child_count_ + i] = o_children[i];
            child_sums_[child_count_ + i] = o_sums[i] + child_sums_[child_count_ - 1];
            child_sizes_[child_count_ + i] = o_sizes[i] + child_sizes_[child_count_ - 1];
        }
        child_count_ += o_size;
    }

    /**
     * @brief Size of the subtree in "allocated" bits.
     *
     * Reports number of bits allocated for this subtree, Does not take into
     * account any sort of memory fragmentation or similar.
     *
     * @return Number of allocated bits.
     */
    uint64_t bits_size() const {
        uint64_t ret = sizeof(node) * 8;
        if (has_leaves()) {
            leaf_type* const* children =
                reinterpret_cast<leaf_type* const*>(children_);
            for (uint8_t i = 0; i < child_count_; i++) {
                ret += children[i]->bits_size();
            }
        } else {
            node* const* children = reinterpret_cast<node* const*>(children_);
            for (uint8_t i = 0; i < child_count_; i++) {
                ret += children[i]->bits_size();
            }
        }
        return ret;
    }

    void flush() {
        if (has_leaves()) {
            leaf_type** children = reinterpret_cast<leaf_type**>(children_);
            for (uint8_t i = 0; i < child_count_; i++) {
                children[i]->commit();
            }
        } else {
            node** children = reinterpret_cast<node**>(children_);
            for (uint8_t i = 0; i < child_count_; i++) {
                children[i]->flush();
            }
        }
    }

    /**
     * @brief Check that the subtree structure is internally consistent.
     *
     * Checks that cumulative sizes and sums agree with the reported sizes and
     * sums of the children.
     *
     * If the children are leaves, also checks the validity of capacity for the
     * child.
     *
     * If compiled with `NDEBUG` defined, this function will do essentially
     * nothing since all checks are based on C assertions.
     *
     * Also does not say anything about the correctness of the subtree given the
     * sequence of operations up to this point. Only guarantees that the
     * structure is internally consistent and "could" be the result of some
     * valid sequence of operations.
     *
     * Number of nodes are reported for verification of allocation counts.
     *
     * @return Number of nodes in this subtree.
     */
    uint64_t validate() const {
        uint64_t ret = 1;
        uint64_t child_s_sum = 0;
        uint64_t child_p_sum = 0;
        if (has_leaves()) {
            leaf_type* const* children =
                reinterpret_cast<leaf_type* const*>(children_);
            for (uint8_t i = 0; i < child_count_; i++) {
                uint64_t child_size = children[i]->size();
                assert(child_size >= leaf_size / 3);
                child_s_sum += child_size;
                assert(child_sizes_[i] == child_s_sum);
                child_p_sum += children[i]->p_sum();
                assert(child_sums_[i] == child_p_sum);
                assert(children[i]->capacity() * WORD_BITS <= leaf_size);
                assert(children[i]->size() <= leaf_size);
                ret += children[i]->validate();
            }
        } else {
            node* const* children = reinterpret_cast<node* const*>(children_);
            for (uint8_t i = 0; i < child_count_; i++) {
                uint64_t child_size = children[i]->size();
                assert(child_size >= branches / 3);
                child_s_sum += child_size;
                assert(child_sizes_[i] == child_s_sum);
                child_p_sum += children[i]->p_sum();
                assert(child_sums_[i] == child_p_sum);
                ret += children[i]->validate();
            }
        }
        return ret;
    }

    /**
     * @brief Outputs this subtree as json.
     *
     * Prints this and all subtrees recursively. A final new line will not be
     * output, since it is assumed that this is called from another internal
     * node or a root element like bv::bit_vector.
     *
     * @param internal_only If true, leaves will not output their data arrays to
     * save space
     */
    void print(bool internal_only) const {
        std::cout << "{\n\"type\": \"node\",\n"
                  << "\"has_leaves\": " << has_leaves() << ",\n"
                  << "\"child_count\": " << int(child_count_) << ",\n"
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
            leaf_type* const* children =
                reinterpret_cast<leaf_type* const*>(children_);
            for (uint8_t i = 0; i < child_count_; i++) {
                children[i]->print(internal_only);
                if (i != child_count_ - 1) {
                    std::cout << ",";
                }
                std::cout << "\n";
            }
        } else {
            node* const* children = reinterpret_cast<node* const*>(children_);
            for (uint8_t i = 0; i < child_count_; i++) {
                children[i]->print(internal_only);
                if (i != child_count_ - 1) {
                    std::cout << ",";
                }
                std::cout << "\n";
            }
        }
        std::cout << "]}";
    }

   protected:
    /**
     * @brief Find the lowest child index s.t. the cumulative size at the index
     * is at least q.
     *
     * This is implemented as a branchless binary search that uses the "sign
     * bit" for index manipulations instead of conditional moves. This is the
     * reason for limiting the maximum data structure size to `(~dtype(0)) >>
     * 1`.
     *
     * If \f$q > \f$ `size()`, this will return either the index of the last
     * child or the index following the last child. Either way, this will likely
     * very quickly lead to undefined behaviour and segmentation fault.
     *
     * If `size() > (~(dtype(0)) >> 1`, querying is considered undefined
     * behaviour.
     *
     * This conditionally compiles the binary search based on acceptable branch
     * factors. For low branching factors this is expected to be slightly slower
     * than efficient vectorized linear searches. For higher branchin factors
     * this branchless binary search should be faster as long as cache
     * performance is good. Agressibe prefetching is done in an attempt to
     * ensure that cache misses don't occur during querying. See
     * https://github.com/saskeli/search_microbench for a simple benchmark.
     *
     * @param q Query target
     * @return \f$\underset{i}{\mathrm{arg min}}(\mathrm{cum\_sizes}[i] \geq
     * q)\f$.
     */
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

    /**
     * @brief Find the lowest child index s.t. the cumulative sum at the index
     * is at least q.
     *
     * This is implemented as a branchless binary search that uses the "sign
     * bit" for index manipulations instead of conditional moves. This is the
     * reason for limiting the maximum data structure size to `(~dtype(0)) >>
     * 1`.
     *
     * If \f$q > \f$ `p_sum()`, this will return either the index of the last
     * child or the index following the last child. Either way, this will likely
     * very quickly lead to undefined behaviour and segmentation fault.
     *
     * If `p_sum() > (~(dtype(0)) >> 1`, querying is considered undefined
     * behaviour.
     *
     * This conditionally compiles the binary search based on acceptable branch
     * factors. For low branching factors this is expected to be slightly slower
     * than efficient vectorized linear searches. For higher branchin factors
     * this branchless binary search should be faster as long as cache
     * performance is good. Agressibe prefetching is done in an attempt to
     * ensure that cache misses don't occur during querying. See
     * https://github.com/saskeli/search_microbench for a simple benchmark.
     *
     * @param q Query target
     * @return \f$\underset{i}{\mathrm{arg min}}(\mathrm{cum\_sums}[i] \geq
     * q)\f$.
     */
    uint8_t find_sum(dtype q) const {
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

    /**
     * @brief Ensure that there is space for insertion in the child leaves.
     *
     * If there is sufficient space in either sibling, elements will be
     * transferred to the sibling with more room. If there is insufficient space
     * in the siblings, a new leaf is allocated and elements are transferred
     * from the target leaf to one of the siblings.
     *
     * Generally delays allocation of a new leaf as long as posisble, and when
     * reallocating, generates a new leaf that is \f$\approx \frac{2}{3}\f$
     * full.
     *
     * @tparam allocator Type of `alloc`.
     *
     * @param index Location of the full leaf.
     * @param leaf Pointer to the full leaf.
     * @param alloc Instance of allocator to use for reallocation.
     */
    template <class allocator>
    void rebalance_leaf(uint8_t index, leaf_type* leaf, allocator* alloc) {
        // Number of leaves that can fit in the "left" sibling (with potential
        // reallocation).
        uint32_t l_cap = 0;
        if (index > 0) {
            l_cap = child_sizes_[index - 1];
            l_cap -= index > 1 ? child_sizes_[index - 2] : 0;
            [[likely]] l_cap = leaf_size - l_cap;
        }
        // Number of leaves that can fir in the "right" sibling (with potential
        // reallocation).
        uint32_t r_cap = 0;
        if (index < child_count_ - 1) {
            r_cap = child_sizes_[index + 1];
            r_cap -= child_sizes_[index];
            [[likely]] r_cap = leaf_size - r_cap;
        }
        if (l_cap < 2 * leaf_size / 9 && r_cap < 2 * leaf_size / 9) {
            // Rebalancing without creating a new leaf is impossible
            // (impracitcal).
            leaf_type* a_child;
            leaf_type* b_child;
            leaf_type* new_child;
            if (index == 0) {
                // If the full leaf is the first child, a new leaf is created
                // between indexes 0 and 1.
                dtype n_elem = child_sizes_[1] / 3;
                dtype n_cap = (n_elem + 2 * WORD_BITS) / WORD_BITS;
                n_cap += n_cap % 2;
                n_cap = n_cap * WORD_BITS > leaf_size ? leaf_size / WORD_BITS : n_cap;
                a_child = reinterpret_cast<leaf_type*>(children_[0]);
                new_child = alloc->template allocate_leaf<leaf_type>(n_cap);
                b_child = reinterpret_cast<leaf_type*>(children_[1]);
                new_child->transfer_append(b_child, b_child->size() - n_elem);
                new_child->transfer_prepend(a_child, a_child->size() - n_elem);
                [[unlikely]] index++;
            } else {
                // If the full leaf is not the first child, a new leaf is
                // created to the left of the full leaf.
                a_child = reinterpret_cast<leaf_type*>(children_[index - 1]);
                b_child = reinterpret_cast<leaf_type*>(children_[index]);
                dtype n_elem = (a_child->size() + b_child->size()) / 3;
                dtype n_cap = (n_elem + 2 * WORD_BITS) / WORD_BITS;
                n_cap += n_cap % 2;
                n_cap = n_cap * WORD_BITS > leaf_size ? leaf_size / WORD_BITS : n_cap;
                new_child = alloc->template allocate_leaf<leaf_type>(n_cap);
                new_child->transfer_append(b_child, b_child->size() - n_elem);
                new_child->transfer_prepend(a_child, a_child->size() - n_elem);
            }
            // Update cumulative sizes and sums.
            for (uint8_t i = child_count_; i > index; i--) {
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
            [[unlikely]] child_count_++;
        } else if (r_cap > l_cap) {
            // Right sibling has more space than the left sibling. Move elements
            // to right sibling
            leaf_type* sibling =
                reinterpret_cast<leaf_type*>(children_[index + 1]);
            uint32_t n_size = sibling->size() + r_cap / 2;
            if (sibling->capacity() * WORD_BITS < n_size) {
                n_size = n_size / WORD_BITS + 1;
                n_size += n_size % 2;
                n_size = n_size * WORD_BITS <= leaf_size
                             ? n_size
                             : leaf_size / WORD_BITS;
                children_[index + 1] = alloc->reallocate_leaf(
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
            // Left sibling has more space than the right sibling. Move elements
            // to the left sibling.
            leaf_type* sibling =
                reinterpret_cast<leaf_type*>(children_[index - 1]);
            uint32_t n_size = sibling->size() + l_cap / 2;
            if (sibling->capacity() * WORD_BITS < n_size) {
                n_size = n_size / WORD_BITS + 1;
                n_size += n_size % 2;
                n_size = n_size * WORD_BITS <= leaf_size
                             ? n_size
                             : leaf_size / WORD_BITS;
                children_[index - 1] = alloc->reallocate_leaf(
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

    /**
     * @brief Insertion operation if the children are leaves.
     *
     * Reallocation and rebalancing will take place as necessary.
     *
     * @tparam allocator Type of `alloc`.
     *
     * @param index Location of insertion.
     * @param value Value to insert.
     * @param alloc Instance of alloctor to use for allocation and reallocation.
     */
    template <class allocator>
    void leaf_insert(dtype index, bool value, allocator* alloc) {
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
                rebalance_leaf(child_index, child, alloc);
            } else {
                dtype cap = child->capacity();
                children_[child_index] =
                    alloc->reallocate_leaf(child, cap, cap + 2);
            }
            return leaf_insert(index, value, alloc);
        }
        if (child_index != 0) {
            [[likely]] index -= child_sizes_[child_index - 1];
        }
        for (uint8_t i = child_index; i < child_count_; i++) {
            child_sizes_[i]++;
            child_sums_[i] += value;
        }
        child->insert(index, value);
    }

    /**
     * @brief Ensure that there is space for insertion in the child nodes.
     *
     * If there is sufficient space in either sibling, elements will be
     * transferred to the sibling with more room. If there is insufficient space
     * in the siblings, a new node is allocated and elements are transferred
     * from the target node to one of the siblings.
     *
     * Generally delays allocation of a new node as long as possible, and when
     * reallocating, generates a new node that is \f$\approx \frac{2}{3}\f$
     * full.
     *
     * @tparam allocator Type of `alloc`.
     *
     * @param index Location of the full node.
     * @param alloc Allocator instance to use for allocation.
     */
    template <class allocator>
    void rebalance_node(uint8_t index, allocator* alloc) {
        // Number of elements that can be added to the left sibling.
        uint32_t l_cap = 0;
        if (index > 0) {
            [[likely]] l_cap =
                branches -
                reinterpret_cast<node*>(children_[index - 1])->child_count();
        }
        // Number of elements that can be aded to the right sibling.
        uint32_t r_cap = 0;
        if (index < child_count_ - 1) {
            [[likely]] r_cap =
                branches -
                reinterpret_cast<node*>(children_[index + 1])->child_count();
        }
        node* a_node;
        node* b_node;
        if (l_cap <= 1 && r_cap <= 1) {
            // There is no room in either sibling.
            if (index == 0) {
                a_node = reinterpret_cast<node*>(children_[0]);
                b_node = reinterpret_cast<node*>(children_[1]);
                [[unlikely]] index++;
            } else {
                a_node = reinterpret_cast<node*>(children_[index - 1]);
                b_node = reinterpret_cast<node*>(children_[index]);
            }
            node* new_child = alloc->template allocate_node<node>();
            new_child->has_leaves(a_node->has_leaves());
            new_child->transfer_append(b_node, branches / 3);
            new_child->transfer_prepend(a_node, branches / 3);
            for (size_t i = child_count_; i > index; i--) {
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
            child_count_++;
            [[unlikely]] return;
        } else if (l_cap > r_cap) {
            // There is more room in the left sibling.
            a_node = reinterpret_cast<node*>(children_[index - 1]);
            b_node = reinterpret_cast<node*>(children_[index]);
            a_node->transfer_append(b_node, l_cap / 2);
            index--;
        } else {
            // There is more room in the right sibling.
            a_node = reinterpret_cast<node*>(children_[index]);
            b_node = reinterpret_cast<node*>(children_[index + 1]);
            b_node->transfer_prepend(a_node, r_cap / 2);
        }
        // Fix cumulative sums and sizes.
        if (index == 0) {
            child_sizes_[0] = a_node->size();
            [[unlikely]] child_sums_[0] = a_node->p_sum();
        } else {
            child_sizes_[index] = child_sizes_[index - 1] + a_node->size();
            child_sums_[index] = child_sums_[index - 1] + a_node->p_sum();
        }
    }

    /**
     * @brief Insertion operation if the children are internal nodes.
     *
     * Reallocation and rebalancing will take place as necessary.
     *
     * @tparam allocator Type of `alloc`.
     *
     * @param index Location of insertion.
     * @param value Value to insert.
     * @param alloc Allocator instance to use for allocation and reallocation.
     */
    template <class allocator>
    void node_insert(dtype index, bool value, allocator* alloc) {
        uint8_t child_index = find_size(index);
        node* child = reinterpret_cast<node*>(children_[child_index]);
#ifdef DEBUG
        if (child_index >= child_count_) {
            std::cout << int(child_index) << " >= " << int(child_count_)
                      << std::endl;
            assert(child_index < child_count_);
        }
#endif
        if (child->child_count() == branches) {
            rebalance_node(child_index, alloc);
            child_index = find_size(index);
            [[unlikely]] child =
                reinterpret_cast<node*>(children_[child_index]);
        }
        if (child_index != 0) {
            [[likely]] index -= child_sizes_[child_index - 1];
        }
        for (uint8_t i = child_index; i < child_count_; i++) {
            child_sizes_[i]++;
            child_sums_[i] += value;
        }
        child->insert(index, value, alloc);
    }

    /**
     * @brief Transfer elements from the "right" leaf to the "left" leaf.
     *
     * Intended for rebalancing when a removal targets the "left" leaf but
     * there are too few elements in the leaf to maintain structural invariants.
     *
     * Only called when the "left" leaf has index 0. Thus no need for the
     * target indexes for reallocation.
     *
     * @tparam allocator Type of `alloc`.
     *
     * @param a "Left" leaf.
     * @param b "Right" leaf.
     * @param alloc Allocator to use for allocation and reallocation.
     */
    template <class allocator>
    void rebalance_leaves_right(leaf_type* a, leaf_type* b, allocator* alloc) {
        dtype a_cap = a->capacity();
        dtype addition = (b->size() - leaf_size / 3) / 2;
        if (a_cap * WORD_BITS < a->size() + addition) {
            dtype n_cap = 1 + (a->size() + addition) / WORD_BITS;
            n_cap += n_cap % 2;
            n_cap =
                n_cap * WORD_BITS <= leaf_size ? n_cap : leaf_size / WORD_BITS;
            a = alloc->reallocate_leaf(a, a_cap, n_cap);
            children_[0] = a;
        }
        a->transfer_append(b, addition);
        child_sizes_[0] = a->size();
        child_sums_[0] = a->p_sum();
    }

    /**
     * @brief Transfer elements from the "left" leaf to the "right" leaf.
     *
     * Intended for rebalancing when a removal targets the "right" leaf but
     * there are too few elements in the leaf to maintain structural invariants.
     *
     * @tparam allocator Type of `alloc`.
     *
     * @param a "Left" leaf.
     * @param b "Right" leaf.
     * @param idx Index of the "left" leaf for use when reallocating.
     * @param alloc Allocator instance to use for allocation and reallocation.
     */
    template <class allocator>
    void rebalance_leaves_left(leaf_type* a, leaf_type* b, uint8_t idx,
                               allocator* alloc) {
        dtype b_cap = b->capacity();
        dtype addition = (a->size() - leaf_size / 3) / 2;
        if (b_cap * WORD_BITS < b->size() + addition) {
            dtype n_cap = 1 + (b->size() + addition) / WORD_BITS;
            n_cap += n_cap % 2;
            n_cap =
                n_cap * WORD_BITS <= leaf_size ? n_cap : leaf_size / WORD_BITS;
            b = alloc->reallocate_leaf(b, b_cap, n_cap);
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

    /**
     * @brief Merge the right leaf into the left leaf.
     *
     * Intended for when a removal would break structural invariant and there
     * are not enough elements in siblings to transfer elements wilhe
     * maintaining invariants.
     *
     * @tparam allocator Type of `alloc`
     *
     * @param a "Left" leaf.
     * @param b "Right" leaf.
     * @param idx Index of left leaf for reallocation.
     * @param alloc Allocator instance to use for reallocation and deallocation.
     */
    template <class allocator>
    void merge_leaves(leaf_type* a, leaf_type* b, uint8_t idx,
                      allocator* alloc) {
        dtype a_cap = a->capacity();
        if (a_cap * WORD_BITS < a->size() + b->size()) {
            dtype n_cap = 1 + (a->size() + b->size()) / WORD_BITS;
            n_cap += n_cap % 2;
            n_cap =
                n_cap * WORD_BITS <= leaf_size ? n_cap : leaf_size / WORD_BITS;
            a = alloc->reallocate_leaf(a, a_cap, n_cap);
        }
        a->append_all(b);
        alloc->deallocate_leaf(b);
        for (uint8_t i = idx; i < child_count_ - 1; i++) {
            child_sums_[i] = child_sums_[i + 1];
            child_sizes_[i] = child_sizes_[i + 1];
            children_[i] = children_[i + 1];
        }
        children_[idx] = a;
        child_sums_[child_count_ - 1] = (~dtype(0)) >> 1;
        child_sizes_[child_count_ - 1] = (~dtype(0)) >> 1;
        child_count_--;
    }

    /**
     * @brief Removal used when the children are leaves.
     *
     * Will maintain structural invarians by, reallocating and rebalancing as
     * necessary.
     *
     * @tparam Allocator Type of `alloc`.
     *
     * @param index Index of element to remove.
     * @param alloc Allocator instance to use for reallocation and deallocation.
     *
     * @return Value of removed element.
     */
    template <class allocator>
    bool leaf_remove(dtype index, allocator* alloc) {
        uint8_t child_index = find_size(index + 1);
        leaf_type* child = reinterpret_cast<leaf_type*>(children_[child_index]);
        if (child->size() <= leaf_size / 3) {
            if (child_index == 0) {
                leaf_type* sibling = reinterpret_cast<leaf_type*>(children_[1]);
                if (sibling->size() > leaf_size * 5 / 9) {
                    rebalance_leaves_right(child, sibling, alloc);
                } else {
                    merge_leaves(child, sibling, 0, alloc);
                }
                [[unlikely]] ((void)0);
            } else {
                leaf_type* sibling =
                    reinterpret_cast<leaf_type*>(children_[child_index - 1]);
                if (sibling->size() > leaf_size * 5 / 9) {
                    rebalance_leaves_left(sibling, child, child_index - 1,
                                          alloc);
                } else {
                    merge_leaves(sibling, child, child_index - 1, alloc);
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
        for (uint8_t i = child_index; i < child_count_; i++) {
            child_sizes_[i]--;
            child_sums_[i] -= value;
        }
        return value;
    }

    /**
     * @brief Transfer elements from the "right" node to the "left" node.
     *
     * Intended for rebalancing when a removal targets the "left" node but
     * there are too few elements in the node to ensure that structural
     * invariants are maintained.
     *
     * Only called when the "left" node has index 0. Thus no need for the
     * target indexes for reallocation.
     *
     * @tparam allocator Type of `allocator_`.
     *
     * @param a "Left" node.
     * @param b "Right" node.
     * @param idx Index of "left" node for updating cumulative sizes and sums.
     */
    void rebalance_nodes_right(node* a, node* b, uint8_t idx) {
        a->transfer_append(b, (b->child_count() - branches / 3) / 2);
        child_sizes_[idx] = a->size();
        [[unlikely]] child_sums_[idx] = a->p_sum();
    }

    /**
     * @brief Transfer elements from the "lef" node to the "right" node.
     *
     * Intended for rebalancing when a removal targets the "right" node but
     * there are too few elements in the node to ensure that structural
     * invariants are maintained.
     *
     * @tparam allocator Type of `allocator_`.
     *
     * @param a "Left" leaf.
     * @param b "Right" leaf.
     * @param idx Index of the Left node for use when updating cumulative sizes
     * and sums.
     */
    void rebalance_nodes_left(node* a, node* b, uint8_t idx) {
        b->transfer_prepend(a, (a->child_count() - branches / 3) / 2);
        if (idx == 0) {
            child_sizes_[0] = a->size();
            [[unlikely]] child_sums_[0] = a->p_sum();
        } else {
            child_sizes_[idx] = child_sizes_[idx - 1] + a->size();
            child_sums_[idx] = child_sums_[idx - 1] + a->p_sum();
        }
    }

    /**
     * @brief Transfer all from the "right" node to the "left" node.
     *
     * Intended for rebalancing when a removal may break structural invariants
     * but rebalancing is impractical due to fill rate of siblings being
     * \f$\approx \frac{1}{3}\f$. Nodes will be merged instead.
     *
     * @tparam allocator Type of `alloc`.
     *
     * @param a "Left" leaf.
     * @param b "Right" leaf.
     * @param idx Index of the "left" node for updating cumulative sums and
     * sizes.
     * @param alloc Allocator instance to use for deallocation.
     */
    template <class allocator>
    void merge_nodes(node* a, node* b, uint8_t idx, allocator* alloc) {
        a->append_all(b);
        alloc->deallocate_node(b);
        for (uint8_t i = idx; i < child_count_ - 1; i++) {
            child_sums_[i] = child_sums_[i + 1];
            child_sizes_[i] = child_sizes_[i + 1];
            children_[i] = children_[i + 1];
        }
        children_[idx] = a;
        child_sums_[child_count_ - 1] = (~dtype(0)) >> 1;
        child_sizes_[child_count_ - 1] = (~dtype(0)) >> 1;
        child_count_--;
    }

    /**
     * @brief Removal operation when children are internal nodes.
     *
     * Recursively call the removal to children that are internal nodes.
     * Maintains structural invariants by rebalancing and merging nodes as
     * necessary.
     *
     * @tparam allocator Type of `alloc`.
     *
     * @param index Index of element to be removed.
     * @param alloc Allocator instanve for eallocation and deallocatioin
     * @return Value of removed element.
     */
    template <class allocator>
    bool node_remove(dtype index, allocator* alloc) {
        uint8_t child_index = find_size(index + 1);
        node* child = reinterpret_cast<node*>(children_[child_index]);
        if (child->child_count_ <= branches / 3) {
            if (child_index == 0) {
                node* sibling = reinterpret_cast<node*>(children_[1]);
                if (sibling->child_count_ > branches * 5 / 9) {
                    rebalance_nodes_right(child, sibling, 0);
                } else {
                    merge_nodes(child, sibling, 0, alloc);
                }
                [[unlikely]] ((void)0);
            } else {
                node* sibling =
                    reinterpret_cast<node*>(children_[child_index - 1]);
                if (sibling->child_count_ > branches * 5 / 9) {
                    rebalance_nodes_left(sibling, child, child_index - 1);
                } else {
                    merge_nodes(sibling, child, child_index - 1, alloc);
                }
            }
            child_index = find_size(index + 1);
            [[unlikely]] child =
                reinterpret_cast<node*>(children_[child_index]);
        }
        if (child_index != 0) {
            [[likely]] index -= child_sizes_[child_index - 1];
        }
        bool value = child->remove(index, alloc);
        for (uint8_t i = child_index; i < child_count_; i++) {
            child_sizes_[i]--;
            child_sums_[i] -= value;
        }
        return value;
    }
};
}  // namespace bv
#endif