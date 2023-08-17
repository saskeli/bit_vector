#ifndef BV_BIT_VECTOR_HPP
#define BV_BIT_VECTOR_HPP

#include <cassert>
#include <cstdint>
#include <utility>

#include "uncopyable.hpp"

namespace bv {

/**
 * @brief Container class for dynamic b-tree bit vector stuctures.
 *
 * This class manages a dynamic bit vector structure and provides
 * the public API for querying the structure.
 *
 * The structure supports efficient insertion and removal, along
 * with common bit vector operations (`set`, access (`at`), `rank`
 * and `select`). Additionally the convenience and debug functions
 * `sum`, `size`, `bit_size`, `validate`, `print` and `bit_size`
 * are provided at no extra cost.
 *
 * Practical performance depends on node and leaf implementations.
 *
 * @tparam leaf               Type for leaves. Some kind of bv::leaf.
 * @tparam node               Type for internal nodes. Some kind of bv::node.
 * @tparam allocator          Allocator type. For example bv::malloc_alloc.
 * @tparam leaf_size          Maximum number of elements in leaf.
 * @tparam branches           Maximum branching factor of internal node.
 * @tparam aggressive_realloc If true, leaves will only be allowed to have at
 * most 256 elements of unused capacity
 * @tparam compressed         If true, additional bookkeeping will be done to
 * ensure compressed leaves behave correctly.
 */
template <class leaf, class node, class allocator, uint32_t leaf_size,
          uint8_t branches, class dtype, bool aggressive_realloc = false,
          bool compressed = false>
class bit_vector : uncopyable {
   private:
    bool root_is_leaf_ = true;      ///< Value indicating whether the root is in
                                    ///< `n_root_` or `l_root_`.
    bool owned_allocator_ = false;  ///< True if deallocating the allocator
                                    ///< is the responsibilty of the data
                                    ///< structure.
    node* n_root_;  ///< Root of the b-tree if a multi-level structure is
                    ///< required.
    leaf* l_root_;  ///< Root if a single leaf is sufficient.
    allocator* allocator_;  ///< Pointer to allocator used for allocating
                            ///< internal nodes and leaves.

    /** @brief Number of bits in a computer word. */
    static const constexpr uint64_t WORD_BITS = 64;

    /**
     * @brief Increases the height of the tree by one level.
     *
     * A full root node will be split into two and nodes are set as children of
     * a new root node.
     *
     * The root node will be kept as the only node with less than `branches/2`
     * children. The child nodes will each have exactly `branches/2` children.
     */
    void split_root() {
        node* new_root = allocator_->template allocate_node<node>();
        node* sibling = allocator_->template allocate_node<node>();
        if (n_root_->has_leaves()) sibling->has_leaves(true);
        sibling->transfer_prepend(n_root_, branches / 2);
        new_root->append_child(n_root_);
        new_root->append_child(sibling);
        n_root_ = new_root;
    }

    /**
     * @brief Increases the height of the tree by one level.
     *
     * A full leaf node will be split into two and leaves are set as children of
     * a new root node.
     *
     * The root node will be kept as the only node with less than `branches/2`
     * children.
     */
    void split_leaf() {
        dtype cap = l_root_->capacity();
        leaf* sibling = allocator_->template allocate_leaf<leaf>(cap);
        if constexpr (compressed) {
            if (l_root_->size() > leaf_size) {
                sibling->transfer_capacity(l_root_);
            } else {
                sibling->transfer_append(l_root_, leaf_size / 2);
            }
        } else {
            sibling->transfer_append(l_root_, leaf_size / 2);
        }
        if constexpr (aggressive_realloc) {
            cap = l_root_->capacity();
            dtype n_cap = l_root_->desired_capacity();
            if (n_cap < cap) {
                l_root_ = allocator_->reallocate_leaf(l_root_, cap, n_cap);
                [[likely]] (void(0));
            }
            cap = sibling->capacity();
            n_cap = sibling->desired_capacity();
            if (n_cap < cap) {
                sibling = allocator_->reallocate_leaf(sibling, cap, n_cap);
                [[likely]] (void(0));
            }
        }
        n_root_ = allocator_->template allocate_node<node>();
        n_root_->has_leaves(true);
        n_root_->append_child(sibling);
        n_root_->append_child(l_root_);
        root_is_leaf_ = false;
    }

