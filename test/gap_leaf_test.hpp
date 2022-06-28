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
void gap_leaf_insert_test() {
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
    ASSERT_EQ(l->need_realloc(), true);
    ASSERT_GT(l->desired_capacity(), leaf::init_capacity());
    for (uint32_t i = 0; i < leaf::BLOCK_SIZE; i++) {
        ASSERT_EQ(l->at(i), bool(i % 2));
    }
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

#endif