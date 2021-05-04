
#ifndef BV_BIT_VECTOR_HPP
#define BV_BIT_VECTOR_HPP

#include <cstdint>

template <class leaf, class node, class allocator>
class bit_vector {
  private:
    node* root;
    allocator* allocator_;

    void split_root() {
        node* new_root = allocator_->allocate_node();
        node* sibling = root->spit();
        new_root->append_child(root);
        new_root->append_child(sibling);
        root = new_root;
    }

  public:
    bit_vector(allocator* alloc) {
        allocator_ = alloc;
        root = allocator_->allocate_node();
        root->has_leaves(true);
    }

    void insert(uint64_t index, bool value) {
        if (root->child_count() == 64) {
            [[unlikely]] split_root();
        }
        root->insert(index, value);
    }

    void remove(uint64_t index) {
        if (root->child_count() == 1) {
            node* new_root = root->child(0);
            allocator_->deallocate_node(root);
            [[unlikely]] root = new_root;
        }
        root->remove(index);
    }

    uint64_t size() {
        return root->size();
    }

    bool at() {
        return root->at();
    }

    uint64_t rank(uint64_t index) {
        root->rank(index);
    }

    uint64_t select(uint64_t count) {
        root->select(index);
    }

    void set(uint64_t index, bool value) {
        root->set(index, value);
    }

    uint64_t bits_size() {
        return 8 * sizeof(bit_vector) + root->bits_size();
    }
};

#endif