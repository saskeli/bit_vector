#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>

#include "bit_vector/bv.hpp"
//#include "deps/valgrind/callgrind.h"
#include "deps/DYNAMIC/include/dynamic/dynamic.hpp"
//#include "sdsl/bit_vectors.hpp"

typedef bv::malloc_alloc alloc;
typedef bv::leaf<16> leaf;
typedef bv::node<leaf, uint64_t, 16384, 16> node;
typedef bv::simple_bv<8, 16384, 64> bit_vector;

template <class bva, class bvb>
void check(bva* a, bvb* b, uint64_t size) {
    assert(a->size() == size);
    assert(b->size() == size);

    for (uint64_t i = 0; i < size; i++) {
        if (a->at(i) != b->at(i)) {
            std::cout << "Problem at i = " << i << std::endl;
            std::cout << "at(" << i << ") = " << a->at(i) << ", " << b->at(i) << std::endl;
            assert(a->at(i) == b->at(i));
        }
        assert(a->rank(i) == b->rank(i));
    }

    uint64_t ones = a->rank(size);
    assert(a->rank(size) == b->rank(size));

    for (uint64_t i = 0; i < ones; i++) {
        assert(a->select(i + 1) == b->select(i));
    }
}

template <class bva, class bvb>
void insert(bva* a, bvb* b, uint64_t loc, bool val) {
    val = val ? true : false;
    a->insert(loc, val);
    b->insert(loc, val);
}

template <class bva, class bvb>
void remove(bva* a, bvb* b, uint64_t loc) {
    a->remove(loc);
    b->remove(loc);
}

template <class bva, class bvb>
void bv_set(bva* a, bvb* b, uint64_t loc, bool val) {
    val = val ? true : false;
    a->set(loc, val);
    b->set(loc, val);
}

template<class bit_vector, class c_bv>
void run_test(uint64_t* input, uint64_t len, bool show_index) {
    bit_vector bv;
    c_bv cbv;

    uint64_t size = input[0];

    for (uint64_t i = 0; i < size; i++) {
        if (show_index) std::cout << i << std::endl;
        bv.insert(0, i % 2);
        cbv.insert(0, i % 2);
        check(&bv, &cbv, i + 1);
    }

    //bv.print(false);

    uint64_t index = 1;
    while (index < len) {
        switch (input[index])
        {
        case 0:
            insert(&bv, &cbv, input[index + 1], input[index + 2]);
            size++;
            index += 3;
            break;
        case 1:
            remove(&bv, &cbv, input[index + 1]);
            size--;
            index += 2;
            break;
        default:
            bv_set(&bv, &cbv, input[index + 1], input[index + 2]);
            index += 2;
        }
        //bv.print(false);
        check(&bv, &cbv, size);
    }
}

