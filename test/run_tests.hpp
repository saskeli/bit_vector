#ifndef TEST_RUN_HPP
#define TEST_RUN_HPP

#include <cstdint>

#include "../deps/googletest/googletest/include/gtest/gtest.h"

template <class bva, class bvb>
void check(bva* a, bvb* b, uint64_t size) {
    ASSERT_EQ(a->size(), size);
    ASSERT_EQ(b->size(), size);

    for (uint64_t i = 0; i < size; i++) {
        ASSERT_EQ(a->at(i), b->at(i));
        ASSERT_EQ(a->rank(i), b->rank(i));
    }

    uint64_t ones = a->rank(size);
    ASSERT_EQ(b->rank(size), ones);

    for (uint64_t i = 0; i < ones; i++) {
        ASSERT_EQ(a->select(i + 1), b->select(i)) << "i = " << i;
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
void run_test(uint64_t* input, uint64_t len) {
    bit_vector bv;
    c_bv cbv;

    uint64_t size = input[0];

    for (uint64_t i = 0; i < size; i++) {
        bv.insert(0, i % 2);
        cbv.insert(0, i % 2);
    }

    check(&bv, &cbv, size);

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
        check(&bv, &cbv, size);
    }
}

#endif