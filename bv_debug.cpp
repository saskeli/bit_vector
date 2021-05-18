#include <iostream>
#include <iomanip>
#include <cassert>

#include "bit_vector/allocator.hpp"
#include "bit_vector/simple_leaf.hpp"
//#include "bit_vector/simple_node.hpp"
//#include "bit_vector/bit_vector.hpp"

typedef uint32_t data_type;
typedef simple_leaf<8> leaf;
//typedef simple_node<leaf, 16384> node;
//typedef bit_vector<leaf, node, malloc_alloc, 16384> simple_bv;

int main() {
    malloc_alloc* allocator = new malloc_alloc();
    leaf* a = allocator->template allocate_leaf<leaf>(10);
    leaf* b = allocator->template allocate_leaf<leaf>(5);

    uint64_t u = 64;

    for (uint64_t i = 0; i < 64 * 5; i++) {
        a->insert(0, false);
        b->insert(0, true);
    }

    a->transfer_prepend(b, 64);
    assert(u * 6 == a->size());
    assert(u * 1 == a->p_sum());
    assert(u * 4 == b->size());
    assert(u * 4 == b->p_sum());
    for (uint64_t i = 0; i < u * 6; i++) {
        assert((i < u * 1) == a->at(i));
    }
    for (uint64_t i = 0; i < u * 4; i++) {
        assert(true == b->at(i));
    }

    a->transfer_prepend(b, 16);
    assert(u * 6 + 16 == a->size());
    assert(u * 1 + 16 == a->p_sum());
    assert(u * 4 - 16 == b->size());
    assert(u * 4 - 16 == b->p_sum());
    for (uint64_t i = 0; i < u * 6 + 16; i++) {
        assert((i < u + 16) == a->at(i));
    }
    for (uint64_t i = 0; i < u * 4 - 16; i++) {
        assert(true == b->at(i));
    }

    a->transfer_prepend(b, 80);
    assert(u * 7 + 32 == a->size());
    assert(u * 2 + 32 == a->p_sum());
    assert(u * 3 - 32 == b->size());
    assert(u * 3 - 32 == b->p_sum());
    for (uint64_t i = 0; i < u * 7 + 32; i++) {
        assert((i < u * 2 + 32) == a->at(i));
    }
    for (uint64_t i = 0; i < u * 3 - 32; i++) {
        assert(true == b->at(i));
    }

    allocator->template deallocate_leaf<leaf>(a);
    allocator->template deallocate_leaf<leaf>(b);
    delete allocator;
}
