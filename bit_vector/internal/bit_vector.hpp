
#ifndef BV_BIT_VECTOR_HPP
#define BV_BIT_VECTOR_HPP

#include <cstdint>

namespace bv {

template <class leaf, class node, class allocator, uint64_t leaf_size,
          uint8_t branches>
class bit_vector {
   protected:
    bool root_is_leaf_ = true;
    bool owned_allocator_ = false;
    node* n_root_;
    leaf* l_root_;
    allocator* allocator_;

    void split_root() {
        node* new_root = allocator_->template allocate_node<node>();
        node* sibling = n_root_->template split<allocator>();
        new_root->append_child(n_root_);
        new_root->append_child(sibling);
        n_root_ = new_root;
    }

   public:
    bit_vector(allocator* alloc) {
        allocator_ = alloc;
        l_root_ =
            allocator_->template allocate_leaf<leaf>(leaf_size / (2 * 64));
    }

    bit_vector() {
        allocator_ = new allocator();
        owned_allocator_ = true;
        l_root_ =
            allocator_->template allocate_leaf<leaf>(leaf_size / (2 * 64));
    }

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

    void insert(uint64_t index, bool value) {
        if (root_is_leaf_) {
            if (l_root_->need_realloc()) {
                if (l_root_->size() >= leaf_size) {
                    n_root_ = allocator_->template allocate_node<node>();
                    n_root_->has_leaves(true);
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
            if (n_root_->child_count() == 1) {
                if (n_root_->has_leaves()) {
                    l_root_ = reinterpret_cast<leaf*>(n_root_->child(0));
                    root_is_leaf_ = true;
                    allocator_->deallocate_node(n_root_);
                    l_root_->remove(index);
                } else {
                    node* new_root = reinterpret_cast<node*>(n_root_->child(0));
                    allocator_->deallocate_node(n_root_);
                    n_root_ = new_root;
                    n_root_->template remove<allocator>(index);
                }
            } else {
                [[likely]] n_root_->template remove<allocator>(index);
            }
        }
    }

    uint64_t sum() {
        return !root_is_leaf_ ? n_root_->p_sum() : l_root_->p_sum();
    }

    uint64_t size() const {
        return !root_is_leaf_ ? n_root_->size(): l_root_->size();
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
        !root_is_leaf_ ? n_root_->set(index, value) : l_root_->set(index, value);
    }

    uint64_t bit_size() const {
        uint64_t tree_size =
            root_is_leaf_ ? l_root_->bits_size() : n_root_->bits_size();
        return 8 * sizeof(bit_vector) + tree_size;
    }

    void print(bool internal_only) const {
        root_is_leaf_ ? l_root_->print(internal_only)
                      : n_root_->print(internal_only);
        std::cout << std::endl;
    }
};
}  // namespace bv
#endif