   public:
    /**
     * @brief Bit vector constructor with existing allocator
     *
     * One allocator can be used to provide services for multiple bit vector
     * instances. There is no practical benefit from sharing bv::malloc_alloc
     * allocators, but a performance benefit is expected by sharing more
     * advanced allocators between multiple bit vector instances.
     *
     * @param alloc The allocator instance.
     */
    bit_vector(allocator* alloc, dtype size = 0, bool value = false) {
        allocator_ = alloc;
        if constexpr (compressed) {
            l_root_->template allocate_leaf<leaf>(2, size, value);
            return;
        }
        if (size > 0 || value == true) {
            std::cerr << "Uncompressed data structure can't be constructed "
                         "with non-default size or value"
                      << std::endl;
            [[unlikely]] exit(1);
        }
        l_root_ = allocator_->template allocate_leaf<leaf>(2);
    }

    /**
     * @brief Default constructor that creates an owned allocator.
     *
     * The default constructor will create an owned allocator that gets
     * deallocated along with the rest of the data structure.
     */
    bit_vector(dtype size = 0, bool value = false) {
        allocator_ = new allocator();
        owned_allocator_ = true;
        if constexpr (compressed) {
            l_root_ = allocator_->template allocate_leaf<leaf>(2, size, value);
            return;
        }
        if (size > 0 || value == true) {
            std::cerr << "Uncompressed data structure can't be constructed "
                         "with non-default size or value"
                      << std::endl;
            [[unlikely]] exit(1);
        }
        l_root_ = allocator_->template allocate_leaf<leaf>(2);
    }

    /**
     * @brief Deconstructor that deallocates the entire data structure.
     *
     * All of the internal nodes and leaves allocated for this datastucture get
     * recursively deallocated before deallocation of the bit vector container.
     * If the allocator is owned by the bit vector container, that will be
     * dallocated as well.
     */
    ~bit_vector() {
        if (root_is_leaf_) {
            allocator_->deallocate_leaf(l_root_);
        } else {
            n_root_->deallocate(allocator_);
            allocator_->deallocate_node(n_root_);
        }
        if (owned_allocator_) {
            delete (allocator_);
        }
    }

    /**
     * @brief Insert "value" into position "index".
     *
     * The bit vector container ensures that there is sufficient space in the
     * b-tree, splitting nodes and increasing the tree height as necessary.
     *
     * Insert operations take \f$\mathcal{O}\left(b\log_b(n / l) + l\right)\f$
     * amortized time when using bv::node and bv::leaf elements for data
     * storage, where \f$b\f$ is the branching factor, \f$l\f$ is the leaf size
     * and \f$n\f$ is the data structure size. The branching overhead compared
     * to the access operation is due to updating cumulative sums and sizes in
     * the internal nodes.
     *
     * @param index Where should `value` be inserted.
     * @param value What should be inserted at `index`.
     */
    void insert(dtype index, bool value) {
#ifdef DEBUG
        if (index > size()) {
            std::cerr << "Invalid insertion to index " << index << " for "
                      << size() << " element bit vector." << std::endl;
            assert(index <= size());
        }
#endif
        if (root_is_leaf_) {
            if (l_root_->need_realloc()) {
                dtype cap = l_root_->capacity();
                if constexpr (compressed) {
                    if ((cap * WORD_BITS >= leaf_size) ||
                        (l_root_->size() >= ((~uint32_t(0)) >> 1))) {
                        split_leaf();
                        n_root_->insert(index, value, allocator_);
                    } else {
                        dtype n_cap = l_root_->desired_capacity();
                        if (n_cap * WORD_BITS > leaf_size) {
                            split_leaf();
                            n_root_->insert(index, value, allocator_);
                            [[unlikely]] return;
                        }
                        l_root_ = allocator_->reallocate_leaf(l_root_, cap, n_cap);
                        [[likely]] l_root_->insert(index, value);
                    }
                } else {
                    if (cap * WORD_BITS >= leaf_size) {
                        split_leaf();
                        n_root_->insert(index, value, allocator_);
                    } else {
                        dtype n_cap = l_root_->desired_capacity();
                        if (n_cap * WORD_BITS > leaf_size) {
                            split_leaf();
                            n_root_->insert(index, value, allocator_);
                        } else {
                            l_root_ = allocator_->reallocate_leaf(l_root_, cap, n_cap);
                            [[likely]] l_root_->insert(index, value);
                        }
                        [[likely]] (void(0));
                    }
                }
            } else {
                [[likely]] l_root_->insert(index, value);
            }
        } else {
            if (n_root_->child_count() == branches) {
                [[unlikely]] split_root();
            }
            [[likely]] n_root_->insert(index, value, allocator_);
        }
    }

