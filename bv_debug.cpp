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
    uint64_t size = 16384;
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    node* c = a->template allocate_node<node>();
    c->has_leaves(true);
    for (uint64_t i = 0; i < 21; i++) {
        leaf* l = a->template allocate_leaf<leaf>(size / 64);
        for (uint64_t j = 0; j < size; j++) {
            l->insert(j, j % 2);
        }
        c->append_child(l);
    }
    n->append_child(c);

    c = a->template allocate_node<node>();
    c->has_leaves(true);
    for (uint64_t i = 0; i < 64; i++) {
        leaf* l = a->template allocate_leaf<leaf>(size / 64);
        for (uint64_t j = 0; j < size; j++) {
            l->insert(j, j % 2);
        }
        c->append_child(l);
    }
    n->append_child(c);

    assert((2u) == (n->child_count()));
    assert((64u * size + 21u * size) == (n->size()));
    assert((32u * size + 21u * size / 2) == (n->p_sum()));

    n->template remove<alloc>(0);

    assert((2u) == (n->child_count()));
    assert((64u * size + 21u * size - 1) == (n->size()));
    assert((32u * size + 21u * size / 2) == (n->p_sum()));
    node* n1 = reinterpret_cast<node*>(n->child(0));
    node* n2 = reinterpret_cast<node*>(n->child(1));
    assert(abs(n1->child_count() - n2->child_count()) <= 1);
    for (uint64_t j = 0; j < 64u * size + 21u * size - 1; j++) {
        assert((j % 2 == 0) == (n->at(j)));
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    assert((0u) == (a->live_allocations()));
    delete(a);
}
