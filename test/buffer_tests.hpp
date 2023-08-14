#pragma once

#include <cstdint>
#include <array>
#include <random>
#include <ranges>
#include <set>

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
    ASSERT_EQ(b.remove(idx, v), buf::max_elems());
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
    ASSERT_LT(b.remove(idx, v), buf::max_elems());
    ASSERT_EQ(idx, 3u);
}

template <class buf>
void trivial_set(buf& b) {
    b.insert(1, false);
    int diff;
    uint32_t idx = 1;
    ASSERT_TRUE(b.set(idx, false, diff));
    ASSERT_EQ(diff, 0);
    idx = 1;
    ASSERT_TRUE(b.set(idx, true, diff));
    ASSERT_EQ(diff, 1);
    idx = 1;
    ASSERT_TRUE(b.set(idx, false, diff));
    ASSERT_EQ(diff, -1);
    idx = 1;
    ASSERT_TRUE(b.set(idx, false, diff));
    ASSERT_EQ(diff, 0);
}

template <class buf>
void non_set(buf& b) {
    b.insert(4, true);
    b.insert(2, false);
    b.insert(6, true);
    int diff;
    uint32_t idx = 4;
    ASSERT_FALSE(b.set(idx, false, diff));
    ASSERT_EQ(idx, 3u);
    ASSERT_EQ(b.size(), 3u);
}

template <class buf>
void set(buf& b) {
    b.insert(4, true);
    b.insert(2, false);
    b.insert(6, true);
    int diff;
    uint32_t idx = 5;
    ASSERT_TRUE(b.set(idx, false, diff));
    ASSERT_EQ(diff, -1);
    ASSERT_EQ(b.size(), 3u);
}

template <uint16_t b_size, uint32_t max_val> 
void big_sort() {
    static_assert(max_val >= b_size);
    buffer<b_size, false, false> b;
    const constexpr uint32_t step = max_val / b_size;
    std::array<std::pair<uint32_t, bool>, b_size> a;
    for (uint32_t i = 0; i < b_size; i++) {
        a[i] = {i * step, i % 2};
    }
    std::mt19937 gen {1337};
    std::ranges::shuffle(a, gen);
    std::set<uint32_t> adds;
    std::array<uint16_t, b_size> offsets;
    for (uint16_t i = b_size - 1; i < b_size; --i) {
        offsets[i] = std::distance(adds.begin(), adds.lower_bound(a[i].first));
        adds.insert(a[i].first);
    }
    for (uint16_t i = 0; i < b_size; i++) {
        b.insert(a[i].first - offsets[i], a[i].second);
    }
    b.sort();
    ASSERT_EQ(b.size(), b_size);
    for (uint16_t i = 0; i < b_size; i++) {
        ASSERT_EQ(b[i].index(), i * step) << "i=" << i;
        ASSERT_EQ(b[i].value(), bool(i % 2)) << "i=" << i;
        ASSERT_TRUE(b[i].is_insertion()) << "i=" << i;
    }
}

template<class buf>
void trivial_access(buf& b) {
    b.insert(7, true);
    uint32_t idx = 7;
    bool v = false;
    ASSERT_TRUE(b.access(idx, v));
    ASSERT_TRUE(v);
}

template<class buf>
void deferred_access(buf& b) {
    b.insert(4, true);
    b.insert(1, true);
    b.insert(3, false);
    b.insert(8, false);
    uint32_t idx = 4;
    bool v = false;
    ASSERT_FALSE(b.access(idx, v));
    ASSERT_EQ(idx, 2u);
}

template<class buf>
void non_trivial_access(buf& b) {
    b.insert(4, true);
    b.insert(1, true);
    b.insert(3, false);
    b.insert(8, false);
    uint32_t idx = 6;
    bool v = false;
    ASSERT_TRUE(b.access(idx, v));
    ASSERT_TRUE(v);
}

template<class buf>
void trivial_rank_query(buf& b) {
    uint32_t idx = 1337;
    ASSERT_EQ(b.rank(idx), 0u);
    ASSERT_EQ(idx, 1337u);
}

