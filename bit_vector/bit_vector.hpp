
#ifndef BV_BIT_VECTOR_HPP
#define BV_BIT_VECTOR_HPP

#include <cstdint>

template <class leaf, class node, class allocator, uint64_t leaf_size>
class bit_vector {
  private:
    bool root_is_leaf_ = true;
    node* n_root_;
    leaf* l_root_;
    allocator* allocator_;

    void split_root() {
        node* new_root = allocator_->allocate_node();
        node* sibling = n_root_->spit();
        new_root->append_child(n_root_);
        new_root->append_child(sibling);
        n_root_ = new_root;
    }

  public:
    bit_vector(allocator* alloc) {
        allocator_ = alloc;
        l_root_ = allocator_->template allocate_leaf<leaf>(leaf_size / (2 * 64));
    }

    ~bit_vector() {
        if (root_is_leaf_) {
            allocator_->deallocate_leaf(l_root_);
        } else {
            n_root_->template deallocate<allocator, node>();
            allocator_->deallocate_node(n_root_);
        }
    }

    void insert(uint64_t index, bool value) {
        if (root_is_leaf_) {
            if (l_root_->need_realloc()) {
                if (l_root_->size() >= leaf_size) {
                    n_root_ = allocator_->allocate_node();
                    n_root_->has_leaves(true);
                    n_root_->append_child(l_root_);
                    root_is_leaf_ = false;
                    n_root_->insert(index, value);
                } else {
                    uint64_t cap = l_root_->capacity();
                    l_root_ = allocator_->reallocate_leaf(l_root_, cap, cap * 2);
                    l_root_->insert(index, value);
                }
            } else {
                [[likely]] l_root_->insert(index, value);
            }
        } else {
            if (n_root_->child_count() == 64) {
                [[unlikely]] split_root();
            }
            [[likely]] n_root_->insert(index, value);
        }
    }

    void remove(uint64_t index) {
        if (root_is_leaf_) {
            [[unlikely]] l_root_->remove(index);
        } else {
            if (n_root_->child_count() == 1) {
                if (n_root_->has_leaves()) {
                    l_root_ = n_root_->child(0);
                    root_is_leaf_ = true;
                    allocator_->deallocate_node(n_root_);
                    l_root_->reamove(index);
                } else {
                    node* new_root = n_root_->child(0);
                    allocator_->deallocate_node(n_root_);
                    n_root_ = new_root;
                    n_root_->remove(index);
                }
            } else {
                [[likely]] n_root_->remove(index);
            }
        }
    }

    uint64_t size() {
        return root_is_leaf_ ? l_root_->size() : n_root_->size();
    }

    bool at(uint64_t index) {
        return root_is_leaf_ ? l_root_->at(index) : n_root_->at(index);
    }

    uint64_t rank(uint64_t index) {
        return root_is_leaf_ ? l_root_->rank(index) : n_root_->rank(index);
    }

    uint64_t select(uint64_t count) {
        return root_is_leaf_ ? l_root_->selec(count) : n_root_->select(count);
    }

    void set(uint64_t index, bool value) {
        root_is_leaf_ ? l_root_->set(index, value) : n_root_->set(index, value);
    }

    uint64_t bits_size() {
        uint64_t tree_size = root_is_leaf_ ? l_root_->bits_size() : n_root_->bits_size();
        return 8 * sizeof(bit_vector) + tree_size;
    }
};

#endif