#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>

#include "bit_vector/bv.hpp"
#include "deps/valgrind/callgrind.h"
#include "deps/DYNAMIC/include/dynamic/dynamic.hpp"

#define SIZE 256
#define BRANCH 8

typedef bv::malloc_alloc alloc;
typedef bv::simple_bv<8, SIZE, BRANCH> bit_vector;

void check(dyn::suc_bv* bva, bit_vector* bvb, uint64_t size) {
    assert(bva->size() == size);
    assert(bvb->size() == size);

    for (uint64_t i = 0; i < size; i++) {
        assert(bva->at(i) == bvb->at(i));
        assert(bva->rank(i) == bvb->rank(i));
    }

    uint64_t ones = bva->rank(size);
    assert(bvb->rank(size) == ones);

    for (uint64_t i = 0; i < ones; i++) {
        assert(bva->select(i) == bvb->select(i + 1));
    }
}

void insert(dyn::suc_bv* bva, bit_vector* bvb, uint64_t loc, bool val) {
    std::cout << loc << ", " << val << ", " << std::flush;
    val = val ? true : false;
    bva->insert(loc, val);
    bvb->insert(loc, val);
}

void remove(dyn::suc_bv* bva, bit_vector* bvb, uint64_t loc) {
    std::cout << loc << ", " << std::flush;
    bva->remove(loc);
    bvb->remove(loc);
}

void bv_set(dyn::suc_bv* bva, bit_vector* bvb, uint64_t loc, bool val) {
    std::cout << loc << ", " << val << ", " << std::flush;
    val = val ? true : false;
    bva->set(loc, val);
    bvb->set(loc, val);
}

int main() {

    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<unsigned long long> gen(
        std::numeric_limits<std::uint64_t>::min(),
        std::numeric_limits<std::uint64_t>::max());

    uint64_t counter = 1;
    uint64_t ops = 100000;
    while (true) {
        dyn::suc_bv bva;
        bit_vector bvb;
        uint64_t size = ops + (gen(mt) % (2 * BRANCH * BRANCH * SIZE));

        std::cout << counter++ << ": " << size << ", " << std::flush;

        for (uint64_t i = 0; i < size; i++) {
            bva.insert(0, i % 2);
            bvb.insert(0, i % 2);
        }

        check(&bva, &bvb, size);

        for (uint64_t i = 0; i < ops; i++) {
            uint64_t op = gen(mt) % 3;
            std::cout << op << ", " << std::flush;
            switch (op)
            {
            case 0:
                insert(&bva, &bvb, gen(mt) % (size + 1), gen(mt) % 2);
                check(&bva, &bvb, ++size);
                break;
            case 1:
                remove(&bva, &bvb, gen(mt) % size);
                check(&bva, &bvb, --size);
                break;
            default:
                bv_set(&bva, &bvb, gen(mt) % size, gen(mt) % 2);
                check(&bva, &bvb, size);
            }
        }
        std::cout << std::endl;
    }
}