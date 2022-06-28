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

#endif