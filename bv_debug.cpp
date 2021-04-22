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

    uint8_t* data = reinterpret_cast<uint8_t*>(lefa);
    for (size_t i = 0; i < (sizeof(leaf) + 8 * sizeof(uint64_t)); i++) {
        if (i % 40 == 0) std::cout << "\n";
        if (i % 4 == 0) std::cout << " ";
        std::cout << std::setfill('0') << std::setw(2) << std::hex << uint16_t(data[i]);
    }
    std::cout << std::endl;

    alloc->deallocate_leaf<leaf>(lefa);
    
    delete alloc;
    return 0;
}
