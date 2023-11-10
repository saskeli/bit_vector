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
void check(bva* a, bvb* b, uint64_t size, uint64_t s_offset = 0) {
#ifdef GTEST_ON
    ASSERT_EQ(a->size(), size);
    ASSERT_EQ(b->size(), size);
#else
    assert(a->size() == size);
    assert(b->size() == size);
#endif
    for (uint64_t i = 0; i < size; i++) {
#ifdef GTEST_ON
        ASSERT_EQ(a->at(i), b->at(i))
            << "Failed at " << i << " with size " << size;
        ASSERT_EQ(a->rank(i), b->rank(i));
        ASSERT_EQ(a->rank0(i), b->rank0(i));
#else
        if (a->at(i) != b->at(i)) {
            std::cerr << "a->at(" << i << ") = " << a->at(i) << ", b->at(" << i
                      << ") = " << b->at(i) << std::endl;
            assert(a->at(i) == b->at(i));
        }
        assert(a->rank(i) == b->rank(i));
        assert(a->rank0(i) == b->rank0(i));
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
        ASSERT_EQ(a->select(i + 1), b->select(i + s_offset)) << "i = " << i;
#else
        uint64_t ex = b->select(i + s_offset);
        uint64_t ac = a->select(i + 1);
        if (ex != ac) {
            std::cerr << "select(" << i + 1 << ") should be " << ex
                      << ", was " << ac << std::endl;
        }
        assert(ac == ex);
#endif
    }

    uint64_t zeros = a->rank0(size);
#ifdef GTEST_ON
    ASSERT_EQ(b->rank0(size), zeros);
#else
    assert(b->rank0(size) == zeros);
#endif
    for (uint64_t i = 0; i < zeros; i++) {
#ifdef GTEST_ON
        ASSERT_EQ(a->select0(i + 1), b->select0(i + s_offset)) << "i = " << i;
#else
        uint64_t ex = b->select0(i + s_offset);
        uint64_t ac = a->select0(i + 1);
        if (ex != ac) {
            std::cerr << "select(" << i + 1 << ") should be " << ex
                      << ", was " << ac << std::endl;
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

template <class bit_vector, class control>
void run_test(uint64_t* input, uint64_t len, uint64_t s_offset) {
    bit_vector bv;
    control cbv;

    uint64_t size = input[0];

    for (uint64_t i = 0; i < size; i++) {
        
        bv.insert(0, 0);
        cbv.insert(0, 0);
    }

    check(&bv, &cbv, size, s_offset);

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
        check(&bv, &cbv, size, s_offset);
    }
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

#ifdef GTEST_ON
TEST(Run, A) {
    uint64_t a[] = {1570};
    run_test<simple_bv<8, 256, 8>, dyn::suc_bv>(a, 1);
}

TEST(Run, B) {
    uint64_t a[] = {4270};
    run_test<simple_bv<8, 256, 8>, dyn::suc_bv>(a, 1);
}

TEST(Run, C) {
    uint64_t a[] = {
        876, 1,   27,  2,   668, 1,   1,   594, 0,   26,  0,   2,   320, 1,
        2,   679, 1,   0,   82,  1,   0,   673, 0,   2,   201, 0,   1,   875,
        1,   157, 2,   219, 1,   0,   761, 1,   0,   531, 0,   1,   579, 1,
        738, 1,   636, 1,   571, 2,   152, 0,   0,   176, 1,   2,   453, 1,
        1,   93,  2,   795, 0,   1,   275, 2,   580, 1,   0,   406, 0,   2,
        290, 0,   1,   619, 0,   14,  0,   0,   817, 1,   0,   128, 1,   1,
        0,   2,   73,  0,   1,   333, 1,   799, 1,   352, 0,   678, 1,   0,
        667, 1,   1,   786, 0,   643, 0,   1,   295, 0,   340, 1,   0,   53,
        1,   2,   811, 1,   0,   416, 0,   2,   638, 0,   1,   36,  2,   700,
        1,   0,   278, 1,   2,   551, 0,   0,   101, 1,   1,   739, 2,   362,
        0,   0,   392, 0,   0,   608, 0,   0,   800, 1,   2,   4,   0,   1,
        798, 1,   173, 2,   417, 1,   0,   538, 1,   1,   876, 1,   111, 1,
        493, 0,   467, 1,   0,   765, 1,   1,   660, 0,   254, 1,   0,   603,
        0,   0,   499, 0,   1,   483, 1,   415, 1,   522, 2,   132, 1,   1,
        210, 0,   570, 0,   1,   613, 2,   741, 1,   0,   383, 0,   0,   0,
        1,   2,   205, 1,   1,   350, 1,   327, 1,   671, 2,   466, 0,   2,
        562, 0,   2,   728, 0,   0,   599, 1,   1,   514, 2,   762, 1,   2,
        555, 1,   1,   368, 1,   172, 2,   468, 1,   2,   762, 1,   2,   290,
        0,   2,   5,   1,   0,   242, 0,   1,   775, 0,   8,   0,   1,   799,
        0,   281, 1,   2,   242, 1,   0,   660, 1,   2,   401, 1,   0,   798,
        0,   2,   370, 1,   2,   20,  0,   2,   379, 1,   1,   436, 0,   154,
        1};
    run_test<simple_bv<8, 256, 8>, dyn::suc_bv>(a, 148);
}

TEST(Run, D) {
    uint64_t a[] = {
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
    run_test<simple_bv<8, 256, 8>, dyn::suc_bv>(a, 378);
}

TEST(Run, E) {
    uint64_t a[] = {160, 1, 37, 2, 81, 0, 0, 8, 1, 1, 64};
    run_test<bv::simple_bv<8, 256, 8>, dyn::suc_bv>(a, 11);
}

TEST(Run, F) {
    uint64_t a[] = {91, 0,  41, 1,  2,  69, 0,  2,  8, 0,  0, 61, 0,
                    2,  47, 0,  1,  20, 2,  11, 0,  1, 90, 2, 30, 1,
                    1,  27, 2,  49, 0,  2,  80, 1,  1, 5,  1, 6,  1,
                    58, 0,  19, 0,  1,  53, 1,  78, 2, 13, 1, 1,  62};
    run_test<bv::simple_bv<8, 256, 8>, dyn::suc_bv>(a, 52);
}

TEST(RunSdsl, A) {
    uint64_t a[] = {26198, 1, 8440};
    run_sdsl_test<bv::bv>(a, 3);
}

typedef bv::simple_bv<16, 16384, 64, true, true, true> run_len_bv;

TEST(RunRle, A) {
    uint64_t a[] = {9585};
    run_test<run_len_bv, bv::bv>(a, 1, 1);
}

TEST(RunRle, B) {
    uint64_t a[] = {
        8718, 2,    5318, 0,    0,    7604, 0,    2,    7026, 0,    1,    8152,
        2,    7216, 0,    2,    6821, 0,    0,    5820, 1,    0,    7773, 0,
        1,    7272, 0,    7439, 0,    0,    2802, 0,    2,    6857, 0,    1,
        1330, 2,    3683, 0,    0,    5150, 0,    2,    466,  0,    2,    7529,
        0,    1,    689,  1,    120,  2,    1823, 0,    1,    6937, 1,    1889,
        1,    5818, 1,    7356, 1,    7652, 0,    8122, 0,    1,    7630, 2,
        6821, 0,    1,    8004, 0,    1395, 0,    2,    1669, 0,    0,    1080,
        0,    0,    4176, 0,    1,    3352, 2,    4156, 0,    0,    8306, 0,
        1,    329,  1,    5436, 1,    7487, 0,    3793, 0,    2,    4161, 0,
        2,    2957, 0,    2,    324,  0,    1,    2545, 1,    6531, 0,    3862,
        0,    1,    2998, 2,    7888, 0,    1,    8363, 0,    1337, 0,    1,
        2831, 1,    1209, 1,    4861, 2,    267,  0,    0,    6099, 0,    2,
        3670, 0,    0,    5251, 0,    0,    7846, 0,    0,    8327, 0,    1,
        7581, 0,    8515, 0};
    run_test<run_len_bv, bv::bv>(a, 160, 1);
}

TEST(RunRle, C) {
    uint64_t a[] = {
        111, 2,   81, 0,   2,  105, 0,  2,  66,  0,   2,  90, 0,  1,  109, 0,
        89,  0,   0,  42,  0,  0,   72, 0,  2,   107, 0,  0,  69, 0,  2,   104,
        0,   0,   61, 0,   2,  31,  0,  2,  59,  0,   2,  10, 1,  0,  37,  0,
        0,   14,  0,  2,   85, 0,   0,  51, 0,   2,   82, 0,  2,  89, 0,   2,
        55,  0,   0,  91,  0,  1,   16, 2,  80,  0,   1,  34, 2,  87, 0,   0,
        30,  0,   2,  104, 0,  0,   86, 0,  1,   41,  1,  19, 1,  84, 2,   58,
        0,   2,   81, 0,   0,  55,  0,  2,  113, 0,   0,  11, 0,  0,  25,  0,
        2,   103, 0,  1,   69, 1,   52, 2,  29,  0,   0,  11, 1,  1,  27,  2,
        93,  0,   1,  92,  2,  92,  0,  1,  53,  2,   36, 0,  2,  85, 0,   0,
        4,   0,   1,  114, 2,  26,  0,  1,  66,  0,   93, 0,  0,  5,  0,   2,
        83,  0,   2,  106, 0,  1,   15, 0,  89,  0,   0,  13, 0};
    run_test<run_len_bv, bv::bv>(a, 173, 1);
}

TEST(RunRle, D) {
    uint64_t a[] = {10000, 0,  1,  1, 0,  2,  0,  0,  3,  1,  0,  4, 0,  0,
                    5,      1,  0,  6, 0,  0,  7,  1,  0,  8,  0,  0, 9,  1,
                    0,      10, 0,  0, 11, 1,  0,  12, 0,  0,  13, 1, 0,  14,
                    0,      0,  15, 1, 0,  0,  1,  0,  2,  0,  0,  4, 1,  0,
                    6,      0,  0,  8, 1,  0,  10, 0,  0,  12, 1,  0, 14, 0,
                    0,      16, 1,  0, 18, 0,  0,  20, 1,  0,  22, 0, 0,  24,
                    1,      0,  26, 0, 0,  28, 1,  0,  30, 0};
    run_test<run_len_bv, bv::bv>(a, 94, 1);
}
#endif

#endif