#ifndef BV_BIT_VECTOR_HPP
#define BV_BIT_VECTOR_HPP

#include <cassert>
#include <cstdint>

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
 * @tparam leaf      type for leaves. Some kind of bv::leaf.
 * @tparam node      Type for internal nodes. Some kind of bv::node.
 * @tparam allocator Allocator type. For example bv::malloc_alloc.
 * @tparam leaf_size Maximum number of elements in leaf.
 * @tparam branches  Maximum branching factor of internal node.
 */
template <class leaf, class node, class allocator, uint64_t leaf_size,
          uint8_t branches>
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
        l_root_ =
            allocator_->template allocate_leaf<leaf>(leaf_size / (2 * 64));
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
        l_root_ =
            allocator_->template allocate_leaf<leaf>(leaf_size / (2 * 64));
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
            n_root_->template deallocate<allocator>();
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
     * b-tree increasing the tree height as necessary.
     *
     * Insert operations take \f$\approx \log_2(b)\log_b(n / l) + l\f$ amortized
     * time, where \f$b\f$ is the branching factor, \f$l\f$ is the leaf size and
     * \f$n\f$ is the data structure size.
     * 
     * @param index Where should `value` be inserted.
     * @param value What should be inserted at `index`.
     */
    void insert(uint64_t index, bool value) {
        if (root_is_leaf_) {
            if (l_root_->need_realloc()) {
                if (l_root_->size() >= leaf_size) {
                    leaf* sibling = allocator_->template allocate_leaf<leaf>(
                        leaf_size / (2 * 64));
                    sibling->transfer_append(l_root_, leaf_size / 2);
                    n_root_ = allocator_->template allocate_node<node>();
                    n_root_->has_leaves(true);
                    n_root_->append_child(sibling);
                    n_root_->append_child(l_root_);
                    root_is_leaf_ = false;
                    n_root_->template insert<allocator>(index, value);
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
            [[likely]] n_root_->template insert<allocator>(index, value);
        }
    }

    void remove(uint64_t index) {
        if (root_is_leaf_) {
            [[unlikely]] l_root_->remove(index);
        } else {
            n_root_->template remove<allocator>(index);
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
        }
    }

    uint64_t sum() {
        return !root_is_leaf_ ? n_root_->p_sum() : l_root_->p_sum();
    }

    uint64_t size() const {
        return !root_is_leaf_ ? n_root_->size() : l_root_->size();
    }

    bool at(uint64_t index) const {
        return !root_is_leaf_ ? n_root_->at(index) : l_root_->at(index);
    }

    uint64_t rank(uint64_t index) const {
        return !root_is_leaf_ ? n_root_->rank(index) : l_root_->rank(index);
    }

    uint64_t select(uint64_t count) const {
        return !root_is_leaf_ ? n_root_->select(count) : l_root_->select(count);
    }

    void set(uint64_t index, bool value) {
        !root_is_leaf_ ? n_root_->set(index, value)
                       : l_root_->set(index, value);
    }

    uint64_t bit_size() const {
        uint64_t tree_size =
            root_is_leaf_ ? l_root_->bits_size() : n_root_->bits_size();
        return 8 * sizeof(bit_vector) + tree_size;
    }

    void validate() const {
        uint64_t allocs = allocator_->live_allocations();
        if (root_is_leaf_) {
            assert(allocs == l_root_->validate());
        } else {
            assert(allocs == n_root_->validate());
        }
    }

    void print(bool internal_only) const {
        root_is_leaf_ ? l_root_->print(internal_only)
                      : n_root_->print(internal_only);
        std::cout << std::endl;
    }
};
}  // namespace bv
#endif