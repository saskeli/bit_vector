#ifndef TEST_GAP_LEAF_HPP
#define TEST_GAP_LEAF_HPP

#include <cstdint>
#include <iostream>

#include "../deps/googletest/googletest/include/gtest/gtest.h"

template <class leaf, class alloc>
void gap_leaf_init_test() {
    alloc* allocator = new alloc();
    leaf* l = allocator->template allocate_leaf<leaf>();
    ASSERT_EQ(l->p_sum(), 0ul);
    ASSERT_EQ(l->size(), 0ul);
    ASSERT_EQ(l->need_realloc(), false);
    ASSERT_EQ(l->capacity(), leaf::init_capacity());
    ASSERT_EQ(l->capacity() * leaf::WORD_BITS, leaf::BLOCK_SIZE);
    ASSERT_EQ(l->desired_capacity(), leaf::init_capacity());
}

template <class leaf, class alloc>
void gap_leaf_append_test() {
    alloc* allocator = new alloc();
    leaf* l = allocator->template allocate_leaf<leaf>();
    ASSERT_EQ(l->p_sum(), 0ul);
    ASSERT_EQ(l->size(), 0ul);
    ASSERT_EQ(l->need_realloc(), false);
    ASSERT_EQ(l->desired_capacity(), leaf::init_capacity());
    for (uint32_t i = 0; i < leaf::BLOCK_SIZE; i++) {
        l->insert(i, i % 2);
    }
    ASSERT_EQ(l->p_sum(), leaf::BLOCK_SIZE / 2);
    ASSERT_EQ(l->size(), leaf::BLOCK_SIZE);
    for (uint32_t i = 0; i < leaf::BLOCK_SIZE; i++) {
        ASSERT_EQ(l->at(i), bool(i % 2));
    }
    ASSERT_EQ(l->need_realloc(), true);
    ASSERT_GT(l->desired_capacity(), leaf::init_capacity());
}

template <class leaf, class alloc>
void gap_leaf_append_multiblock_test(uint32_t elems) {
    alloc* allocator = new alloc();
    leaf* l = allocator->template allocate_leaf<leaf>();
    ASSERT_EQ(l->p_sum(), 0ul);
    ASSERT_EQ(l->size(), 0ul);
    ASSERT_EQ(l->need_realloc(), false);
    ASSERT_EQ(l->desired_capacity(), leaf::init_capacity());
    for (uint32_t i = 0; i < elems; i++) {
        if (l->need_realloc()) {
            l = allocator->template reallocate_leaf<leaf>(l, l->capacity(), l->desired_capacity());
        }
        l->insert(i, i % 2);
    }
    ASSERT_EQ(l->p_sum(), elems / 2);
    ASSERT_EQ(l->size(), elems);
    for (uint32_t i = 0; i < elems; i++) {
        ASSERT_EQ(l->at(i), bool(i % 2)) << "i = " << i;
    }
    ASSERT_GE(l->capacity(), elems / leaf::WORD_BITS + (elems % leaf::WORD_BITS ? 1 : 0));
}

template <class leaf, class alloc>
void gap_leaf_prepend_test() {
    alloc* allocator = new alloc();
    leaf* l = allocator->template allocate_leaf<leaf>();
    ASSERT_EQ(l->p_sum(), 0ul);
    ASSERT_EQ(l->size(), 0ul);
    ASSERT_EQ(l->need_realloc(), false);
    ASSERT_EQ(l->desired_capacity(), leaf::init_capacity());
    for (uint32_t i = 0; i < leaf::BLOCK_SIZE; i++) {
        l->insert(0, i % 2);
    }
    ASSERT_EQ(l->p_sum(), leaf::BLOCK_SIZE / 2);
    ASSERT_EQ(l->size(), leaf::BLOCK_SIZE);
    for (uint32_t i = 0; i < leaf::BLOCK_SIZE; i++) {
        ASSERT_EQ(l->at(i), bool((leaf::BLOCK_SIZE - 1 - i) % 2));
    }
    ASSERT_EQ(l->need_realloc(), true);
    ASSERT_GT(l->desired_capacity(), leaf::init_capacity());
}

template <class leaf, class alloc>
void gap_leaf_prepend_multiblock_test(uint32_t elems) {
    alloc* allocator = new alloc();
    leaf* l = allocator->template allocate_leaf<leaf>();
    ASSERT_EQ(l->p_sum(), 0ul);
    ASSERT_EQ(l->size(), 0ul);
    ASSERT_EQ(l->need_realloc(), false);
    ASSERT_EQ(l->desired_capacity(), leaf::init_capacity());
    for (uint32_t i = 0; i < elems; i++) {
        if (l->need_realloc()) {
            l = allocator->template reallocate_leaf<leaf>(l, l->capacity(), l->desired_capacity());
        }
        l->insert(0, i % 2);
    }
    ASSERT_EQ(l->p_sum(), elems / 2);
    ASSERT_EQ(l->size(), elems);
    for (uint32_t i = 0; i < elems; i++) {
        ASSERT_EQ(l->at(i), bool((elems - 1 - i) % 2)) << "i = " << i;
    }
    ASSERT_GE(l->capacity(), elems / leaf::WORD_BITS + (elems % leaf::WORD_BITS ? 1 : 0));
}

