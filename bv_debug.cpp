#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>

#include "bit_vector/bv.hpp"
//#include "deps/valgrind/callgrind.h"
//#include "deps/DYNAMIC/include/dynamic/dynamic.hpp"

typedef bv::malloc_alloc alloc;
typedef bv::simple_leaf<8> leaf;
typedef bv::simple_node<leaf, uint64_t, 16384, 64> node;
typedef bv::simple_bv<8, 16384, 64> bit_vector;

int main() {
    uint64_t size = 16384;
    uint8_t b = 64;
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    for (uint64_t i = 0; i < 2; i++) {
        node* c = a->template allocate_node<node>();
        c->has_leaves(true);
        for (uint64_t j = 0; j < b; j++) {
            leaf* l = a->template allocate_leaf<leaf>(size / 64);
            for (uint64_t j = 0; j < size; j++) {
                l->insert(j, false);
            }
            c->append_child(l);
        }
        assert((b) == (c->child_count()));
        n->append_child(c);
    }

    assert((2u) == (n->child_count()));
    assert((2u * size * b) == (n->size()));
    assert((2u * 0) == (n->p_sum()));

    for (uint64_t i = 2u * size * b; i > 0; i--) {
        if (i == 1736703) n->print(true);
        n->template insert<alloc>(i, true);
    }

    assert((4u) == (n->child_count()));
    assert((4u * size * b) == (n->size()));
    assert((2u * size * b) == (n->p_sum()));

    for (uint64_t i = 0; i < 4u * size * b; i++) {
        assert((i % 2) == (n->at(i)));
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    assert((0u) == (a->live_allocations()));
    delete (a);
}