#ifndef BV_NODE_HPP
#define BV_NODE_HPP

#include <cassert>
#include <cstdint>
#include <cstring>
#include <utility>

#ifndef CACHE_LINE
// Apparently the most common cache line size is 64.
#define CACHE_LINE 64
#endif

#include "branch_selection.hpp"
#include "uncopyable.hpp"

//#include "deb.hpp"

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
 * @tparam aggressive_realloc If true, leaves will only be allowed to have at
 * most 256 elements of unused capacity
 * @tparam compressed If true, additional bookkeeping will be done to
 * ensure compressed leaves behave correctly.
 */
template <class leaf_type, class dtype, uint32_t leaf_size, uint16_t branches,
          bool aggressive_realloc = false, bool compressed = false>
class node : uncopyable {
   private:
    typedef branchless_scan<dtype, branches> branching;
    /**
     * @brief Bit indicating whether the nodes children are laves or nodes.
     */
    uint8_t meta_data_;
    /**
     * @brief Number of active children.
     */
    uint16_t child_count_;  // Bad word alignment. betewen 16 and 48 dead bits...
    /**
     * @brief Cumulative child sizes and `(~0) >> 1` for non-existing children.
     */
    branching child_sizes_;
    /**
     * @brief Cumulative child sums and `(~0) >> 1` for non-existint
     * children.
     */
    branching child_sums_;
    void* children_[branches];  ///< pointers to bv::leaf or bv::node children.

    /** @brief Number of bits in a computer word. */
    static const constexpr uint64_t WORD_BITS = 64;

    static_assert(leaf_size >= 256, "leaf size needs to be a at least 256");
    static_assert((leaf_size % 128) == 0,
                  "leaf size needs to be divisible by 128");
    static_assert(leaf_size < 0xffffff, "leaf size must fit in 24 bits");
    static_assert(branches > 2, "Convenient shortcuts and assumptions if this holds.");