template<class buf>
void non_trivial_rank_query(buf& b) {
    b.insert(1, false);
    b.insert(2, true);
    b.insert(1337, true);
    uint32_t idx = 2;
    ASSERT_EQ(b.rank(idx), 0u);
    ASSERT_EQ(idx, 1u);
    idx = 20;
    ASSERT_EQ(b.rank(idx), 1u);
    ASSERT_EQ(idx, 18u);
    idx = 2000;
    ASSERT_EQ(b.rank(idx), 2u);
    ASSERT_EQ(idx, 1997u);
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
    ASSERT_LT(b.remove(idx, v), 16u);
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
    ASSERT_EQ(b.remove(idx, v), 16u);
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

TEST(BufferCompSorted, TrivialSet) {
    buffer<16, true, true> b;
    trivial_set(b);
}

TEST(BufferCompSorted, NonSet) {
    buffer<16, true, true> b;
    non_set(b);
}

TEST(BufferCompSorted, Set) {
    buffer<16, true, true> b;
    set(b);
}

TEST(BufferCompSorted, TrivialAccess) {
    buffer<16, true, true> b;
    trivial_access(b);
}

TEST(BufferCompSorted, DeferredAccess) {
    buffer<16, true, true> b;
    deferred_access(b);
}

TEST(BufferCompSorted, NonTrivialAccess) {
    buffer<16, true, true> b;
    non_trivial_access(b);
}

TEST(BufferCompSorted, TrivialRankQuery) {
    buffer<16, true, true> b;
    trivial_rank_query(b);
}

TEST(BufferCompSorted, RankQuery) {
    buffer<16, true, true> b;
    non_trivial_rank_query(b);
}

TEST(BufferCompSorted, ClearFirst) {
    buffer<16, true, true> b;
    b.insert(4, false);
    b.insert(6, true);
    b.insert(14, false);
    b.insert(24, true);
    uint32_t elems = 10;
    ASSERT_EQ(b.clear_first(elems), 1u);
    ASSERT_EQ(elems, 8u);
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
    ASSERT_LT(b.remove(idx, v), 16u);
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
    ASSERT_EQ(b.remove(idx, v), 16u);
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

TEST(BufferCompUnsorted, TrivialSet) {
    buffer<16, true, false> b;
    trivial_set(b);
}

TEST(BufferCompUnsorted, NonSet) {
    buffer<16, true, false> b;
    non_set(b);
}

TEST(BufferCompUnsorted, Set) {
    buffer<16, true, false> b;
    set(b);
}

TEST(BufferCompUnsorted, TrivialAccess) {
    buffer<16, true, false> b;
    trivial_access(b);
}

TEST(BufferCompUnsorted, DeferredAccess) {
    buffer<16, true, false> b;
    deferred_access(b);
}

TEST(BufferCompUnsorted, NonTrivialAccess) {
    buffer<16, true, false> b;
    non_trivial_access(b);
}

TEST(BufferCompUnsorted, TrivialRankQuery) {
    buffer<16, true, false> b;
    trivial_rank_query(b);
}

TEST(BufferCompUnsorted, RankQuery) {
    buffer<16, true, false> b;
    non_trivial_rank_query(b);
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
    ASSERT_LT(b.remove(idx, v), 16u);
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
    ASSERT_EQ(b.remove(idx, v), 16u);
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

TEST(BufferSorted, RemoveBug1) {
    buffer<16, false, true> b;
    b.insert(7, true);
    b.insert(1, false);
    uint32_t idx = 2;
    bool v = true;
    uint32_t cb_idx = b.remove(idx, v);
    ASSERT_LT(cb_idx, 16u);
    ASSERT_EQ(idx, 1u);
    b.set_remove_value(cb_idx, true);
    std::pair<uint32_t, std::pair<bool, bool>> elems[] = {
        {1, {true, false}},
        {2, {false, true}},
        {7, {true, true}}};
    uint16_t i = 0;
    for (auto be : b) {
        ASSERT_EQ(be.index(), elems[i].first) << "i = " << i;
        ASSERT_EQ(be.is_insertion(), elems[i].second.first) << "i = " << i;
        if (be.is_insertion()) {
            ASSERT_EQ(be.value(), elems[i].second.second) << "i = " << i;
        }
        ++i;
    }
}

TEST(BufferSorted, MixedOrder) {
    buffer<16, false, true> b;
    b.insert(1337, true);
    bool v = false;
    uint32_t idx = 1337;
    ASSERT_EQ(b.remove(idx, v), 16u);
    ASSERT_TRUE(v);
    ASSERT_EQ(b.size(), 0u);
    uint32_t cb_idx = b.remove(idx, v);
    ASSERT_LT(cb_idx, 16u);
    b.set_remove_value(cb_idx, true);
    ASSERT_EQ(idx, 1337u);
    ASSERT_EQ(b.size(), 1u);
    cb_idx = b.remove(idx, v);
    ASSERT_LT(cb_idx, 16u);
    b.set_remove_value(cb_idx, false);
    ASSERT_EQ(idx, 1338u);
    ASSERT_EQ(b.size(), 2u);
    b.insert(1337, true);
    b.insert(1337, false);
    std::pair<uint32_t, std::pair<bool, bool>> elems[] = {
        {1337, {false, true}},
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

TEST(BufferSorted, TrivialSet) {
    buffer<16, false, true> b;
    trivial_set(b);
}

TEST(BufferSorted, NonSet) {
    buffer<16, false, true> b;
    non_set(b);
}

TEST(BufferSorted, Set) {
    buffer<16, false, true> b;
    set(b);
}

TEST(BufferSorted, MixedSet) {
    buffer<16, false, true> b;
    b.insert(7, true);
    b.insert(1, false);
    uint32_t idx = 2;
    bool v = true;
    b.remove(idx, v);
    idx = 3;
    b.remove(idx, v);
    int diff;
    idx = 7;
    ASSERT_FALSE(b.set(idx, true, diff));
    ASSERT_EQ(idx, 7u);
    ASSERT_EQ(b.size(), 4u);
    idx = 6;
    ASSERT_TRUE(b.set(idx, false, diff));
    ASSERT_EQ(diff, -1);
    ASSERT_EQ(b.size(), 4u);
    idx = 3;
    ASSERT_FALSE(b.set(idx, false, diff));
    ASSERT_EQ(idx, 4u);
    ASSERT_EQ(b.size(), 4u);
}

TEST(BufferSorted, TrivialAccess) {
    buffer<16, false, true> b;
    trivial_access(b);
}

TEST(BufferSorted, DeferredAccess) {
    buffer<16, false, true> b;
    deferred_access(b);
}

TEST(BufferSorted, NonTrivialAccess) {
    buffer<16, false, true> b;
    non_trivial_access(b);
}

TEST(BufferSorted, MixedAccess) {
    buffer<16, false, true> b;
    b.insert(7, true);
    b.insert(1, false);
    uint32_t idx = 2;
    bool v = true;
    b.remove(idx, v);
    idx = 3;
    b.remove(idx, v);
    idx = 6;
    ASSERT_TRUE(b.access(idx, v));
    ASSERT_TRUE(v);
    idx = 5;
    ASSERT_FALSE(b.access(idx, v));
    ASSERT_EQ(idx, 6u);
    idx = 3;
    ASSERT_FALSE(b.access(idx, v));
    ASSERT_EQ(idx, 4u);
}

TEST(BufferSorted, TrivialRankQuery) {
    buffer<16, false, true> b;
    trivial_rank_query(b);
}

TEST(BufferSorted, RankQuery) {
    buffer<16, false, true> b;
    non_trivial_rank_query(b);
}

TEST(BuferSorted, MixedRankQuery) {
    buffer<16, false, true> b;
    uint32_t index = 3;
    bool v;
    uint32_t cb_idx = b.remove(index, v);
    b.set_remove_value(cb_idx, false);
    index = 10;
    cb_idx = b.remove(index, v);
    b.set_remove_value(cb_idx, true);
    b.insert(100, true);
    b.insert(1000, true);
    index = 5;
    cb_idx = 0;
    ASSERT_EQ(b.rank(index), cb_idx);
    ASSERT_EQ(index, 6u);
    index = 15;
    --cb_idx;
    ASSERT_EQ(b.rank(index), cb_idx);
    ASSERT_EQ(index, 17u);
    index = 150;
    ++cb_idx;
    ASSERT_EQ(b.rank(index), cb_idx);
    ASSERT_EQ(index, 151u);
    index = 1500;
    ++cb_idx;
    ASSERT_EQ(b.rank(index), cb_idx);
    ASSERT_EQ(index, 1500u);
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
    ASSERT_LT(b.remove(idx, v), 16u);
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
    ASSERT_EQ(b.remove(idx, v), 16u);
    ASSERT_TRUE(v);
    ASSERT_EQ(b.size(), 0u);
}

TEST(BufferUnsorted, NonTrivialRemove) {
    buffer<16, false, false> b;
    non_trivial_remove<true>(b);
}

TEST(BufferUnsorted, DeferredRemove) {
    buffer<16, false, false> b;
    deferred_removal(b);
}

TEST(BufferUnsorted, TrivialSet) {
    buffer<16, false, false> b;
    trivial_set(b);
}


TEST(BufferUnsorted, NonSet) {
    buffer<16, false, false> b;
    non_set(b);
}

TEST(BufferUnsorted, Set) {
    buffer<16, false, false> b;
    set(b);
}

TEST(BufferUnsorted, Sort16) {
    const constexpr uint16_t b_size = 16;
    big_sort<b_size, b_size * 2>();
}

TEST(BufferUnsorted, BigSort16) {
    const constexpr uint16_t b_size = 16;
    big_sort<b_size, (uint32_t(1) << 30) - 1>();
}

TEST(BufferUnsorted, Sort256) {
    const constexpr uint16_t b_size = 256;
    big_sort<b_size, b_size * 2>();
}

TEST(BufferUnsorted, BigSort256) {
    const constexpr uint16_t b_size = 256;
    big_sort<b_size, (uint32_t(1) << 30) - 1>();
}

TEST(BufferUnsorted, Sort512) {
    const constexpr uint16_t b_size = 512;
    big_sort<b_size, b_size * 2>();
}

TEST(BufferUnsorted, BigSort512) {
    const constexpr uint16_t b_size = 512;
    big_sort<b_size, (uint32_t(1) << 30) - 1>();
}

TEST(BufferUnsorted, TrivialAccess) {
    buffer<16, false, false> b;
    trivial_access(b);
}

TEST(BufferUnsorted, DeferredAccess) {
    buffer<16, false, false> b;
    deferred_access(b);
}

TEST(BufferUnsorted, NonTrivialAccess) {
    buffer<16, false, false> b;
    non_trivial_access(b);
}

TEST(BufferUnsorted, TrivialRankQuery) {
    buffer<16, false, false> b;
    trivial_rank_query(b);
}

TEST(BufferUnsorted, RankQuery) {
    buffer<16, false, false> b;
    non_trivial_rank_query(b);
}