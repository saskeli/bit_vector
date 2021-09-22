#ifndef TEST_RUN_HPP
#define TEST_RUN_HPP

#include <cstdint>
#include "sdsl/bit_vectors.hpp"

#ifdef GTEST_ON
#include "../deps/googletest/googletest/include/gtest/gtest.h"
#endif

#include <cassert>

void print_sdsl(sdsl::bit_vector* sdsl_bv) {
    uint64_t s = sdsl_bv->size();
    for (size_t i = 0; i < s; i++) {
        if (i % 8 == 0 && i > 0) std::cout << " ";
        if (i % 64 == 0 && i > 0) std::cout << std::endl;
        std::cout << sdsl_bv[0][i];
    }
    std::cout << std::endl;
}

template <class bva, class bvb>
void check(bva* a, bvb* b, uint64_t size) {
#ifdef GTEST_ON
    ASSERT_EQ(a->size(), size);
    ASSERT_EQ(b->size(), size);
#else
    assert(a->size() == size);
    assert(b->size() == size);
#endif
    for (uint64_t i = 0; i < size; i++) {
#ifdef GTEST_ON
        ASSERT_EQ(a->at(i), b->at(i)) << "Failed at " << i << " with size " << size;
        ASSERT_EQ(a->rank(i), b->rank(i));
#else
        assert(a->at(i) == b->at(i));
        assert(a->rank(i) == b->rank(i));
#endif
    }

    uint64_t ones = a->rank(size);
#ifdef GTEST_ON
    ASSERT_EQ(b->rank(size), ones);
#else
    assert(b->rank(size) == ones);
#endif
    for (uint64_t i = 0; i < ones; i++) {
#ifdef GTEST_ON
        ASSERT_EQ(a->select(i + 1), b->select(i)) << "i = " << i;
#else
        uint64_t ex = b->select(i);
        uint64_t ac = a->select(i + 1);
        if (ex != ac) {
            for (size_t i = 0; i < 64; i++) {
                std::cout << a->at(i);
            }
            std::cout << std::endl;
            std::cerr << "select(" << i + 1 << ") should be " << ex << ", was " << ac << std::endl;
            a->print(false);
        }
        assert(ac == ex);
#endif
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

template <class bit_vector, class c_bv>
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
#ifndef GTEST_ON
        std::cout << "index = " << index << std::endl;
#endif
        switch (input[index]) {
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
                index += 3;
        }
        check(&bv, &cbv, size);
    }
}

template <class bit_vector>
void check_sup(bit_vector* bv, uint64_t size) {
    auto* q = bv->generate_query_structure();
#ifdef GTEST_ON
    ASSERT_EQ(q->size(), size);
    ASSERT_EQ(bv->size(), size);
#else
    assert(q->size() == size);
    assert(bv->size() == size);
#endif

    for (uint64_t i = 0; i < size; i++) {
#ifdef GTEST_ON
        ASSERT_EQ(q->at(i), bv->at(i));
        ASSERT_EQ(q->rank(i), bv->rank(i));
#else
        assert(q->at(i) == bv->at(i));
        assert(q->rank(i) == bv->rank(i));
#endif
    }

    uint64_t ones = bv->rank(size);
#ifdef GTEST_ON
    ASSERT_EQ(q->rank(size), ones);
#else
    assert(q->rank(size) == ones);
#endif

    for (uint64_t i = 1; i <= ones; i++) {
#ifdef GTEST_ON
        ASSERT_EQ(q->select(i), bv->select(i)) << "i = " << i;
#else
        assert(q->select(i) == bv->select(i));
#endif
    }
    delete (q);
}

template <class bit_vector>
void insert_sup(bit_vector* bv, uint64_t loc, bool val) {
    bv->insert(loc, val);
}

template <class bit_vector>
void remove_sup(bit_vector* bv, uint64_t loc) {
    bv->remove(loc);
}

template <class bit_vector>
void set_sup(bit_vector* bv, uint64_t loc, bool val) {
    bv->set(loc, val);
}

template <class bit_vector>
void run_sup_test(uint64_t* input, uint64_t len) {
    bit_vector bv;
    uint64_t size = input[0];
    for (uint64_t i = 0; i < size; i++) {
        bv.insert(0, i % 2);
    }

    check_sup(&bv, size);

    uint64_t index = 1;
    while (index < len) {
        switch (input[index]) {
            case 0:
                insert_sup(&bv, input[index + 1], input[index + 2]);
                size++;
                index += 3;
                break;
            case 1:
                remove_sup(&bv, input[index + 1]);
                size--;
                index += 2;
                break;
            default:
                set_sup(&bv, input[index + 1], input[index + 2]);
                index += 3;
        }
        check_sup(&bv, size);
    }
}

template <class bit_vector>
void check_sdsl(bit_vector* bv, uint64_t size) {
    sdsl::bit_vector sdsl_bv(size);
    bv->dump(sdsl_bv.data());
    sdsl::bit_vector::rank_1_type sdsl_rank(&sdsl_bv);
    sdsl::bit_vector::select_1_type sdsl_select(&sdsl_bv);
#ifdef GTEST_ON
    ASSERT_EQ(sdsl_bv.size(), bv->size());
#else
    assert(sdsl_bv.size() == bv->size());
#endif

    for (uint64_t i = 0; i < size; i++) {
#ifdef GTEST_ON
        ASSERT_EQ(bv->at(i), sdsl_bv[i]);
        ASSERT_EQ(sdsl_rank(i), bv->rank(i));
#else
        uint64_t ex = bv->at(i);
        uint64_t ac = sdsl_bv[i];
        if (ex != ac) {
            bv->print(false);
            std::cout << "!=" << std::endl;
            print_sdsl(&sdsl_bv);
            assert(ex == ac);
        }
        assert(sdsl_rank(i) == bv->rank(i));
#endif
    }

    uint64_t ones = bv->rank(size);
    for (uint64_t i = 1; i <= ones; i++) {
#ifdef GTEST_ON
        ASSERT_EQ(sdsl_select(i), bv->select(i));
#else
        assert(sdsl_select(i) == bv->select(i));
#endif
    }
}

template <class bit_vector>
void run_sdsl_test(uint64_t* input, uint64_t len) {
    bit_vector bv;
    uint64_t size = input[0];
    for (uint64_t i = 0; i < size; i++) {
        bv.insert(0, i % 2);
    }

    check_sdsl(&bv, size);

    uint64_t index = 1;
    while (index < len) {
        switch (input[index]) {
            case 0:
                insert_sup(&bv, input[index + 1], input[index + 2]);
                size++;
                index += 3;
                break;
            case 1:
                remove_sup(&bv, input[index + 1]);
                size--;
                index += 2;
                break;
            default:
                set_sup(&bv, input[index + 1], input[index + 2]);
                index += 3;
        }
        check_sdsl(&bv, size);
    }
}

#endif