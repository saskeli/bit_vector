#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>

#include "bit_vector/bv.hpp"
//#include "deps/valgrind/callgrind.h"
#include "deps/DYNAMIC/include/dynamic/dynamic.hpp"
#include "test/run_tests.hpp"

int main() {
    uint32_t size = 10000;
    uint32_t i_count = 20;
    bv::malloc_alloc* a = new bv::malloc_alloc();
    bv::leaf<16, 16384, true, true>* l = a->allocate_leaf<bv::leaf<16, 16384, true, true>>(32, size, false);
    l->print(false);
    std::cout << std::endl;
    for (size_t i = 0; i < i_count; i++) {
        l->insert(size >> 1, true);
    }
    l->print(false);
    std::cout << std::endl;

    //for (size_t i = 0; i < (size >> 1); i++) {
    //    assert(l->at(i) == false);
    //}
    for (size_t i = 0; i < i_count; i++) {
        uint32_t ac = l->select(i + 1);
        uint32_t ex = (size >> 1) + i;
        if (ac != ex) {
            std::cout << "select(" << i + 1 << ") should be " << ex << " but was " << ac << std::endl;
            exit(1);
        }
    }
    //for (size_t i = (size >> 1) + i_count; i < size + i_count; i++) {
    //    if (l->at(i)) {
    //        std::cout << "bv[" << i << "] should be false" << std::endl;
    //        exit(1);
    //    }
    //}
    a->deallocate_leaf(l);
    delete a;
    /*uint64_t a[] = {
        26198, 1, 8440
    };
    run_sdsl_test<bv::bv>(a, 3);*/
    /*
    uint64_t a[] = {91, 0,  41, 1,  2,  69, 0,  2,  8, 0,  0, 61, 0,
                    2,  47, 0,  1,  20, 2,  11, 0,  1, 90, 2, 30, 1,
                    1,  27, 2,  49, 0,  2,  80, 1,  1, 5,  1, 6,  1,
                    58, 0,  19, 0,  1,  53, 1,  78, 2, 13, 1, 1,  62};
    run_test<bv::simple_bv<8, 256, 8>, dyn::suc_bv>(a, 52);*/
    /*uint64_t a[] = {16387, 2,     7184, 1,    1,    9890,  1,    10662,
                    1,     11795, 1,    7332, 0,    14649, 1,    2,
                    14480, 1,     2,    6282, 0,    2,     4190, 1,
                    2,     16170, 0,    1,    12969};
    run_sup_test<bv::bv>(a, 29);*/
}