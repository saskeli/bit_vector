#ifndef BV_BIT_VECTOR_HPP
#define BV_BIT_VECTOR_HPP

#ifndef WORD_BITS
#define WORD_BITS 64
#endif

#include <cassert>
#include <cstdint>
#include <utility>

#include "query_support.hpp"

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
 * @tparam leaf      Type for leaves. Some kind of bv::leaf.
 * @tparam node      Type for internal nodes. Some kind of bv::node.
 * @tparam allocator Allocator type. For example bv::malloc_alloc.
 * @tparam leaf_size Maximum number of elements in leaf.
 * @tparam branches  Maximum branching factor of internal node.
 * @tparam aggressive_realloc If true, leaves will only be allowed to have at most 256 elements of unused capacity
 */
template <class leaf, class node, class allocator, uint64_t leaf_size,
          uint8_t branches, class dtype, bool aggressive_realloc = false>
class bit_vector {
   protected:
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
    bit_vector(allocator* alloc) {
        allocator_ = alloc;
        l_root_ = allocator_->template allocate_leaf<leaf>(leaf_size /
                                                           (2 * WORD_BITS));
    }

    /**
     * @brief Default constructor that creates an owned allocator.
     *
     * The default constructor will create an owned allocator that gets
     * deallocated along with the rest of the data structure.
     */
    bit_vector() {
        allocator_ = new allocator();
        owned_allocator_ = true;
        l_root_ = allocator_->template allocate_leaf<leaf>(leaf_size /
                                                           (2 * WORD_BITS));
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
     * @brief Populate a given query support stucture using `this`
     *
     * Traverses the tree and ads encountered leaves to the query support
     * strucutre.
     *
     * @tparam block_size Size of blocks used in the query support structure.
     * @param qs Query support structure.
     */
    template <dtype block_size = 2048>
    void generate_query_structure(
        query_support<dtype, leaf, block_size>* qs) const {
        static_assert(block_size * 3 <= leaf_size);
        static_assert(block_size >= 2 * 64);
        if (root_is_leaf_) {
            [[unlikely]] qs->append(l_root_);
        } else {
            n_root_->generate_query_structure(qs);
        }
        qs->finalize();
    }

    /**
     * @brief Create and Populate a query support structure using `this`
     * 
     * Creates a new support structure and adds leaves in order.
     * 
     * @tparam block_size Size of blocks used in the query support structure.
     * @return A new query support strucutre.
     */
    template <dtype block_size = 2048>
    query_support<dtype, leaf, block_size>* generate_query_structure() const {
        static_assert(block_size * 3 <= leaf_size);
        query_support<dtype, leaf, block_size>* qs =
            new query_support<dtype, leaf, block_size>(size());
        generate_query_structure(qs);
        return qs;
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
    void insert(uint64_t index, bool value) {
#ifdef DEBUG
        if (index > size()) {
            std::cerr << "Invalid insertion to index " << index << " for "
                      << size() << " element bit vector." << std::endl;
            assert(index <= size());
        }
#endif
        if (root_is_leaf_) {
            if (l_root_->need_realloc()) {
                if (l_root_->size() >= leaf_size) {
                    leaf* sibling = allocator_->template allocate_leaf<leaf>(
                        2 + leaf_size / (2 * WORD_BITS));
                    sibling->transfer_append(l_root_, leaf_size / 2);
                    if constexpr (aggressive_realloc) {
                        l_root_ = allocator_->reallocate_leaf(l_root_, l_root_->capacity(), 2 + leaf_size / (2 * WORD_BITS));
                    }
                    n_root_ = allocator_->template allocate_node<node>();
                    n_root_->has_leaves(true);
                    n_root_->append_child(sibling);
                    n_root_->append_child(l_root_);
                    root_is_leaf_ = false;
                    n_root_->insert(index, value, allocator_);
                } else {
                    uint64_t cap = l_root_->capacity();
                    l_root_ =
                        allocator_->reallocate_leaf(l_root_, cap, cap + 2);
                    [[likely]] l_root_->insert(index, value);
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
    bool remove(uint64_t index) {
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
    uint64_t sum() const {
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
    uint64_t size() const {
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
    bool at(uint64_t index) const {
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
    uint64_t rank(uint64_t index) const {
        return !root_is_leaf_ ? n_root_->rank(index) : l_root_->rank(index);
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
    uint64_t select(uint64_t count) const {
        return !root_is_leaf_ ? n_root_->select(count) : l_root_->select(count);
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
    void set(uint64_t index, bool value) {
        !root_is_leaf_ ? n_root_->set(index, value)
                       : l_root_->set(index, value);
    }

    /**
     * @brief Recursively flushes all buffers in the data structure.
     *
     * If execution transitions to a phase where no insertions or removals are
     * expected, flushing all buffers should improve query time at the cost of a
     * fairly expensive flushing operations.
     */
    void flush() { root_is_leaf_ ? l_root_->commit() : n_root_->flush(); }

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
    }

    /**
     * @brief Output data structure to standard out as json.
     *
     * @param internal_only If true, actual bit vector data will not be output
     * to save space.
     */
    void print(bool internal_only) const {
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