#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>

#include "bit_vector/bv.hpp"
//#include "deps/valgrind/callgrind.h"
//#include "deps/DYNAMIC/include/dynamic/dynamic.hpp"
//#include "test/run_tests.hpp"

typedef bv::malloc_alloc alloc;
typedef bv::gap_leaf<16384, 32, 7> leaf;

/*typedef bv::leaf<16, 16384> leaf;
typedef bv::leaf<16, 16384, true, true> rl_l;
typedef bv::node<leaf, uint64_t, 16384, 64> node;
typedef bv::simple_bv<16, 16384, 64, true, true, true> h_rle;//*/

int main() {
    uint32_t elems = 10000;
    uint32_t offset = 1000;
    alloc* allocator = new alloc();
    leaf* l = allocator->template allocate_leaf<leaf>();
    for (uint32_t i = 0; i < offset * 2; i++) {
        if (l->need_realloc()) {
            l = allocator->template reallocate_leaf<leaf>(l, l->capacity(), l->desired_capacity());
        }
        l->insert(i, i % 2);
        for (uint32_t j = 0; j < l->size(); j++) {
            if (l->at(j) != j % 2) {
                std::cerr << "i = " << i << ", j = " << j << std::endl;
                exit(1); 
            }
            l->validate();
        }
    }
    for (uint32_t i = offset * 2; i < elems; i++) {
        if (l->need_realloc()) {
            //std::cerr << "Ralloc " << l->capacity() << " to " << l->desired_capacity() << " at i = " << i << std::endl;
            l = allocator->template reallocate_leaf<leaf>(l, l->capacity(), l->desired_capacity());
        }
        
        //std::cerr << "i = " << i << std::endl;
        if (i == 2144) {
            l->print(false);
            std::cerr << std::endl;
        }
        l->insert(offset, 0);
        if (i == 2144) {
            l->print(false);
            std::cerr << std::endl;
        }
        for (uint32_t j = 0; j < offset; j++) {
            if (l->at(j) != j % 2) {
                std::cerr << "i = " << i << ", aj = " << j << std::endl;
                exit(1); 
            }
        }
        for (uint32_t j = offset; j < l->size() - offset; j++) {
            if (l->at(j)) {
                std::cerr << "i = " << i << ", bj = " << j << std::endl;
                exit(1); 
            }
        }
        for (uint32_t j = 0; j < offset; j++) {
            if (l->at(l->size() - offset + j) != ((offset + j) % 2)) {
                std::cerr << "i = " << i << ", cj = " << l->size() - offset + j << std::endl;
                exit(1);
            }
        }
        l->validate();
    }
    assert(l->size() == elems);

    /*alloc* a = new alloc();
    rl_l* l = a->template allocate_leaf<rl_l>(32, 1000, false);

    for (size_t i = 500; i < 1000; i++) {
        l->set(i, true);
    }

    l->insert(200, true);
    l->insert(900, false);

    l->clear_last(400);

    a->deallocate_leaf(l);
    delete a;*/

    /*uint64_t size = 16384;
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    for (uint64_t i = 0; i < 2; i++) {
        leaf* l = a->template allocate_leaf<leaf>(size / 64);
        for (uint64_t j = 0; j < size; j++) {
            l->insert(j, j % 2);
        }
        n->append_child(l);
    }

    assert(2u == n->child_count());
    assert(2u * size == n->size());
    assert(2u * size / 2 == n->p_sum());

    for (uint64_t i = 2u * size - 1; i < 2u * size; i -= 2) {
        n->remove(i, a);
    }

    n->print();
    std::cout << std::endl;

    assert(2u == n->child_count());
    assert(1u * size == n->size());
    assert(1u * 0 == n->p_sum());

    for (uint64_t i = 0; i < size / 2; i++) {
        assert(false == n->at(i));
    }

    n->deallocate(a);
    a->deallocate_node(n);
    assert(0u == a->live_allocations());
    delete (a);*/

    /*uint64_t a[] = {111, 2, 81, 0, 2, 105, 0, 2, 66, 0, 2, 90, 0, 1, 109, 0, 89, 0, 0, 42, 0, 0, 72, 0, 2, 107, 0, 0, 69, 0, 2, 104, 0, 0, 61, 0, 2, 31, 0, 2, 59, 0, 2, 10, 1, 0, 37, 0, 0, 14, 0, 2, 85, 0, 0, 51, 0, 2, 82, 0, 2, 89, 0, 2, 55, 0, 0, 91, 0, 1, 16, 2, 80, 0, 1, 34, 2, 87, 0, 0, 30, 0, 2, 104, 0, 0, 86, 0, 1, 41, 1, 19, 1, 84, 2, 58, 0, 2, 81, 0, 0, 55, 0, 2, 113, 0, 0, 11, 0, 0, 25, 0, 2, 103, 0, 1, 69, 1, 52, 2, 29, 0, 0, 11, 1, 1, 27, 2, 93, 0, 1, 92, 2, 92, 0, 1, 53, 2, 36, 0, 2, 85, 0, 0, 4, 0, 1, 114, 2, 26, 0, 1, 66, 0, 93, 0, 0, 5, 0, 2, 83, 0, 2, 106, 0, 1, 15, 0, 89, 0, 0, 13, 0};
    run_test<h_rle, bv::bv>(a, 173, 1);*/

    /*uint64_t a[] = {
        26198, 1, 8440
    };
    run_sdsl_test<bv::bv>(a, 3);*/
    
    
    /*uint64_t a[] = {1570};
    run_test<bv::simple_bv<8, 256, 8>, dyn::suc_bv>(a, 1);*/

    /*uint64_t a[] = {16387, 2,     7184, 1,    1,    9890,  1,    10662,
                    1,     11795, 1,    7332, 0,    14649, 1,    2,
                    14480, 1,     2,    6282, 0,    2,     4190, 1,
                    2,     16170, 0,    1,    12969};
    run_sup_test<bv::bv>(a, 29);*/
}