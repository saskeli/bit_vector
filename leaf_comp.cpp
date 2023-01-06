#include <iostream>
#include <new>

#include "bit_vector/internal/new_leaf.hpp"

typedef bv::leaf<16, 1 << 14, false, true> leaf;

int main() {
    void* buffer = calloc(1024, 1);
    leaf* l = new (buffer)leaf(2, 24, true);
    std::cout << "leaf with " << l->size() << " elements and " << l->p_sum() << " 1-bits" << std::endl;
    for (size_t i = 0; i < l->size(); i++) {
        std::cout << "l->at(" << i << ") = " << l->at(i) << std::endl;
    }
    return 0;
}
