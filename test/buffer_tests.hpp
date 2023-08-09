#pragma once

#include <cstdint>

#include "../deps/googletest/googletest/include/gtest/gtest.h"

template<class buf>
void one_insert(buf& b) {
    b.insert(1337, true);
    ASSERT_EQ(b[0].index(), 1337u);
    ASSERT_TRUE(b[0].value());
    ASSERT_TRUE(b[0].is_insertion());
    ASSERT_EQ(b.size(), 1);
}

template<class buf>
void hundred_ordered_insertions(buf& b) {
    const uint32_t big = uint32_t(1) << 30;
    const uint32_t step = big / buf::max_elems();
    for (uint32_t i = 0; i < buf::max_elems(); ++i) {
        b.insert(i * step, i % 2);
    }
    ASSERT_EQ(b.size(), buf::max_elems());
    for (uint32_t i = 0; i < buf::max_elems(); ++i) {
        ASSERT_EQ(b[i].index(), i * step);
        ASSERT_EQ(b[i].value(), i % 2);
        ASSERT_EQ(b[i].is_insertion(), true);
    }
    uint16_t i = 0;
    for (auto be : b) {
        ASSERT_EQ(be.index(), i * step);
        ASSERT_EQ(be.value(), i % 2);
        ASSERT_EQ(be.is_insertion(), true);
        ++i;
    }
}

// Compressed and sorted tests
TEST(BufferCompSorted, Initialize) { 
    buffer<16, true, true> b;
    ASSERT_EQ(b.size(), 0);
}

TEST(BufferCompSorted, SingleInsert) { 
    buffer<16, true, true> b;
    one_insert(b);
}

TEST(BufferCompSorted, EmptyRemove) {
    buffer<16, true, true> b;
    uint32_t idx = 1337;
    bool v = true;
    ASSERT_FALSE(b.remove(idx, v));
    ASSERT_TRUE(v);
    ASSERT_EQ(idx, 1337u);
}

TEST(BufferCompSorted, OrderedInsert) {
    buffer<16, true, true> b;
    hundred_ordered_insertions(b);
}

// Compressed and unsorted tests
TEST(BufferCompUnsorted, Initialize) { 
    buffer<16, true, false> b;
    ASSERT_EQ(b.size(), 0);
}

TEST(BufferCompUnsorted, SingleInsert) { 
    buffer<16, true, false> b;
    one_insert(b);
}

TEST(BufferCompUnsorted, EmptyRemove) {
    buffer<16, true, false> b;
    uint32_t idx = 1337;
    bool v = true;
    ASSERT_FALSE(b.remove(idx, v));
    ASSERT_TRUE(v);
    ASSERT_EQ(idx, 1337u);
}

TEST(BufferCompUnsorted, OrderedInsert) {
    buffer<16, true, false> b;
    hundred_ordered_insertions(b);
}

// Uncompressed and sorted tests
TEST(BufferSorted, Initialize) { 
    buffer<16, false, true> b;
    ASSERT_EQ(b.size(), 0);
}

TEST(BufferSorted, SingleInsert) { 
    buffer<16, false, true> b;
    one_insert(b);
}

TEST(BufferSorted, EmptyRemove) {
    buffer<16, false, true> b;
    uint32_t idx = 1337;
    bool v = true;
    ASSERT_FALSE(b.remove(idx, v));
    ASSERT_TRUE(v);
    ASSERT_EQ(idx, 1337u);
}

TEST(BufferSorted, OrderedInsert) {
    buffer<16, false, true> b;
    hundred_ordered_insertions(b);
}

// Uncompressed and unsorted tests
TEST(BufferUnsorted, Initialize) { 
    buffer<16, false, false> b;
    ASSERT_EQ(b.size(), 0);
}

TEST(BufferUnsorted, SingleInsert) { 
    buffer<16, false, false> b;
    one_insert(b);
}

TEST(BufferUnsorted, EmptyRemove) {
    buffer<16, false, false> b;
    uint32_t idx = 1337;
    bool v = true;
    ASSERT_FALSE(b.remove(idx, v));
    ASSERT_TRUE(v);
    ASSERT_EQ(idx, 1337u);
}

TEST(BufferUnsorted, OrderedInsert) {
    buffer<16, false, false> b;
    hundred_ordered_insertions(b);
}