#ifndef BV_PACKED_ARRAY_TEST_HPP
#define BV_PACKED_ARRAY_TEST_HPP

#include <cstdint>

#include "../deps/googletest/googletest/include/gtest/gtest.h"
#include "../bit_vector/internal/packed_array.hpp"

template<uint16_t elems, uint16_t width>
void packed_array_init_test() {
    bv::packed_array<elems, width> arr;
    for (uint32_t i = 0; i < elems; i++) {
        ASSERT_EQ(arr[i], 0);
        ASSERT_EQ(arr.at(i), 0);
    }
}

template<uint16_t elems, uint16_t width>
void packed_array_single_set_test() {
    bv::packed_array<elems, width> arr;
    constexpr uint16_t max_val = (uint16_t(1) << width) - 1;
    arr.set(elems / 2, max_val);
    for (uint32_t i = 0; i < elems; i++) {
        ASSERT_EQ(arr[i], i == elems / 2 ? max_val : 0);
        ASSERT_EQ(arr.at(i), i == elems / 2 ? max_val : 0);
    }
}

template<uint16_t elems, uint16_t width>
void packed_array_set_test() {
    bv::packed_array<elems, width> arr;
    constexpr uint16_t max_val = (uint16_t(1) << width) - 1;
    for (uint32_t i = 0; i < elems; i++) {
        arr.set(i, i % max_val);
    }
    for (uint32_t i = 0; i < elems; i++) {
        ASSERT_EQ(arr[i], i % max_val);
        ASSERT_EQ(arr.at(i), i % max_val);
    }
}

template<uint16_t elems, uint16_t width>
void packed_array_operator_set_test() {
    bv::packed_array<elems, width> arr;
    constexpr uint16_t max_val = (uint16_t(1) << width) - 1;
    for (uint32_t i = 0; i < elems; i++) {
        arr[i] = i % max_val;
    }
    for (uint32_t i = 0; i < elems; i++) {
        ASSERT_EQ(arr[i], i % max_val);
        ASSERT_EQ(arr.at(i), i % max_val);
    }
}

template<uint16_t elems, uint16_t width>
void packed_array_clear_test() {
    bv::packed_array<elems, width> arr;
    constexpr uint16_t max_val = (uint16_t(1) << width) - 1;
    for (uint32_t i = 0; i < elems; i++) {
        arr[i] = i % max_val;
    }
    arr.clear();
    for (uint32_t i = 0; i < elems; i++) {
        ASSERT_EQ(arr[i], 0);
    }
}

template<uint16_t elems, uint16_t width>
void packed_array_set_ref_set_test() {
    bv::packed_array<elems, width> arr;
    constexpr uint16_t max_val = (uint16_t(1) << width) - 1;
    for (uint32_t i = 0; i < elems; i++) {
        arr[i] = i % max_val;
    }
    for (uint32_t i = 0; i < elems / 2; i++) {
        auto t = arr[i];
        arr[i] = arr[elems - 1 - i];
        arr[elems - 1 - i] = t;
    }
    for (uint32_t i = 0; i < elems; i++) {
        ASSERT_EQ(arr[elems - 1 - i], i % max_val) << "i = " << i;
    }
}

TEST(PackedArray, Init) {
    packed_array_init_test<32, 7>();
}

TEST(PackedArray, SingleSet) {
    packed_array_single_set_test<32, 5>();
}

TEST(PackedArray, SetClean) {
    packed_array_set_test<16, 8>();
}

TEST(PackedArray, Set) {
    packed_array_set_test<32, 7>();
}

TEST(PackedArray, OperatorSet) {
    packed_array_operator_set_test<128, 11>();
}

TEST(PackedArray, Clear) {
    packed_array_clear_test<32, 7>();
}

TEST(PackedArray, RefSet) {
    packed_array_set_ref_set_test<128, 11>();
}

#endif