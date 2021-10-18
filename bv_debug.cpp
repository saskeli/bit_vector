#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>

#include "bit_vector/bv.hpp"
//#include "deps/valgrind/callgrind.h"
#include "deps/DYNAMIC/include/dynamic/dynamic.hpp"
#include "test/run_tests.hpp"

typedef bv::malloc_alloc alloc;
typedef bv::leaf<16, 16384, true, true> leaf;
typedef bv::node<leaf, uint64_t, 16384, 64, true, true> node;
typedef bv::simple_bv<16, 16384, 64, true, true, true> h_rle;

int main() {
    alloc* a = new alloc();
    leaf* l = a->template allocate_leaf<leaf>(32, (~uint32_t(0)) >> 1, false);
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    n->append_child(l);
    n->insert(300, true, a);
    n->print();
    std::cout << std::endl;
    assert((n->child_count()) == (2u));
    assert((n->size()) == (((~uint32_t(0)) >> 1) + 1u));
    assert((n->p_sum()) == (1u));
    assert((n->at(300)) == (true));
    for (size_t i = 0; i < n->size(); i += 10000) {
        assert((n->at(i)) == (false));
        assert((n->rank(i)) == ((i > 300) * 1u));
    }
    assert((n->select(1)) == (300u));

    n->deallocate(a);
    a->deallocate_leaf(n);
    delete a;
    /*uint64_t a[] = {111, 2, 81, 0, 2, 105, 0, 2, 66, 0, 2, 90, 0, 1, 109, 0, 89, 0, 0, 42, 0, 0, 72, 0, 2, 107, 0, 0, 69, 0, 2, 104, 0, 0, 61, 0, 2, 31, 0, 2, 59, 0, 2, 10, 1, 0, 37, 0, 0, 14, 0, 2, 85, 0, 0, 51, 0, 2, 82, 0, 2, 89, 0, 2, 55, 0, 0, 91, 0, 1, 16, 2, 80, 0, 1, 34, 2, 87, 0, 0, 30, 0, 2, 104, 0, 0, 86, 0, 1, 41, 1, 19, 1, 84, 2, 58, 0, 2, 81, 0, 0, 55, 0, 2, 113, 0, 0, 11, 0, 0, 25, 0, 2, 103, 0, 1, 69, 1, 52, 2, 29, 0, 0, 11, 1, 1, 27, 2, 93, 0, 1, 92, 2, 92, 0, 1, 53, 2, 36, 0, 2, 85, 0, 0, 4, 0, 1, 114, 2, 26, 0, 1, 66, 0, 93, 0, 0, 5, 0, 2, 83, 0, 2, 106, 0, 1, 15, 0, 89, 0, 0, 13, 0};
    run_test<h_rle, bv::bv>(a, 173, 1);*/
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