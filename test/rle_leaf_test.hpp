#ifndef TEST_RLE_LEAF_HPP
#define TEST_RLE_LEAF_HPP

#include <cstdint>
#include <iostream>

#include "../deps/googletest/googletest/include/gtest/gtest.h"

template<class rl_l, class alloc>
void rle_leaf_init_zeros(uint32_t size) {
    alloc* a = new alloc();
    rl_l* l = a->template allocate_leaf<rl_l>(8, size, false);
    EXPECT_EQ(l->size(), size);
    EXPECT_EQ(l->p_sum(), 0u);
    for (size_t i = 0; i < size; i++) {
        EXPECT_EQ(l->at(i), false);
        EXPECT_EQ(l->rank(i), 0u);
    }

    a->deallocate_leaf(l);
    delete a;
}

template<class rl_l, class alloc>
void rle_leaf_init_ones(uint32_t size) {
    alloc* a = new alloc();
    rl_l* l = a->template allocate_leaf<rl_l>(8, size, true);
    EXPECT_EQ(l->size(), size);
    EXPECT_EQ(l->p_sum(), size);
    for (size_t i = 0; i < size; i++) {
        EXPECT_EQ(l->at(i), true);
        EXPECT_EQ(l->rank(i), i);
        EXPECT_EQ(l->select(i + 1), i);
    }

    a->deallocate_leaf(l);
    delete a;
}

template<class rl_l, class alloc>
void rle_leaf_insert(uint32_t size, uint32_t i_count) {
    alloc* a = new alloc();
    rl_l* l = a->template allocate_leaf<rl_l>(32, size, false);
    EXPECT_EQ(l->size(), size);
    EXPECT_EQ(l->p_sum(), 0u);
    for (size_t i = 0; i < i_count; i++) {
        l->insert(0, true);
    }
    EXPECT_EQ(l->size(), size + i_count);
    EXPECT_EQ(l->p_sum(), i_count);
    for (size_t i = 0; i < i_count; i++) {
        EXPECT_EQ(l->at(i), true);
        EXPECT_EQ(l->rank(i), i);
        EXPECT_EQ(l->select(i + 1), i);
    }
    for (size_t i = i_count; i < size + i_count; i++) {
        EXPECT_EQ(l->at(i), false);
        EXPECT_EQ(l->rank(i), i_count);
    }

    a->deallocate_leaf(l);
    delete a;
}

template<class rl_l, class alloc>
void rle_leaf_insert_middle(uint32_t size, uint32_t i_count) {
    alloc* a = new alloc();
    rl_l* l = a->template allocate_leaf<rl_l>(32, size, false);
    EXPECT_EQ(l->size(), size);
    EXPECT_EQ(l->p_sum(), 0u);
    for (size_t i = 0; i < i_count; i++) {
        l->insert(size >> 1, true);
    }
    EXPECT_EQ(l->size(), size + i_count);
    EXPECT_EQ(l->p_sum(), i_count);
    for (size_t i = 0; i < size >> 1; i++) {
        EXPECT_EQ(l->at(i), false);
        EXPECT_EQ(l->rank(i), 0u);
    }
    for (size_t i = 0; i < i_count; i++) {
        EXPECT_EQ(l->at(i + (size >> 1)), true);
        EXPECT_EQ(l->rank(i + (size >> 1)), i);
        EXPECT_EQ(l->select(i + 1), i + (size >> 1));
    }
    for (size_t i = (size >> 1) + i_count; i < size + i_count; i++) {
        EXPECT_EQ(l->at(i), false);
        EXPECT_EQ(l->rank(i), i_count);
    }

    a->deallocate_leaf(l);
    delete a;
}

template<class rl_l, class alloc>
void rle_leaf_insert_end(uint32_t size, uint32_t i_count) {
    alloc* a = new alloc();
    rl_l* l = a->template allocate_leaf<rl_l>(32, size, false);
    EXPECT_EQ(l->size(), size);
    EXPECT_EQ(l->p_sum(), 0u);
    for (size_t i = 0; i < i_count; i++) {
        l->insert(size, true);
    }
    EXPECT_EQ(l->size(), size + i_count);
    EXPECT_EQ(l->p_sum(), i_count);
    for (size_t i = 0; i < size; i++) {
        EXPECT_EQ(l->at(i), false);
        EXPECT_EQ(l->rank(i), 0u);
    }
    for (size_t i = 0; i < i_count; i++) {
        EXPECT_EQ(l->at(size + i), true);
        EXPECT_EQ(l->rank(size + i), i);
        EXPECT_EQ(l->select(i + 1), size + i);
    }

    a->deallocate_leaf(l);
    delete a;
}


#endif