int main() {
    uint64_t size = 16384;
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    for (uint8_t i = 0; i < 2; i++) {
        leaf* l = a->template allocate_leaf<leaf>(size / 64);
        for (uint64_t i = 0; i < size - 2; i++) {
            l->insert(i, i % 2);
        }
        n->append_child(l);
    }

    assert((size - 2) == (n->p_sum()));
    assert((size * 2 - 4) == (n->size()));
    assert((2u) == (n->child_count()));

    for (uint64_t i = 0; i < 2 * size - 4; i++) {
        assert((i % 2) == n->at(i));
    }
    n->print(false);
    for (uint64_t i = 0; i < 4; i++) {
        n->insert(i, i % 2, a);
    }
    n->print(false);

    assert((size) == (n->p_sum()));
    assert((size * 2) == (n->size()));
    assert((3u) == (n->child_count()));

    for (uint64_t i = 0; i < 2 * size; i++) {
        if ((i % 2) != n->at(i)) {
            std::cout << "i = " << i << std::endl;
            assert((i % 2) == n->at(i));
        }
    }

    n->deallocate(a);
    a->deallocate_node(n);
    assert((0u) == (a->live_allocations()));
    delete (a);

    /*uint64_t a[] = {
        377, 0,   50,  0,   0,   284, 0,   2,   357, 1,   2,   233, 1,   2,
        251, 0,   1,   148, 0,   83,  0,   1,   137, 2,   286, 0,   0,   178,
        1,   0,   352, 0,   0,   163, 1,   1,   288, 0,   158, 0,   1,   121,
        2,   263, 0,   2,   322, 1,   1,   254, 0,   57,  1,   1,   170, 0,
        353, 1,   1,   24,  0,   348, 1,   0,   65,  1,   0,   254, 1,   2,
        221, 1,   2,   187, 0,   0,   243, 0,   2,   19,  1,   1,   7,   2,
        264, 1,   1,   140, 1,   243, 0,   313, 0,   0,   362, 1,   0,   340,
        1,   2,   280, 1,   0,   304, 0,   2,   262, 0,   1,   344, 0,   320,
        0,   2,   318, 0,   2,   12,  1,   0,   352, 1,   0,   79,  0,   2,
        295, 1,   0,   7,   1,   0,   220, 1,   2,   351, 1,   1,   126, 2,
        326, 0,   2,   81,  0,   0,   107, 0,   0,   277, 1,   2,   20,  0,
        1,   341, 1,   286, 0,   127, 0,   2,   376, 0,   2,   338, 1,   1,
        270, 1,   184, 1,   15,  1,   299, 1,   45,  0,   361, 0,   2,   70,
        0,   2,   46,  0,   0,   255, 0,   2,   374, 1,   2,   310, 0,   0,
        52,  1,   0,   29,  0,   1,   124, 1,   282, 1,   145, 1,   280, 1,
        276, 2,   338, 0,   0,   239, 0,   2,   217, 0,   2,   41,  1,   0,
        381, 1,   2,   355, 1,   0,   238, 1,   0,   365, 0,   2,   155, 0,
        2,   65,  0,   2,   219, 0,   2,   168, 0,   2,   54,  0,   1,   238,
        2,   377, 1,   1,   325, 1,   364, 1,   327, 2,   343, 0,   0,   85,
        0,   1,   162, 1,   297, 2,   51,  0,   1,   94,  2,   6,   1,   1,
        162, 0,   147, 1,   2,   186, 1,   2,   19,  0,   0,   322, 1,   1,
        259, 0,   84,  0,   0,   193, 1,   0,   222, 0,   2,   240, 1,   0,
        169, 0,   2,   62,  1,   1,   247, 0,   263, 1,   0,   151, 1,   2,
        336, 0,   1,   231, 1,   276, 0,   128, 1,   0,   124, 1,   1,   299,
        0,   134, 0,   2,   235, 1,   1,   1,   2,   224, 1,   1,   79,  1,
        332, 2,   305, 0,   0,   254, 1,   0,   251, 1,   1,   236, 1,   83,
        2,   75,  0,   0,   133, 1,   2,   306, 1,   1,   23,  0,   10,  1};
    run_test<bit_vector, dyn::suc_bv>(a, 378, false);*/

    /*uint64_t seed = 2586862946;
    uint64_t size = 1000000000;
    uint64_t steps = 100;
    bit_vector bv;
    dyn::suc_bv cbv;

    std::mt19937 mt(seed);
    std::uniform_int_distribution<unsigned long long> gen(
        std::numeric_limits<std::uint64_t>::min(),
        std::numeric_limits<std::uint64_t>::max());

    uint64_t start_size = 1000000;

    double startexp = log2(double(start_size));
    double delta = (log2(double(size)) - log2(double(start_size))) / steps;
    uint64_t ops = 100000;
    std::cerr << "startexp: " << startexp << ". delta: " << delta;
    std::cerr << "\nWith seed: " << seed << std::endl;
    std::vector<uint64_t> loc, val;

    std::cout << "Filling to 900000..." << std::flush;
    for (uint64_t i = 0; i < 900000; i++) {
        uint64_t aloc = gen(mt) % (i + 1);
        bool aval = gen(mt) % 2;
        bv.insert(aloc, aval);
        cbv.insert(aloc, aval);
    }
    std::cout << "OK" << std::endl;
    for (uint64_t step = 1; step <= steps; step++) {
        uint64_t start = bv.size();
        uint64_t target = uint64_t(pow(2.0, startexp + delta * step));

        uint64_t checksum = 0;

        std::cout << start << " -> " << target << "..." << std::flush;

        for (size_t i = start; i < target; i++) {
            uint64_t aloc = gen(mt) % (i + 1);
            bool aval = gen(mt) % 2;
            bv.insert(aloc, aval);
            cbv.insert(aloc, aval);
            if (target == 7413102 && (i == 6954489 || i == 6954488)) {
                std::cout << " " << i << " ";
                bv.validate();
                check<bit_vector, dyn::suc_bv>(&bv, &cbv, bv.size());
            }
        }

        if (target == 7413102) {
            std::cout << " B ";
            check<bit_vector, dyn::suc_bv>(&bv, &cbv, bv.size());
        }

        loc.clear();
        for (uint64_t i = target; i > target - ops; i--) {
            loc.push_back(gen(mt) % i);
        }

        for (size_t i = 0; i < ops; i++) {
            bv.remove(loc[i]);
            cbv.remove(loc[i]);
        }

        loc.clear();
        val.clear();
        for (uint64_t i = bv.size(); i < target; i++) {
            loc.push_back(gen(mt) % (i + 1));
            val.push_back(gen(mt) % 2);
        }

        for (size_t i = 0; i < loc.size(); i++) {
            bv.insert(loc[i], val[i]);
            cbv.insert(loc[i], val[i]);
        }

        loc.clear();
        for (size_t i = 0; i < ops; i++) {
            loc.push_back(gen(mt) % target);
        }

        for (size_t i = 0; i < ops; i++) {
            checksum += bv.at(loc[i]);
            checksum -= cbv.at(loc[i]);
        }
        assert(checksum == 0);

        loc.clear();
        for (size_t i = 0; i < ops; i++) {
            loc.push_back(gen(mt) % target);
        }

        for (size_t i = 0; i < ops; i++) {
            checksum += bv.rank(loc[i]);
            checksum -= cbv.rank(loc[i]);
        }
        assert(checksum == 0);

        uint64_t limit = bv.rank(target - 1);
        assert(limit == cbv.rank(target - 1));
        loc.clear();
        for (size_t i = 0; i < ops; i++) {
            loc.push_back(gen(mt) % limit);
        }

        for (size_t i = 0; i < ops; i++) {
            checksum += bv.select(loc[i] + 1);
            checksum -= cbv.select(loc[i]);
        }
        assert(checksum == 0);
        std::cout << "OK" << std::endl;
    }

    uint64_t tot_size = bv.size();
    check<bit_vector, dyn::suc_bv>(&bv, &cbv, tot_size);
    std::cout << "PASSED!" << std::endl;*/
}