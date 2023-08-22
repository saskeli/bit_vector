#pragma once

#include <cstdint>
#include <set>

#include "../deps/googletest/googletest/include/gtest/gtest.h"

TEST(CircularBuffer, Init) {
    uint64_t buf_s[4];
    Circular_Buffer<4> sc(buf_s);
    ASSERT_EQ(sc.space(), uint32_t(4 * 64));
}

TEST(CircularBuffer, SimplePushPoll) {
    uint64_t buf_s[4];
    Circular_Buffer<4> sc(buf_s);
    
    sc.push_back(1337, 64);
    ASSERT_EQ(sc.space(), uint32_t(3 * 64));
    ASSERT_EQ(sc.poll(), 1337u);
    ASSERT_EQ(sc.space(), uint32_t(4 * 64));
}

TEST(CircularBuffer, SimpleCompound) {
    uint64_t buf_s[4];
    Circular_Buffer<4> sc(buf_s);

    sc.push_back(0b00011001, 6);
    sc.push_back(0b1, 1);
    sc.push_back(0b010, 3);
    ASSERT_EQ(sc.space(), uint32_t(4 * 64 - 10));
    ASSERT_EQ(sc.poll(), 0b0101011001u);
}

TEST(CircularBuffer, CompoundWord) {
    uint64_t buf_s[4];
    Circular_Buffer<4> sc(buf_s);

    uint64_t zero = 0;
    uint64_t ones = ~zero;
    uint64_t one = 1;

    sc.push_back(ones, 30);
    sc.push_back(zero, 1);
    sc.push_back(ones, 64);

    ASSERT_EQ(sc.space(), uint32_t(4 * 64 - 95));
    ASSERT_EQ(sc.poll(), ones ^ (one << 30));
    ASSERT_EQ(sc.space(), uint32_t(4 * 64 - 31));
    ASSERT_EQ(sc.poll(), (one << 31) - 1);
}

TEST(CircularBuffer, WrapAround) {
    uint64_t buf_s[4];
    Circular_Buffer<4> sc(buf_s);

    for (uint16_t i = 0; i < 100; i++) {
        sc.push_back(0, 64);
        sc.push_back(~uint64_t(0), 64);
        ASSERT_EQ(sc.poll(), 0u);
        ASSERT_EQ(sc.poll(), ~uint64_t(0));
    }
}

TEST(CircularBuffer, Bug1) {
    uint64_t buf_s[4];
    Circular_Buffer<4> sc(buf_s);

    const constexpr uint64_t zeros = 0;
    const constexpr uint64_t ones = ~zeros;

    sc.push_back(zeros, 60);
    sc.push_back(zeros, 64);
    sc.push_back(zeros, 64);
    sc.push_back(zeros, 64);
    ASSERT_EQ(sc.poll(), zeros);
    sc.push_back(zeros, 64);
    ASSERT_EQ(sc.poll(), zeros);
    sc.push_back(zeros, 64);
    ASSERT_EQ(sc.poll(), zeros);
    sc.push_back(zeros, 64);
    ASSERT_EQ(sc.poll(), zeros);
    sc.push_back(ones << 4, 4);
    sc.push_back(ones >> 8, 56);
    ASSERT_EQ(sc.poll(), zeros);
    sc.push_back(ones, 64);
    ASSERT_EQ(sc.poll(), zeros);
    sc.push_back(ones, 64);
    ASSERT_EQ(sc.poll(), zeros);
    sc.push_back(ones, 64);
    ASSERT_EQ(sc.poll(), ones);
    sc.push_back(ones, 64);
    ASSERT_EQ(sc.poll(), ones);
    sc.push_back(ones, 64);
    ASSERT_EQ(sc.poll(), ones);
    sc.push_back(ones, 64);
    ASSERT_EQ(sc.poll(), ones);
    sc.push_back(ones >> 56, 64);
    ASSERT_EQ(sc.poll(), ones);
    sc.push_back(zeros, 64);
    ASSERT_EQ(sc.poll(), ones);
    ASSERT_EQ(sc.poll(), ones);
}