    /**
     * @brief Remove element at "index".
     *
     * Removes a bit from the underlying data structure, decreasing the tree
     * height as appropriate. Tree height is decreased if `n_root_` is the
     * active root and has exactly one child.
     *
     * Remove operations take \f$\mathcal{O}\left(b\log_b(n / l) + l\right)\f$
     * amortized time when using bv::node and bv::leaf elements for data
     * storage, where \f$b\f$ is the branching factor, \f$l\f$ is the leaf size
     * and \f$n\f$ is the data structure size. The branching overhead compared
     * to the access operation is due to updating cumulative sums and sizes in
     * the internal nodes.
     *
     * The value of the removed bit needs to be bubbled up the data structure to
     * update cumulative sums. This makes returning the removed value here a
     * 0-cost "side effect".
     *
     * @param index Position of element to remove.
     *
     * @return Value of the removed bit.
     */
    bool remove(dtype index) {
        if (root_is_leaf_) {
            [[unlikely]] return l_root_->remove(index);
        } else {
            bool v = n_root_->remove(index, allocator_);
            if (n_root_->child_count() == 1) {
                if (n_root_->has_leaves()) {
                    l_root_ = reinterpret_cast<leaf*>(n_root_->child(0));
                    root_is_leaf_ = true;
                    allocator_->deallocate_node(n_root_);
                } else {
                    node* new_root = reinterpret_cast<node*>(n_root_->child(0));
                    allocator_->deallocate_node(n_root_);
                    n_root_ = new_root;
                }
                [[unlikely]] (void(0));
            }
            return v;
        }
    }

    /**
     * @brief Number of 1-bits in the data structure.
     *
     * Cumulative partial sums are maintained as the data structure changes. As
     * such this is a constant time lookup operation.
     *
     * @return \f$\sum_{i = 0}^{n - 1} \mathrm{bv}[i]\f$ for \f$n\f$
     * element bit vector.
     */
    dtype sum() const {
        return !root_is_leaf_ ? n_root_->p_sum() : l_root_->p_sum();
    }

    /**
     * @brief Number of elements stored in the data structure
     *
     * Cumulative sizes are maintained as the data structure changes. As such
     * this is a constant time lookup operation.
     *
     * @return \f$n\f$.
     */
    dtype size() const {
        return !root_is_leaf_ ? n_root_->size() : l_root_->size();
    }

    /**
     * @brief Retrieves the value of the index<sup>th</sup> element in the data
     * structure.
     *
     * Performs an at-query on either `n_root_` or `l_root_` based on tree
     * status.
     *
     * When using bv::node and bv::leaf element for the internal data structure,
     * access operations take
     * \f$\mathcal{O}\left(\log_2(b)\log_b(n / l)\right)\f$ time,
     * where \f$b\f$ is the branching factor, \f$l\f$ is the leaf size and
     * \f$n\f$ is the data structure size.
     *
     * @param index Index to access
     *
     * @return Boolean value indicating if the index<sup>th</sup> element is
     * set.
     */
    bool at(dtype index) const {
        return !root_is_leaf_ ? n_root_->at(index) : l_root_->at(index);
    }

    /**
     * @brief Number of 1-bits up to position index.
     *
     * Counts the number of bits set in the first index bits.
     *
     * When using bv::node and bv::leaf elements for the internal data
     * structure, rank operations take \f$\mathcal{O}\left(\log_2(b)\log_b(n /
     * l) + l\right)\f$ time, where \f$b\f$ is the branching factor, \f$l\f$ is
     * the leaf size and \f$n\f$ is the data structure size.
     *
     * @param index Number of elements to include in the "summation".
     *
     * @return \f$\sum_{i = 0}^{\mathrm{index - 1}} \mathrm{bv}[i]\f$.
     */
    dtype rank(dtype index) const {
        return !root_is_leaf_ ? n_root_->rank(index) : l_root_->rank(index);
    }
    dtype rank0(dtype index) const {
        if (index == 0) {
            [[unlikely]] return 0;
        }
        dtype ret = index - rank(index);
        return ret;
    }
    dtype rank(bool v, dtype index) const {
        return v ? rank(index) : rank0(index);
    }

    /**
     * @brief Index of the count<sup>th</sup> 1-bit in the data structure
     *
     * When using bv::node and bv::leaf for internal strucutres, finds the
     * position of the count<sup>th</sup> 1-bit is found in
     * \f$\mathcal{O}\left(\log_2(b)\log_b(n / l) + l\right) \f$ time, where
     * \f$b\f$ is the branching factor, \f$l\f$ is the leaf size and \f$n\f$ is
     * the data structure size.
     *
     * @param count Selection target.
     *
     * @return \f$\underset{i \in [0..n)}{\mathrm{arg min}}\left(\sum_{j = 0}^i
     * \mathrm{bv}[j]\right) =  \f$ count.
     */
    dtype select(dtype count) const {
        return !root_is_leaf_ ? n_root_->select(count) : l_root_->select(count);
    }

