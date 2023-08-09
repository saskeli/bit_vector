#pragma once

#include <cstdint>

#include "../deps/googletest/googletest/include/gtest/gtest.h"

template <class buf>
void one_insert(buf& b) {
    b.insert(1337, true);
    ASSERT_EQ(b[0].index(), 1337u);
    ASSERT_TRUE(b[0].value());
    ASSERT_TRUE(b[0].is_insertion());
    ASSERT_EQ(b.size(), 1);
}

template <class buf>
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
        ASSERT_TRUE(b[i].is_insertion());
    }
    uint16_t i = 0;
    for (auto be : b) {
        ASSERT_EQ(be.index(), i * step);
        ASSERT_EQ(be.value(), i % 2);
        ASSERT_TRUE(be.is_insertion());
        ++i;
    }
}

template <bool sort, class buf>
void four_with_reorder(buf& b) {
    ASSERT_GE(buf::max_elems(), 4);
    b.insert(4, true);
    b.insert(1, false);
    b.insert(5, false);
    b.insert(3, true);
    if constexpr (sort) {
        b.sort();
    }
    uint32_t locs[] = {1, 3, 6, 7};
    for (uint16_t i = 0; i < 4; i++) {
        ASSERT_EQ(b[i].index(), locs[i]);
        ASSERT_EQ(b[i].value(), bool(i % 2));
        ASSERT_TRUE(b[i].is_insertion());
    }
    uint16_t i = 0;
    for (auto be : b) {
        ASSERT_EQ(be.index(), locs[i]);
        ASSERT_EQ(be.value(), bool(i % 2));
        ASSERT_TRUE(be.is_insertion());
        ++i;
    }
}

template <bool sort, class buf>
void non_trivial_remove(buf& b) {
    ASSERT_GE(buf::max_elems(), 4);
    b.insert(4, true);
    b.insert(1, false);
    b.insert(5, false);
    b.insert(3, true);
    bool v = true;
    uint32_t idx = 6;
    ASSERT_TRUE(b.remove(idx, v));
    ASSERT_FALSE(v);
    if constexpr (sort) {
        b.sort();
    }
    std::pair<uint32_t, bool> elems[] = {{1, false}, {3, true}, {6, true}};
    uint16_t i = 0;
    for (auto be : b) {
        ASSERT_EQ(be.index(), elems[i].first);
        ASSERT_EQ(be.value(), elems[i].second);
        ASSERT_TRUE(be.is_insertion());
        ++i;
    }
    ASSERT_EQ(b.size(), 3u);
}

template <class buf>
void deferred_removal(buf& b) {
    ASSERT_GE(buf::max_elems(), 4);
    b.insert(4, true);
    b.insert(1, false);
    b.insert(5, false);
    b.insert(3, true);
    bool v = true;
    uint32_t idx = 5;
    ASSERT_FALSE(b.remove(idx, v));
    ASSERT_EQ(idx, 3u);
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

TEST(BufferCompSorted, UnorderedInsert) {
    buffer<16, true, true> b;
    four_with_reorder<false>(b);
}

TEST(BufferCompSorted, TrivialRemove) {
    buffer<16, true, true> b;
    b.insert(4, true);
    bool v = false;
    uint32_t idx = 4;
    ASSERT_TRUE(b.remove(idx, v));
    ASSERT_TRUE(v);
    ASSERT_EQ(b.size(), 0u);
}

TEST(BufferCompSorted, NonTrivialRemove) {
    buffer<16, true, true> b;
    non_trivial_remove<false>(b);
}

TEST(BufferCompSorted, DeferredRemove) {
    buffer<16, true, true> b;
    deferred_removal(b);
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

TEST(BufferCompUnsorted, UnorderedInsert) {
    buffer<16, true, false> b;
    four_with_reorder<true>(b);
}

TEST(BufferCompUnsorted, TrivialRemove) {
    buffer<16, true, false> b;
    b.insert(4, true);
    bool v = false;
    uint32_t idx = 4;
    ASSERT_TRUE(b.remove(idx, v));
    ASSERT_TRUE(v);
    ASSERT_EQ(b.size(), 0u);
}

TEST(BufferCompUnsorted, NonTrivialRemove) {
    buffer<16, true, false> b;
    non_trivial_remove<true>(b);
}

TEST(BufferCompUnsorted, DeferredRemove) {
    buffer<16, true, false> b;
    deferred_removal(b);
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

TEST(BufferSorted, UnorderedInsert) {
    buffer<16, false, true> b;
    four_with_reorder<false>(b);
}

TEST(BufferSorted, TrivialRemove) {
    buffer<16, false, true> b;
    b.insert(4, true);
    bool v = false;
    uint32_t idx = 4;
    ASSERT_TRUE(b.remove(idx, v));
    ASSERT_TRUE(v);
    ASSERT_EQ(b.size(), 0u);
}

TEST(BufferSorted, NonTrivialRemove) {
    buffer<16, false, true> b;
    non_trivial_remove<false>(b);
}

TEST(BufferSorted, DeferredRemove) {
    buffer<16, false, true> b;
    deferred_removal(b);
}

TEST(BufferSorted, MixedOrder) {
    buffer<16, false, true> b;
    b.insert(1337, true);
    bool v = false;
    uint32_t idx = 1337;
    ASSERT_TRUE(b.remove(idx, v));
    ASSERT_TRUE(v);
    ASSERT_EQ(b.size(), 0u);
    ASSERT_FALSE(b.remove(idx, v));
    ASSERT_EQ(idx, 1337u);
    ASSERT_EQ(b.size(), 1u);
    ASSERT_FALSE(b.remove(idx, v));
    ASSERT_EQ(idx, 1338u);
    ASSERT_EQ(b.size(), 2u);
    b.insert(1337, true);
    b.insert(1337, false);
    std::pair<uint32_t, std::pair<bool, bool>> elems[] = {
        {1337, {false, false}},
        {1337, {false, false}},
        {1337, {true, false}},
        {1338, {true, true}}};
    uint16_t i = 0;
    for (auto be : b) {
        ASSERT_EQ(be.index(), elems[i].first) << "i = " << i;
        ASSERT_EQ(be.value(), elems[i].second.second) << "i = " << i;
        ASSERT_EQ(be.is_insertion(), elems[i].second.first) << "i = " << i;
        ++i;
    }
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

TEST(BufferUnsorted, UnorderedInsert) {
    buffer<16, false, false> b;
    four_with_reorder<true>(b);
}

TEST(BufferUnsorted, TrivialRemove) {
    buffer<16, false, false> b;
    b.insert(4, true);
    bool v = false;
    uint32_t idx = 4;
    ASSERT_TRUE(b.remove(idx, v));
    ASSERT_TRUE(v);
    ASSERT_EQ(b.size(), 0u);
}

TEST(BufferUnorted, NonTrivialRemove) {
    buffer<16, false, false> b;
    non_trivial_remove<true>(b);
}

TEST(BufferUnsorted, DeferredRemove) {
    buffer<16, false, false> b;
    deferred_removal(b);
}