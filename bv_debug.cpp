#include <iostream>
#include <iomanip>

#include "bit_vector/simple_node.hpp"
#include "bit_vector/simple_leaf.hpp"
#include "bit_vector/allocator.hpp"

typedef uint32_t data_type;
typedef malloc_alloc<data_type> allocator;
typedef simple_leaf<data_type, allocator, 8> leaf;
typedef simple_node<data_type, leaf, allocator, 16384> node;

int main()
{
    allocator* alloc = new allocator();

    leaf* l = alloc->allocate_leaf<leaf>(8);

    for (uint64_t i = 0; i < 10000; i++) {
        l->insert(0, bool(i % 2));
        if (l->need_realloc()) {
            data_type cap = l->capacity();
            l = alloc->template reallocate_leaf<leaf>(l, cap, 2 * cap);
        }
    }

    std::cout << "BV: ";
    for (data_type i = 0; i < 30; i++) {
        std::cout << l->at(i) << " ";
    }
    std::cout << std::endl;

    for (data_type i = 0; i < 4; i++) {
        data_type val = l->rank(i);
        std::cout << "Rank(" << i << ") = " << val << std::endl;
    }

    for (data_type i = 1; i < 3; i++) {
        data_type val = l->select(i);
        std::cout << "Select(" << i << ") = " << val << std::endl;
    }
    
    alloc->template deallocate_leaf<leaf>(l);
    
    delete alloc;
    return 0;
}
