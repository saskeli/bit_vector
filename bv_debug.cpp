#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>

#include "bit_vector/bv.hpp"
//#include "deps/valgrind/callgrind.h"
#include "deps/DYNAMIC/include/dynamic/dynamic.hpp"
#include "test/run_tests.hpp"

typedef bv::leaf<16, 16384, true, false> rl_l;

int main() {
    bv::malloc_alloc* a = new bv::malloc_alloc();
    rl_l* la = a->template allocate_leaf<rl_l>(32);//, 980, false);
    for (size_t i = 0; i < 980; i++) {
        la->insert(i, 0);
    }
    //assert(la->is_compressed());
    assert((la->size()) == (980u));
    assert((la->p_sum()) == (0u));
    for (size_t i = 1; i < 980; i += 2) {
        la->set(i, 1);
    }
    //assert(!la->is_compressed());
    assert((la->size()) == (980u));
    assert((la->p_sum()) == (490u));
    for (size_t i = 0; i < 980; i += 2) {
        la->set(i, 1);
    }
    la->print(false);
    std::cout << std::endl;
    for (size_t i = 0; i < 20; i++) {
        la->insert(0, 1);
    }
    la->print(false);
    std::cout << std::endl;
    //assert(la->is_compressed());
    assert((la->size()) == (1000u));
    assert((la->p_sum()) == (1000u));

    a->deallocate_leaf(la);
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