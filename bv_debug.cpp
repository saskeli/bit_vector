#include <iostream>
#include <iomanip>

#include "bit_vector/simple_node.hpp"
#include "bit_vector/simple_leaf.hpp"
#include "bit_vector/allocator.hpp"

typedef simple_leaf<uint32_t, 8, 16384, malloc_alloc> leaf;

int main()
{
    malloc_alloc* alloc = new malloc_alloc();

    leaf* lefa = alloc->allocate_leaf<leaf, uint32_t>(8);

    for (uint64_t i = 0; i < 500; i++) {
        lefa->insert(0, bool(i % 2));
    }
    std::cout << "Content: \n";
    for (uint64_t i = 0; i < lefa->size(); i++) {
        std::cout << lefa->at(i) << " ";
    }
    std::cout << std::endl;

    alloc->deallocate_leaf<leaf>(lefa);
    
    delete alloc;
    return 0;
}