    dtype select0(dtype count) const {
        dtype a = 0;
        dtype b = size();
        while (a < b) {
            dtype m = (a + b) / 2;
            dtype r_m = rank0(m);
            if (r_m < count) {
                a = m + 1;
            } else if (r_m > count) {
                b = m - 1;
            } else {
                b = m;
            }
        }
        return a - 1;
    }
    dtype select(bool v, dtype count) const {
        return v ? select(count) : select0(count);
    }


    /**
     * @brief Sets the bit at "index" to "value".
     *
     * When using bv::node and bv::leaf for internal strucutres, sets the value
     * of the index<sup>th</sup> bit in \f$\mathcal{O}\left(b\log_b(n /
     * l)\right) \f$ time, where \f$b\f$ is the branching factor, \f$l\f$ is the
     * leaf size and \f$n\f$ is the data structure size. As with insertion and
     * removal there is overhead in updating internal nodes.
     *
     * @param index Index to set.
     * @param value value to set the index<sup>th</sup> bit to.
     */
    void set(dtype index, bool value) {
        if (root_is_leaf_) {
            if constexpr (compressed) {
                if (l_root_->is_compressed() && l_root_->need_realloc()) {
                    dtype cap = l_root_->capacity();
                    if (cap * WORD_BITS >= leaf_size ||
                        l_root_->size() >= ((~uint32_t(0)) >> 1)) {
                        split_leaf();
                        n_root_->set(index, value, allocator_);
                        [[unlikely]] return;
                    }
                    dtype n_cap = l_root_->desired_capacity();
                    if (n_cap > leaf_size / WORD_BITS) {
                        split_leaf();
                        n_root_->set(index, value, allocator_);
                        [[unlikely]] return;
                    }
                    l_root_ = allocator_->reallocate_leaf(l_root_, cap, n_cap);
                    [[unlikely]] (void(0));
                }
            }
            [[unlikely]] l_root_->set(index, value);
        } else {
            if constexpr (compressed) {
                if (n_root_->child_count() == branches) {
                    [[unlikely]] split_root();
                }
            }
            n_root_->set(index, value, allocator_);
        }
    }

    /**
     * @brief Recursively flushes all buffers in the data structure.
     *
     * If execution transitions to a phase where no insertions or removals are
     * expected, flushing all buffers should improve query time at the cost of a
     * fairly expensive flushing operations.
     */
    void flush() { root_is_leaf_ ? l_root_->flush() : n_root_->flush(); }

    /**
     * @brief Write raw bit data to the area provided.
     *
     * The are provided should have at least bv.size() allocated and zeroed
     * out bits.
     *
     * @param data Pointer to where raw data should be dumped.
     */
    void dump(uint64_t* data) {
        if (root_is_leaf_) {
            l_root_->dump(data, 0);
        } else {
            n_root_->dump(data, 0);
        }
    }

    /**
     * @brief Total size of data structure allocations in bits.
     *
     * Calculates the total size of "this", allocated nodes and allocated leaves
     * in bits.
     *
     * Size of the allocator is not included, and no consideration is made on
     * the effects of memory fragmentation.
     *
     * @return `8 * sizeof(bit_vector) + tree_size`
     */
    uint64_t bit_size() const {
        uint64_t tree_size =
            root_is_leaf_ ? l_root_->bits_size() : n_root_->bits_size();
        return 8 * sizeof(bit_vector) + tree_size;
    }

    /**
     * @brief Asserts that the data structure is internally consistent.
     *
     * Walks through the entire data structure and ensures that invariants and
     * cumulative sums and sizes are valid for a data structure of the specified
     * type.
     *
     * This does not guarantee that the data structure is correct given the
     * preceding query sequence, simply checks that a valid data structure seems
     * to be correctly defined.
     *
     * If the `-DNDEBUG` compiler flag is given, this function will do nothing
     * since assertions will be `(void(0))`ed out.
     */
    void validate() const {
#ifndef NDEBUG
        if (owned_allocator_) {
            uint64_t allocs = allocator_->live_allocations();
            if (root_is_leaf_) {
                assert(allocs == l_root_->validate());
            } else {
                assert(allocs == n_root_->validate());
            }
        } else {
            if (root_is_leaf_)
                l_root_->validate();
            else
                n_root_->validate();
        }
#endif
    }

    /**
     * @brief Output data structure to standard out as json.
     *
     * @param internal_only If true, actual bit vector data will not be output
     * to save space.
     */
    void print(bool internal_only = true) const {
        root_is_leaf_ ? l_root_->print(internal_only)
                      : n_root_->print(internal_only);
        std::cout << std::endl;
    }

    double leaf_usage() const {
        std::pair<uint64_t, uint64_t> p;
        if (root_is_leaf_) {
            p = l_root_->leaf_usage();
        } else {
            p = n_root_->leaf_usage();
        }
        return double(p.second) / p.first;
    }
};
}  // namespace bv
#endif