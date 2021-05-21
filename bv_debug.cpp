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
//typedef bit_vector<leaf, node, malloc_alloc, 16384> simple_bv;

int main() {
    malloc_alloc* a = new malloc_alloc();
    node* nd = a->template allocate_node<node>();
    nd->has_leaves(true);
    for (uint64_t i = 0; i < 64; i++) {
        leaf* l = a->template allocate_leaf<leaf>(4);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, j % 2 == 0);
        }
        nd->append_child(l);
    }

    assert(64u ==  nd->child_count());
    assert(64u * 128 ==  nd->size());
    assert(64u * 64 ==  nd->p_sum());
    uint64_t* counts = nd->child_sizes();
    uint64_t* sums = nd->child_sums();
    for (uint64_t i = 0; i < 64; i++) {
        assert((i + 1) * 128 ==  counts[i]);
        assert((i + 1) * 64 ==  sums[i]);
    }

    nd->template deallocate<malloc_alloc>();
    a->deallocate_node(nd);
}