   public:
    /**
     * @brief Constructor
     */
    node() : meta_data_(0), child_count_(0), child_sizes_(), child_sums_() {}

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
        uint16_t child_index = child_sizes_.find(index + 1);
        index -= child_index != 0 ? child_sizes_.get(child_index - 1) : 0;
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
    template <class allocator>
    int set(dtype index, bool v, allocator alloc) {
        uint16_t child_index = child_sizes_.find(index + 1);
        int change = 0;
        if (has_leaves()) {
            leaf_type* child =
                reinterpret_cast<leaf_type*>(children_[child_index]);
            if constexpr (compressed) {
                if (child->is_compressed() && child->need_realloc()) {
                    std::cerr << "Compressed leaf desires reallocation" << std::endl;
                    dtype cap = child->capacity();
                    dtype n_cap = child->desired_capacity();
                    if (cap * WORD_BITS >= leaf_size || n_cap * WORD_BITS >= leaf_size) {
                        std::cerr << " -> rebalancing" << std::endl;
                        rebalance_leaf(child_index, child, alloc);
                    } else {
                        std::cerr << " -> reallocating" << std::endl;
                        children_[child_index] = alloc->reallocate_leaf(
                            child, cap, n_cap);
                    }
                    child_index = child_sizes_.find(index + 1);
                    child =
                        reinterpret_cast<leaf_type*>(children_[child_index]);
                    [[unlikely]] (void(0));
                }
            }
            index -= child_index != 0 ? child_sizes_.get(child_index - 1) : 0;
            [[unlikely]] change = child->set(index, v);
        } else {
            node* child = reinterpret_cast<node*>(children_[child_index]);
            if constexpr (compressed) {
                if (child->child_count() == branches) {
                    rebalance_node(child_index, alloc);
                    child_index = child_sizes_.find(index);
                    [[unlikely]] child =
                        reinterpret_cast<node*>(children_[child_index]);
                }
            }
            index -= child_index != 0 ? child_sizes_.get(child_index - 1) : 0;
            change = child->set(index, v, alloc);
        }
        uint16_t c_count = child_count_;
        child_sums_.increment(child_index, c_count, change);
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
        uint16_t child_index = child_sizes_.find(index);
        dtype res = 0;
        if (child_index != 0) {
            res = child_sums_.get(child_index - 1);
            [[likely]] index -= child_sizes_.get(child_index - 1);
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
        uint16_t child_index = child_sums_.find(count);
        dtype res = 0;
        if (child_index != 0) {
            res = child_sizes_.get(child_index - 1);
            [[likely]] count -= child_sums_.get(child_index - 1);
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
            for (uint16_t i = 0; i < child_count_; i++) {
                leaf_type* l = children[i];
                alloc->deallocate_leaf(l);
            }
        } else {
            node** children = reinterpret_cast<node**>(children_);
            for (uint16_t i = 0; i < child_count_; i++) {
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
    uint16_t child_count() const { return child_count_; }

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
    branching* child_sizes() { return &child_sizes_; }

    /**
     * @brief Get pointer to the cumulative child sums.
     *
     * Intended for efficient acces when balancing nodes.
     *
     * @return Address of the array of cumulative child sums.
     */
    branching* child_sums() { return &child_sums_; }

    /**
     * @brief Logical number of elements stored in the subtree.
     *
     * @return Number of elements in subtree.
     */
    dtype size() const {
        return child_count_ > 0 ? child_sizes_.get(child_count_ - 1) : 0;
    }

    /**
     * @brief Logical number of 1-bits stored in the subtree.
     *
     * @return Number of 1-bits in subtree.
     */
    dtype p_sum() const {
        return child_count_ > 0 ? child_sums_.get(child_count_ - 1) : 0;
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
        child_sizes_.append(child_count_, new_child->size());
        child_sums_.append(child_count_, new_child->p_sum());
        children_[child_count_] = new_child;
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
    void* child(uint16_t i) { return children_[i]; }

    /**
     * @brief Insert "value" at "index".
     *
     * Inserts into the logical bit vector while ensuring that structural
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
     * Removes element at "index" while ensuring that structural invariants
     * hold. Children will be rebalanced and merger as necessary.
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
    void clear_first(uint16_t elems) {
        child_sizes_.clear_first(elems, child_count_);
        child_sums_.clear_first(elems, child_count_);
        for (uint16_t i = 0; i < child_count_ - elems; i++) {
            children_[i] = children_[i + elems];
        }
        child_count_ -= elems;
    }

    /**
     * @brief Moves the first "elems" elements from "other" to the end of
     * "this".
     *
     * Elements are first copied to this node, after which `clear_firs(elems)`
     * will be called on "other". Cumulative sizes and sums will be updated
     * accordingly.
     *
     * @param other Node to transfer elements from.
     * @param elems Number of elements to transfer.
     */
    void transfer_append(node* other, uint16_t elems) {
        void** o_children = other->children();
        branching* o_sizes = other->child_sizes();
        branching* o_sums = other->child_sums();
        uint16_t local_index = child_count_;
        for (uint16_t i = 0; i < elems; i++) {
            children_[local_index] = o_children[i];
            local_index++;
        }
        child_sizes_.append(elems, child_count_, o_sizes);
        child_sums_.append(elems, child_count_, o_sums);
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
    void clear_last(uint16_t elems) {
        child_sizes_.clear_last(elems, child_count_);
        child_sums_.clear_last(elems, child_count_);
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
    void transfer_prepend(node* other, uint16_t elems) {
        void** o_children = other->children();
        branching* o_sizes = other->child_sizes();
        branching* o_sums = other->child_sums();
        uint16_t o_size = other->child_count();
        memmove(children_ + elems, children_, child_count_ * sizeof(void*));
        for (uint16_t i = 0; i < elems; i++) {
            children_[i] = o_children[o_size - elems + i];
        }
        child_sizes_.prepend(elems, child_count_, o_size, o_sizes);
        child_sums_.prepend(elems, child_count_, o_size, o_sums);
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
        branching* o_sizes = other->child_sizes();
        branching* o_sums = other->child_sums();
        uint16_t o_size = other->child_count();
        for (uint16_t i = 0; i < o_size; i++) {
            children_[child_count_ + i] = o_children[i];
        }
        child_sizes_.append(o_size, child_count_, o_sizes);
        child_sums_.append(o_size, child_count_, o_sums);
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
            for (uint16_t i = 0; i < child_count_; i++) {
                ret += children[i]->bits_size();
            }
        } else {
            node* const* children = reinterpret_cast<node* const*>(children_);
            for (uint16_t i = 0; i < child_count_; i++) {
                ret += children[i]->bits_size();
            }
        }
        return ret;
    }

    void flush() {
        if (has_leaves()) {
            leaf_type** children = reinterpret_cast<leaf_type**>(children_);
            for (uint16_t i = 0; i < child_count_; i++) {
                children[i]->flush();
            }
        } else {
            node** children = reinterpret_cast<node**>(children_);
            for (uint16_t i = 0; i < child_count_; i++) {
                children[i]->flush();
            }
        }
    }

    uint64_t dump(uint64_t* data, uint64_t offset) {
        if (has_leaves()) {
            leaf_type** children = reinterpret_cast<leaf_type**>(children_);
            for (uint16_t i = 0; i < child_count_; i++) {
                offset = children[i]->dump(data, offset);
            }
        } else {
            node** children = reinterpret_cast<node**>(children_);
            for (uint16_t i = 0; i < child_count_; i++) {
                offset = children[i]->dump(data, offset);
            }
        }
        return offset;
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
            for (uint16_t i = 0; i < child_count_; i++) {
                uint64_t child_size = children[i]->size();
                assert(child_size >= leaf_size / 3);
                child_s_sum += child_size;
                assert(child_sizes_.get(i) == child_s_sum);
                child_p_sum += children[i]->p_sum();
                assert(child_sums_.get(i) == child_p_sum);
                assert(children[i]->capacity() * WORD_BITS <= leaf_size);
                if constexpr (!compressed) {
                    assert(children[i]->size() <= leaf_size);
                }
                ret += children[i]->validate();
            }
        } else {
            node* const* children = reinterpret_cast<node* const*>(children_);
            for (uint16_t i = 0; i < child_count_; i++) {
                uint64_t child_size = children[i]->size();
                assert(child_size >= branches / 3);
                child_s_sum += child_size;
                assert(child_sizes_.get(i) == child_s_sum);
                child_p_sum += children[i]->p_sum();
                assert(child_sums_.get(i) == child_p_sum);
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
    std::ostream& print(bool internal_only = true, std::ostream& out = std::cout) const {
        out << "{\n\"type\": \"node\",\n"
                  << "\"has_leaves\": " << (has_leaves() ? "true" : "false")
                  << ",\n"
                  << "\"child_count\": " << int(child_count_) << ",\n"
                  << "\"size\": " << size() << ",\n"
                  << "\"child_sizes\": [";
        for (uint16_t i = 0; i < branches; i++) {
            out << child_sizes_.get(i);
            if (i != branches - 1) {
                out << ", ";
            }
        }
        out << "],\n"
                  << "\"p_sum\": " << p_sum() << ",\n"
                  << "\"child_sums\": [";
        for (uint16_t i = 0; i < branches; i++) {
            out << child_sums_.get(i);
            if (i != branches - 1) {
                out << ", ";
            }
        }
        out << "],\n"
                  << "\"children\": [\n";
        if (has_leaves()) {
            leaf_type* const* children =
                reinterpret_cast<leaf_type* const*>(children_);
            for (uint16_t i = 0; i < child_count_; i++) {
                children[i]->print(internal_only);
                if (i != child_count_ - 1) {
                    out << ",";
                }
                out << "\n";
            }
        } else {
            node* const* children = reinterpret_cast<node* const*>(children_);
            for (uint16_t i = 0; i < child_count_; i++) {
                children[i]->print(internal_only);
                if (i != child_count_ - 1) {
                    out << ",";
                }
                out << "\n";
            }
        }
        out << "]}";
        return out;
    }

    std::pair<uint64_t, uint64_t> leaf_usage() const {
        std::pair<uint64_t, uint64_t> p(0, 0);
        if (has_leaves()) {
            leaf_type* const* children =
                reinterpret_cast<leaf_type* const*>(children_);
            for (uint16_t i = 0; i < child_count_; i++) {
                auto op = children[i]->leaf_usage();
                p.first += op.first;
                p.second += op.second;
            }
        } else {
            node* const* children = reinterpret_cast<node* const*>(children_);
            for (uint16_t i = 0; i < child_count_; i++) {
                auto op = children[i]->leaf_usage();
                p.first += op.first;
                p.second += op.second;
            }
        }
        return p;
    }

   private:
    /**
     * @brief Splits a leaf with `n > leaf_size` elements into 2 leaves with
     * part of the encoded content each.
     *
     * Intended for use with compressed leaves where a leaf can contain a bigger
     * slice of the universe encoded into less than `leaf_size` bits.
     *
     * @param index Index of leaf to split.
     * @param leaf  Pointer to leaf to split.
     * @param alloc Pointer to allocator to use.
     *
     * @tparam allocator type of alloc.
     */
    template <class allocator>
    void split_leaf(uint16_t index, leaf_type* leaf, allocator* alloc) {
        dtype cap = leaf->capacity();
        //leaf->print(false);
        //std::cout << std::endl;
        leaf_type* sibling = alloc->template allocate_leaf<leaf_type>(cap);
        sibling->transfer_capacity(leaf);
        if constexpr (aggressive_realloc) {
            cap = leaf->capacity();
            dtype n_cap = leaf->desired_capacity();
            if (n_cap < cap) {
                leaf = alloc->reallocate_leaf(leaf, cap, n_cap);
            }
            cap = sibling->capacity();
            n_cap = sibling->desired_capacity();
            if (n_cap < cap) {
                sibling = alloc->reallocate_leaf(sibling, cap, n_cap);
            }
        }
        //sibling->print(false);
        //std::cout << std::endl;
        //leaf->print(false);
        //std::cout << std::endl;
        for (dtype i = child_count_; i > index + 1u; i--) {
            children_[i] = children_[i - 1];
        }
        children_[index] = sibling;
        children_[index + 1] = leaf;
        child_sizes_.insert(index + 1, child_count_, leaf->size());
        child_sums_.insert(index + 1, child_count_, leaf->p_sum());
        child_count_++;
    }

    /**
     * @brief Ensure that there is space for insertion in the child leaves.
     *
     * If there is sufficient space in either sibling, elements will be
     * transferred to the sibling with more room. If there is insufficient space
     * in the siblings, a new leaf is allocated and elements are transferred
     * from the target leaf to one of the siblings.
     *
     * Generally delays allocation of a new leaf as long as possible, and when
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
    void rebalance_leaf(uint16_t index, leaf_type* leaf, allocator* alloc) {
        //if (!compressed && do_debug) {
        //        std::cout << "Rebalancing leaf " << int(index) << ":" << std::endl;
        //}
        if constexpr (compressed) {
            // Leaf contains a sufficiently large partition of the universe that
            // it can just be split into 2.
            if (leaf->size() > leaf_size) {
                //if (compressed && do_debug) {
                //    std::cout << " split" << std::endl;
                //}
                split_leaf(index, leaf, alloc);
                return;
            }
        }
        // Number of elements that can fit in the "left" sibling (with potential
        // reallocation).
        uint32_t l_cap = 0;
        if (index > 0) {
            l_cap = child_sizes_.get(index - 1);
            l_cap -= index > 1 ? child_sizes_.get(index - 2) : 0;
            if constexpr (compressed) {
                l_cap = l_cap > leaf_size ? 0 : leaf_size - l_cap;
            } else {
                l_cap = leaf_size - l_cap;
            }
            [[likely]] (void(0));
        }
        // Number of leaves that can fit in the "right" sibling (with potential
        // reallocation).
        uint32_t r_cap = 0;
        if (index < child_count_ - 1) {
            r_cap = child_sizes_.get(index + 1);
            r_cap -= child_sizes_.get(index);
            if constexpr (compressed) {
                r_cap = r_cap > leaf_size ? 0 : leaf_size - r_cap;
            } else {
                r_cap = leaf_size - r_cap;
            }
            [[likely]] (void(0));
        }
        if (l_cap < 2 * leaf_size / 9 && r_cap < 2 * leaf_size / 9) {
            // Rebalancing without creating a new leaf is impossible
            // (impractical).
            leaf_type* a_child;
            leaf_type* b_child;
            leaf_type* new_child;
            dtype n_cap;
            if (index == 0) {
                // If the full leaf is the first child, a new leaf is created
                // between indexes 0 and 1.
                a_child = reinterpret_cast<leaf_type*>(children_[0]);
                b_child = reinterpret_cast<leaf_type*>(children_[1]);
                dtype n_elem = (a_child->size() + (leaf_size - r_cap)) / 3;
                n_cap = 2 + (2 * leaf_size) / (3 * WORD_BITS);
                n_cap += n_cap % 2;
                new_child = alloc->template allocate_leaf<leaf_type>(n_cap);
                new_child->transfer_prepend(a_child, a_child->size() - n_elem);
                if constexpr (compressed) {
                    n_elem = b_child->size() - n_elem;
                    n_elem = n_elem + new_child->size() > (2 * leaf_size) / 3 ? leaf_size / 3 : n_elem;
                    new_child->transfer_append(b_child, n_elem);
                } else {
                    new_child->transfer_append(b_child, b_child->size() - n_elem);
                }
                [[unlikely]] index++;
            } else {
                // If the full leaf is not the first child, a new leaf is
                // created to the left of the full leaf.
                //if (compressed && do_debug) {
                //    std::cout << " split 2->3" << std::endl;
                //}
                a_child = reinterpret_cast<leaf_type*>(children_[index - 1]);
                b_child = reinterpret_cast<leaf_type*>(children_[index]);
                //if (compressed && do_debug) {
                //    a_child->print(false);
                //    std::cout << std::endl;
                //    b_child->print(false);
                //    std::cout << std::endl;
                //}
                dtype n_elem = ((leaf_size - l_cap) + b_child->size()) / 3;
                n_cap = 2 + (2 * leaf_size) / (3 * WORD_BITS);
                n_cap += n_cap % 2;
                new_child = alloc->template allocate_leaf<leaf_type>(n_cap);
                //if (compressed && do_debug) {
                //    std::cout << "Yoinking " << (b_child->size() - n_elem) << " elems from the right" << std::endl;
                //}
                new_child->transfer_append(b_child, b_child->size() - n_elem);
                if constexpr (compressed) {
                    n_elem = a_child->size() - n_elem;
                    n_elem = n_elem + new_child->size() > (2 * leaf_size) / 3 ? leaf_size / 3 : n_elem;
                    //if (do_debug) {
                    //    std::cout << "Yoinking " << n_elem << " elems from the left" << std::endl;
                    //}
                    new_child->transfer_prepend(a_child, n_elem);
                } else {
                    //if (do_debug) {
                    //    std::cout << "Yoinking " << n_elem << " elems from the left" << std::endl;
                    //}
                    new_child->transfer_prepend(a_child, a_child->size() - n_elem);
                }
                //if (compressed && do_debug) {
                //    a_child->print(false);
                //    std::cout << std::endl;
                //    new_child->print(false);
                //    std::cout << std::endl;
                //    b_child->print(false);
                //    std::cout << std::endl;
                //}
                a_child->validate();
                new_child->validate();
                b_child->validate();
            }
            if constexpr (aggressive_realloc) {
                uint64_t cap = a_child->capacity();
                n_cap = a_child->desired_capacity();
                if (cap > n_cap) {
                    a_child = alloc->reallocate_leaf(a_child, cap, n_cap);
                    [[unlikely]] children_[index - 1] = a_child;
                }
                cap = b_child->capacity();
                n_cap = b_child->desired_capacity();
                if (cap > n_cap) {
                    b_child = alloc->reallocate_leaf(b_child, cap, n_cap);
                    [[unlikely]] children_[index] = b_child;
                }
            }
            // Update cumulative sizes and sums.
            for (uint16_t i = child_count_; i > index; i--) {
                children_[i] = children_[i - 1];
            }
            children_[index] = new_child;
            child_sizes_.insert(index, child_count_, a_child->size(),
                                new_child->size());
            child_sums_.insert(index, child_count_, a_child->p_sum(),
                               new_child->p_sum());
            [[unlikely]] child_count_++;
        } else if (r_cap > l_cap) {
            // Right sibling has more space than the left sibling. Move elements
            // to right sibling
            
            leaf_type* sibling =
                reinterpret_cast<leaf_type*>(children_[index + 1]);
            //if (!compressed && do_debug) {
            //    std::cout << "Scooting right" << std::endl;
            //    //leaf->print(false);
            //    //std::cout << std::endl;
            //    //sibling->print(false);
            //    //std::cout << std::endl;
            //}
            uint32_t n_size = sibling->size() + r_cap / 2;
            if (sibling->capacity() * WORD_BITS <= n_size) {
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
            uint32_t cap = leaf->capacity();
            uint32_t n_cap = leaf->desired_capacity();
            if (cap > n_cap) {
                leaf = alloc->reallocate_leaf(leaf, cap, n_cap);
                [[unlikely]] children_[index] = leaf;
            }
            cap = sibling->capacity();
            n_cap = sibling->desired_capacity();
            if (cap > n_cap) {
                sibling = alloc->reallocate_leaf(sibling, cap, n_cap);
                [[unlikely]] children_[index + 1] = sibling;
            }
            child_sizes_.set(
                index, index != 0 ? child_sizes_.get(index - 1) + leaf->size()
                                  : leaf->size());
            child_sums_.set(
                index, index != 0 ? child_sums_.get(index - 1) + leaf->p_sum()
                                  : leaf->p_sum());
            //if (!compressed && do_debug) {
            //    std::cout << "scooted right" << std::endl;
            //    //leaf->print(false);
            //    //std::cout << std::endl;
            //    //sibling->print(false);
            //    //std::cout << std::endl;
            //}
        } else {
            // Left sibling has more space than the right sibling. Move elements
            // to the left sibling.
            leaf_type* sibling =
                reinterpret_cast<leaf_type*>(children_[index - 1]);
            //if (!compressed && do_debug) {
            //    std::cout << "Scooting left" << std::endl;
            //    //sibling->print(false);
            //    //std::cout << std::endl;
            //    //leaf->print(false);
            //    //std::cout << std::endl;
            //}
            uint32_t n_size = sibling->size() + l_cap / 2;
            if (sibling->capacity() * WORD_BITS <= n_size) {
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
            uint32_t cap = leaf->capacity();
            uint32_t n_cap = leaf->desired_capacity();
            if (cap > n_cap) {
                leaf = alloc->reallocate_leaf(leaf, cap, n_cap);
                [[unlikely]] children_[index] = leaf;
            }
            cap = sibling->capacity();
            n_cap = sibling->desired_capacity();
            if (cap > n_cap) {
                sibling = alloc->reallocate_leaf(sibling, cap, n_cap);
                [[unlikely]] children_[index - 1] = sibling;
            }
            child_sizes_.set(index - 1,
                             index > 1
                                 ? child_sizes_.get(index - 2) + sibling->size()
                                 : sibling->size());
            child_sums_.set(index - 1, index > 1 ? child_sums_.get(index - 2) +
                                                       sibling->p_sum()
                                                 : sibling->p_sum());
            //if (!compressed && do_debug) {
            //    std::cout << "Scooted left" << std::endl;
            //    //sibling->print(false);
            //    //std::cout << std::endl;
            //    //leaf->print(false);
            //    //std::cout << std::endl;
            //}
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
        uint16_t child_index = child_sizes_.find(index);
        leaf_type* child = reinterpret_cast<leaf_type*>(children_[child_index]);
        if (child->need_realloc()) {
            dtype cap = child->capacity();
            dtype n_cap = child->desired_capacity();
            if constexpr (compressed) {
                if (child->is_compressed()) {
                    if ((child->size() >= (~uint32_t(0) >> 1)) ||
                        (n_cap * WORD_BITS > leaf_size)) {
                        rebalance_leaf(child_index, child, alloc);
                    } else {
                        children_[child_index] =
                            alloc->reallocate_leaf(child, cap, n_cap);
                        [[likely]] (void(0));
                    }
                } else {
                    if (n_cap * WORD_BITS > leaf_size) {
                        rebalance_leaf(child_index, child, alloc);
                    } else {
                        children_[child_index] =
                            alloc->reallocate_leaf(child, cap, n_cap);
                        [[likely]] (void(0));
                    }
                }
            } else {
                if (n_cap * WORD_BITS > leaf_size) {
                    rebalance_leaf(child_index, child, alloc);
                } else {
                    children_[child_index] =
                        alloc->reallocate_leaf(child, cap, n_cap);
                    [[likely]] (void(0));
                }
            }
            child_index = child_sizes_.find(index);
            child = reinterpret_cast<leaf_type*>(children_[child_index]);
            [[unlikely]] (void(0));
        }
        if (child_index != 0) {
            [[likely]] index -= child_sizes_.get(child_index - 1);
        }
        child_sizes_.increment(child_index, child_count_, 1u);
        child_sums_.increment(child_index, child_count_, value);
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
    void rebalance_node(uint16_t index, allocator* alloc) {
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
                children_[i] = children_[i - 1];
            }
            child_sizes_.insert(index, child_count_, a_node->size(),
                                new_child->size());
            child_sums_.insert(index, child_count_, a_node->p_sum(),
                               new_child->p_sum());
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
            child_sizes_.set(0, a_node->size());
            [[unlikely]] child_sums_.set(0, a_node->p_sum());
        } else {
            child_sizes_.set(index,
                             child_sizes_.get(index - 1) + a_node->size());
            child_sums_.set(index,
                            child_sums_.get(index - 1) + a_node->p_sum());
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
        uint16_t child_index = child_sizes_.find(index);
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
            child_index = child_sizes_.find(index);
            [[unlikely]] child =
                reinterpret_cast<node*>(children_[child_index]);
        }
        if (child_index != 0) {
            [[likely]] index -= child_sizes_.get(child_index - 1);
        }
        child_sizes_.increment(child_index, child_count_, 1u);
        child_sums_.increment(child_index, child_count_, value);
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
        if constexpr (compressed) {
            addition = addition > leaf_size / 3 ? leaf_size / 3 : addition;
        }
        if (a_cap * WORD_BITS < a->size() + addition) {
            dtype n_cap = 2 + (a->size() + addition) / WORD_BITS;
            n_cap += n_cap % 2;
            n_cap =
                n_cap * WORD_BITS <= leaf_size ? n_cap : leaf_size / WORD_BITS;
            a = alloc->reallocate_leaf(a, a_cap, n_cap);
            children_[0] = a;
        }
        a->transfer_append(b, addition);
        if constexpr (aggressive_realloc) {
            uint32_t cap = b->capacity();
            uint32_t d_cap = b->desired_capacity();
            if (cap > d_cap) {
                b = alloc->reallocate_leaf(b, cap, d_cap);
                [[unlikely]] children_[1] = b;
            }
        }
        child_sizes_.set(0, a->size());
        child_sums_.set(0, a->p_sum());
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
    void rebalance_leaves_left(leaf_type* a, leaf_type* b, uint16_t idx,
                               allocator* alloc) {
        dtype b_cap = b->capacity();
        dtype addition = (a->size() - leaf_size / 3) / 2;
        if constexpr (compressed) {
            addition = addition > leaf_size / 3 ? leaf_size / 3 : addition;
        }
        if (b_cap * WORD_BITS < b->size() + addition) {
            dtype n_cap = 2 + (b->size() + addition) / WORD_BITS;
            n_cap += n_cap % 2;
            n_cap =
                n_cap * WORD_BITS <= leaf_size ? n_cap : leaf_size / WORD_BITS;
            b = alloc->reallocate_leaf(b, b_cap, n_cap);
            children_[idx + 1] = b;
        }
        b->transfer_prepend(a, addition);
        if constexpr (aggressive_realloc) {
            uint32_t cap = a->capacity();
            uint32_t n_cap = a->desired_capacity();
            if (cap > n_cap) {
                a = alloc->reallocate_leaf(a, cap, n_cap);
                [[unlikely]] children_[idx] = a;
            }
        }
        if (idx == 0) {
            child_sizes_.set(0, a->size());
            [[unlikely]] child_sums_.set(0, a->p_sum());
        } else {
            child_sizes_.set(idx, child_sizes_.get(idx - 1) + a->size());
            child_sums_.set(idx, child_sums_.get(idx - 1) + a->p_sum());
        }
    }

    /**
     * @brief Merge the right leaf into the left leaf.
     *
     * Intended for when a removal would break structural invariant and there
     * are not enough elements in siblings to transfer elements while
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
    void merge_leaves(leaf_type* a, leaf_type* b, uint16_t idx,
                      allocator* alloc) {
        dtype a_cap = a->capacity();
        if (a_cap * WORD_BITS < a->size() + b->size()) {
            dtype n_cap = 2 + (a->size() + b->size()) / WORD_BITS;
            n_cap += n_cap % 2;
            n_cap =
                n_cap * WORD_BITS <= leaf_size ? n_cap : leaf_size / WORD_BITS;
            a = alloc->reallocate_leaf(a, a_cap, n_cap);
        }
        a->append_all(b);
        alloc->deallocate_leaf(b);
        for (uint16_t i = idx; i < child_count_ - 1; i++) {
            children_[i] = children_[i + 1];
        }
        children_[idx] = a;
        child_sizes_.remove(idx, child_count_);
        child_sums_.remove(idx, child_count_);
        child_count_--;
    }

    /**
     * @brief Removal used when the children are leaves.
     *
     * Will maintain structural invariants by reallocating and rebalancing as
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
        uint16_t child_index = child_sizes_.find(index + 1);
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
            child_index = child_sizes_.find(index);
            [[unlikely]] child =
                reinterpret_cast<leaf_type*>(children_[child_index]);
        }
        if (child_index != 0) {
            [[likely]] index -= child_sizes_.get(child_index - 1);
        }
        bool value = child->remove(index);
        if constexpr (aggressive_realloc) {
            uint64_t cap = child->capacity();
            if (cap * WORD_BITS > child->size() + 4 * WORD_BITS) {
                child = alloc->reallocate_leaf(child, cap, cap - 2);
                [[unlikely]] children_[child_index] = child;
            }
        }
        child_sizes_.increment(child_index, child_count_, -1);
        child_sums_.increment(child_index, child_count_, -int(value));
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
    void rebalance_nodes_right(node* a, node* b, uint16_t idx) {
        a->transfer_append(b, (b->child_count() - branches / 3) / 2);
        child_sizes_.set(idx, a->size());
        [[unlikely]] child_sums_.set(idx, a->p_sum());
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
    void rebalance_nodes_left(node* a, node* b, uint16_t idx) {
        b->transfer_prepend(a, (a->child_count() - branches / 3) / 2);
        if (idx == 0) {
            child_sizes_.set(0, a->size());
            [[unlikely]] child_sums_.set(0, a->p_sum());
        } else {
            child_sizes_.set(idx, child_sizes_.get(idx - 1) + a->size());
            child_sums_.set(idx, child_sums_.get(idx - 1) + a->p_sum());
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
    void merge_nodes(node* a, node* b, uint16_t idx, allocator* alloc) {
        a->append_all(b);
        alloc->deallocate_node(b);
        for (uint16_t i = idx; i < child_count_ - 1; i++) {
            children_[i] = children_[i + 1];
        }
        children_[idx] = a;
        child_sizes_.remove(idx, child_count_);
        child_sums_.remove(idx, child_count_);
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
        uint16_t child_index = child_sizes_.find(index + 1);
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
            child_index = child_sizes_.find(index + 1);
            [[unlikely]] child =
                reinterpret_cast<node*>(children_[child_index]);
        }
        if (child_index != 0) {
            [[likely]] index -= child_sizes_.get(child_index - 1);
        }
        bool value = child->remove(index, alloc);
        child_sizes_.increment(child_index, child_count_, -1);
        child_sums_.increment(child_index, child_count_, -int(value));
        return value;
    }
};
}  // namespace bv
#endif