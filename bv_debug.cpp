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
    leaf* c = allocator->template allocate_leaf<leaf>(5);

    for (uint64_t i = 0; i < 96; i++) {
        a->insert(0, false);
        b->insert(0, true);
        c->insert(0, i % 2 == 1);
    }

    assert(96u == a->size());
    assert(0u == a->p_sum());
    assert(96u == b->size());
    assert(96u == b->p_sum());
    assert(96u == c->size());
    assert(48u == c->p_sum());

    for (uint64_t i = 0; i < 96; i++) {
        assert(false == a->at(i));
        assert(true == b->at(i));
        assert((i % 2 == 0) == c->at(i));
    }

    std::cout << "Leaf a:\n";
    a->print();
    std::cout << "\nLeaf b:\n";
    b->print();

    a->append_all(b);
    assert(96u * 2 == a->size());
    assert(96u == a->p_sum());
    std::cout << "\nLeaf a with b appended:\n";
    a->print();
    std::cout << std::endl;
    for (uint64_t i = 0; i < 96 * 2; i++) {
        assert((i > 96) == a->at(i));
    }

    a->append_all(c);
    assert(96u * 3 == a->size());
    assert(96u + 48 == a->p_sum());
    for (uint64_t i = 0; i < 96 * 2; i++) {
        assert((i > 96) == a->at(i));
    }
    for (uint64_t i = 96 * 2; i < 96 * 3; i++) {
        assert((i % 2 == 0) == a->at(i));
    }

    allocator->template deallocate_leaf<leaf>(a);
    allocator->template deallocate_leaf<leaf>(b);
    allocator->template deallocate_leaf<leaf>(c);
    delete allocator;
}