template <class leaf, class alloc>
void gap_leaf_insert_test() {
    alloc* allocator = new alloc();
    leaf* l = allocator->template allocate_leaf<leaf>();
    ASSERT_EQ(l->p_sum(), 0ul);
    ASSERT_EQ(l->size(), 0ul);
    ASSERT_EQ(l->need_realloc(), false);
    ASSERT_EQ(l->desired_capacity(), leaf::init_capacity());
    for (uint32_t i = 0; i < leaf::BLOCK_SIZE; i++) {
        l->insert(i / 2, i % 2);
    }
    ASSERT_EQ(l->p_sum(), leaf::BLOCK_SIZE / 2);
    ASSERT_EQ(l->size(), leaf::BLOCK_SIZE);
    for (uint32_t i = 0; i < leaf::BLOCK_SIZE; i++) {
        ASSERT_EQ(l->at(i), i < leaf::BLOCK_SIZE / 2);
    }
    ASSERT_EQ(l->need_realloc(), true);
    ASSERT_GT(l->desired_capacity(), leaf::init_capacity());
}

template <class leaf, class alloc>
void gap_leaf_insert_multiblock_test(uint32_t elems) {
    alloc* allocator = new alloc();
    leaf* l = allocator->template allocate_leaf<leaf>();
    ASSERT_EQ(l->p_sum(), 0ul);
    ASSERT_EQ(l->size(), 0ul);
    ASSERT_EQ(l->need_realloc(), false);
    ASSERT_EQ(l->desired_capacity(), leaf::init_capacity());
    for (uint32_t i = 0; i < elems; i++) {
        if (l->need_realloc()) {
            l = allocator->template reallocate_leaf<leaf>(l, l->capacity(), l->desired_capacity());
        }
        l->insert(i / 2, i % 2);
    }
    ASSERT_EQ(l->p_sum(), elems / 2);
    ASSERT_EQ(l->size(), elems);
    for (uint32_t i = 0; i < elems; i++) {
        ASSERT_EQ(l->at(i), i < elems / 2) << "i = " << i;
    }
    ASSERT_GE(l->capacity(), elems / leaf::WORD_BITS + (elems % leaf::WORD_BITS ? 1 : 0));
}

template <class leaf, class alloc>
void gap_leaf_insert_offset_test(uint32_t elems, uint32_t offset) {
    alloc* allocator = new alloc();
    leaf* l = allocator->template allocate_leaf<leaf>();
    ASSERT_EQ(l->p_sum(), 0ul);
    ASSERT_EQ(l->size(), 0ul);
    ASSERT_EQ(l->need_realloc(), false);
    ASSERT_EQ(l->desired_capacity(), leaf::init_capacity());
    for (uint32_t i = 0; i < offset * 2; i++) {
        if (l->need_realloc()) {
            l = allocator->template reallocate_leaf<leaf>(l, l->capacity(), l->desired_capacity());
        }
        l->insert(i, i % 2);
    }
    for (uint32_t i = offset * 2; i < elems; i++) {
        if (l->need_realloc()) {
            l = allocator->template reallocate_leaf<leaf>(l, l->capacity(), l->desired_capacity());
        }
        l->insert(offset, false);
    }
    ASSERT_EQ(l->p_sum(), (offset / 2) * 2);
    ASSERT_EQ(l->size(), elems);
    for (uint32_t i = 0; i < offset; i++) {
        ASSERT_EQ(l->at(i), bool(i % 2)) << "i = " << i;
    }
    for (uint32_t i = offset; i < elems - offset; i++) {
        ASSERT_EQ(l->at(i), false);
    }
    for (uint32_t i = 0; i < offset; i++) {
        ASSERT_EQ(l->at(i + elems - offset), bool(i % 2));
    }
    ASSERT_GE(l->capacity(), elems / leaf::WORD_BITS + (elems % leaf::WORD_BITS ? 1 : 0));
}

/*template <class leaf, class alloc>
void gap_leaf_remove_test(uint64_t n) {
}

template <class leaf, class alloc>
void gap_leaf_rank_test(uint64_t n) {
}

template <class leaf, class alloc>
void gap_leaf_rank_offset_test(uint64_t n) {
}

template <class leaf, class alloc>
void gap_leaf_select_test(uint64_t n) {
}

template <class leaf, class alloc>
void gap_leaf_select_offset_test(uint64_t n) {
}

template <class leaf, class alloc>
void gap_leaf_set_test(uint64_t n) {
}

template <class leaf, class alloc>
void gap_leaf_clear_start_test() {
}

template <class leaf, class alloc>
void gap_leaf_transfer_append_test() {
}

template <class leaf, class alloc>
void gap_leaf_clear_end_test() {
}

template <class leaf, class alloc>
void gap_leaf_transfer_prepend_test() {
}

template <class leaf, class alloc>
void gap_leaf_append_all_test() {
}

template <class leaf, class alloc>
void gap_leaf_hit_buffer_test() {
}

template <class leaf, class alloc>
void gap_leaf_commit_test(uint64_t size) {
}*/

TEST(GapLeaf, Init) {
    gap_leaf_init_test<g_leaf, ma>();
}

TEST(GapLeaf, Append) {
    gap_leaf_append_test<g_leaf, ma>();
}
TEST(GapLeaf, AppendMultiblock) {
    gap_leaf_append_multiblock_test<g_leaf, ma>(10000);
}

TEST(GapLeaf, Prepend) {
    gap_leaf_prepend_test<g_leaf, ma>();
}

TEST(GapLeaf, PrependMultiblock) {
    gap_leaf_prepend_multiblock_test<g_leaf, ma>(10000);
}

TEST(GapLeaf, Insert) {
    gap_leaf_insert_test<g_leaf, ma>();
}

TEST(GapLeaf, InsertMultiblock) {
    gap_leaf_insert_multiblock_test<g_leaf, ma>(10000);
}

TEST(GapLead, InsertOffset) {
    gap_leaf_insert_offset_test<g_leaf, ma>(10000, 2000);
}

#endif