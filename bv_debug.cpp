#include <iostream>
#include <iomanip>
#include <cassert>

#include "bit_vector/allocator.hpp"
#include "bit_vector/simple_leaf.hpp"
#include "bit_vector/simple_node.hpp"
//#include "bit_vector/bit_vector.hpp"

typedef uint32_t data_type;
typedef simple_leaf<8> leaf;
typedef simple_node<leaf, 16384> node;
typedef malloc_alloc alloc;
//typedef bit_vector<leaf, node, malloc_alloc, 16384> simple_bv;

int main() {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    for (uint64_t i = 0; i < 20; i++) {
        node* c = a->template allocate_node<node>();
        c->has_leaves(true);
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, false);
        }
        c->append_child(l);
        n->append_child(c);
    }
    for (uint64_t i = 0; i < 20; i++) {
        node* c = a->template allocate_node<node>();
        c->has_leaves(true);
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, true);
        }
        c->append_child(l);
        n->append_child(c);
    }
    for (uint64_t i = 0; i < 20; i++) {
        node* c = a->template allocate_node<node>();
        c->has_leaves(true);
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, j % 2 != 0);
        }
        c->append_child(l);
        n->append_child(c);
    }
    assert((30u * 128) == (n->p_sum()));
    assert((60u * 128) == (n->size()));
    assert((60u) == (n->child_count()));

    for (uint64_t i = 0; i < 20u * 128; i++) {
        assert((false) == (n->at(i)));
    }
    for (uint64_t i = 20u * 128; i < 40u * 128; i++) {
        assert((true) == (n->at(i)));
    }
    for (uint64_t i = 40u * 128; i < 60u * 128; i++) {
        assert((i % 2 == 0) == (n->at(i)));
    }
    n->template deallocate<alloc>();
    a->deallocate_node(n);
    assert((0u) == (a->live_allocations()));
    delete(a);
}
