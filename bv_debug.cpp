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
    n->has_leaves(true);
    leaf* l = a->template allocate_leaf<leaf>(size / 64);
    uint64_t m = 0;
    for (uint64_t j = 0; j < size; j++) {
        l->insert(j, (m++) % 2);
    }
    n->append_child(l);

    l = a->template allocate_leaf<leaf>(size / 64);
    for (uint64_t j = 0; j < size / 3; j++) {
        l->insert(j, (m++) % 2);
    }
    n->append_child(l);

    assert((2u) == (n->child_count()));
    assert((m) == (n->size()));
    assert((m / 2) == (n->p_sum()));

    n->template remove<alloc>(5 * size / 6);
    n->template remove<alloc>(5 * size / 6);

    assert((2u) == (n->child_count()));
    assert((m - 2) == (n->size()));
    assert(m / 2 - 1 == n->p_sum());
    for (uint64_t j = 0; j < m - 2; j++) {
        assert(j % 2 == n->at(j));
    }

    uint64_t rem = size / 3;
    rem += rem % 2 ? 1 : 2;
    for (uint64_t i = 0; i < rem; i++) {
        n->template remove<alloc>(2 * size / 3 + 2);
    }

    assert((2u) == (n->child_count()));
    assert((m - rem - 2) == (n->size()));
    assert(m / 2 - rem / 2 - 1 == n->p_sum());
    for (uint64_t j = 0; j < m - rem - 2; j++) {
        assert(j % 2 == n->at(j));
    }

    for (uint64_t i = 0; i < rem; i++) {
        n->template remove<alloc>(size / 2 + 2);
    }

    assert((1u) == (n->child_count()));
    assert((m - 2 * rem - 2) == (n->size()));
    assert(m / 2 - rem - 1 == n->p_sum());
    for (uint64_t j = 0; j < m - 2 * rem - 2; j++) {
        assert(j % 2 == n->at(j));
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    assert((0u) == (a->live_allocations()));
    delete(a);
}
