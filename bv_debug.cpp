#include <iostream>
#include <iomanip>

#include "bit_vector/allocator.hpp"
#include "bit_vector/simple_leaf.hpp"
#include "bit_vector/simple_node.hpp"
#include "bit_vector/bit_vector.hpp"

typedef uint32_t data_type;
typedef simple_leaf<8> leaf;
typedef simple_node<leaf, 16384> node;
typedef bit_vector<leaf, node, malloc_alloc, 16384> simple_bv;

int main() {
    malloc_alloc* alloc = new malloc_alloc();
    
    simple_bv* bv = new simple_bv(alloc);

    delete bv;    
    delete alloc;
    return 0;
}
