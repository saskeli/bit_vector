#ifndef TEST_RLE_LEAF_HPP
#define TEST_RLE_LEAF_HPP

#include <cstdint>
#include <iostream>

#include "../deps/googletest/googletest/include/gtest/gtest.h"

template<class rl_l, class alloc>
void rle_leaf_init_zeros_test(uint32_t size) {
    alloc* a = new alloc();
    rl_l* l = a->template allocate_leaf<rl_l>(8, size, false);
    ASSERT_TRUE(l->is_compressed());
    ASSERT_EQ(l->size(), size);
    ASSERT_EQ(l->p_sum(), 0u);
    for (size_t i = 0; i < size; i++) {
        ASSERT_EQ(l->at(i), false);
        ASSERT_EQ(l->rank(i), 0u);
    }

    a->deallocate_leaf(l);
    delete a;
}

template<class rl_l, class alloc>
void rle_leaf_init_ones_test(uint32_t size) {
    alloc* a = new alloc();
    rl_l* l = a->template allocate_leaf<rl_l>(8, size, true);
    ASSERT_TRUE(l->is_compressed());
    ASSERT_EQ(l->size(), size);
    ASSERT_EQ(l->p_sum(), size);
    for (size_t i = 0; i < size; i++) {
        ASSERT_EQ(l->at(i), true);
        ASSERT_EQ(l->rank(i), i);
        ASSERT_EQ(l->select(i + 1), i);
    }

    a->deallocate_leaf(l);
    delete a;
}

template<class rl_l, class alloc>
void rle_leaf_insert_test(uint32_t size, uint32_t i_count) {
    alloc* a = new alloc();
    rl_l* l = a->template allocate_leaf<rl_l>(32, size, false);
    ASSERT_TRUE(l->is_compressed());
    ASSERT_EQ(l->size(), size);
    ASSERT_EQ(l->p_sum(), 0u);
    for (size_t i = 0; i < i_count; i++) {
        l->insert(0, true);
    }
    ASSERT_EQ(l->size(), size + i_count);
    ASSERT_EQ(l->p_sum(), i_count);
    for (size_t i = 0; i < i_count; i++) {
        ASSERT_EQ(l->at(i), true);
        ASSERT_EQ(l->rank(i), i);
        ASSERT_EQ(l->select(i + 1), i);
    }
    for (size_t i = i_count; i < size + i_count; i++) {
        ASSERT_EQ(l->at(i), false);
        ASSERT_EQ(l->rank(i), i_count);
    }

    a->deallocate_leaf(l);
    delete a;
}

template<class rl_l, class alloc>
void rle_leaf_insert_middle_test(uint32_t size, uint32_t i_count) {
    alloc* a = new alloc();
    rl_l* l = a->template allocate_leaf<rl_l>(32, size, false);
    ASSERT_TRUE(l->is_compressed());
    ASSERT_EQ(l->size(), size);
    ASSERT_EQ(l->p_sum(), 0u);
    for (size_t i = 0; i < i_count; i++) {
        l->insert(size >> 1, true);
    }
    ASSERT_EQ(l->size(), size + i_count);
    ASSERT_EQ(l->p_sum(), i_count);
    for (size_t i = 0; i < size >> 1; i++) {
        ASSERT_EQ(l->at(i), false);
        ASSERT_EQ(l->rank(i), 0u);
    }
    for (size_t i = 0; i < i_count; i++) {
        ASSERT_EQ(l->at(i + (size >> 1)), true);
        ASSERT_EQ(l->rank(i + (size >> 1)), i);
        ASSERT_EQ(l->select(i + 1), i + (size >> 1));
    }
    for (size_t i = (size >> 1) + i_count; i < size + i_count; i++) {
        ASSERT_EQ(l->at(i), false);
        ASSERT_EQ(l->rank(i), i_count);
    }

    a->deallocate_leaf(l);
    delete a;
}

template<class rl_l, class alloc>
void rle_leaf_insert_end_test(uint32_t size, uint32_t i_count) {
    alloc* a = new alloc();
    rl_l* l = a->template allocate_leaf<rl_l>(32, size, false);
    ASSERT_TRUE(l->is_compressed());
    ASSERT_EQ(l->size(), size);
    ASSERT_EQ(l->p_sum(), 0u);
    for (size_t i = 0; i < i_count; i++) {
        l->insert(size, true);
    }
    ASSERT_EQ(l->size(), size + i_count);
    ASSERT_EQ(l->p_sum(), i_count);
    for (size_t i = 0; i < size; i++) {
        ASSERT_EQ(l->at(i), false);
        ASSERT_EQ(l->rank(i), 0u);
    }
    for (size_t i = 0; i < i_count; i++) {
        ASSERT_EQ(l->at(size + i), true);
        ASSERT_EQ(l->rank(size + i), i);
        ASSERT_EQ(l->select(i + 1), size + i);
    }

    a->deallocate_leaf(l);
    delete a;
}

template<class rl_l, class alloc>
void rle_leaf_remove_test(uint32_t size, uint32_t i_count) {
    alloc* a = new alloc();
    rl_l* l = a->template allocate_leaf<rl_l>(32, size, false);
    ASSERT_TRUE(l->is_compressed());
    ASSERT_EQ(l->size(), size);
    ASSERT_EQ(l->p_sum(), 0u);
    for (size_t i = 0; i < i_count; i++) {
        l->insert(size >> 1, true);
    }
    ASSERT_EQ(l->size(), size + i_count);
    ASSERT_EQ(l->p_sum(), i_count);
    for (size_t i = 0; i < (size >> 1); i++) {
        l->remove(0);
    }
    ASSERT_EQ(l->size(), size - (size >> 1) + i_count);
    ASSERT_EQ(l->p_sum(), i_count);
    for (size_t i = 0; i < i_count; i++) {
        ASSERT_EQ(l->at(i), true);
        ASSERT_EQ(l->rank(i), i);
        ASSERT_EQ(l->select(i + 1), i);
    }
    for (size_t i = i_count; i < size - (size >> 1) + i_count; i++) {
        ASSERT_EQ(l->at(i), false);
        ASSERT_EQ(l->rank(i), i_count);
    }

    for (size_t i = l->size() - 1; i < l->size(); i -= 2) {
        l->remove(i);
    }

    ASSERT_EQ(l->size(), (size - (size >> 1) + i_count) >> 1);
    ASSERT_EQ(l->p_sum(), i_count >> 1);
    for (size_t i = 0; i < i_count >> 1; i++) {
        ASSERT_EQ(l->at(i), true);
        ASSERT_EQ(l->rank(i), i);
        ASSERT_EQ(l->select(i + 1), i);
    }

    for (size_t i = i_count >> 1; i < ((size - (size >> 1) + i_count) >> 1); i++) {
        ASSERT_EQ(l->at(i), false);
        ASSERT_EQ(l->rank(i), i_count >> 1);
    }

    a->deallocate_leaf(l);
    delete a;
}

template<class rl_l, class alloc>
void rle_leaf_set_test(uint32_t size, uint32_t i_count) {
    alloc* a = new alloc();
    rl_l* l = a->template allocate_leaf<rl_l>(32, size, false);
    ASSERT_TRUE(l->is_compressed());
    ASSERT_EQ(l->size(), size);
    ASSERT_EQ(l->p_sum(), 0u);
    for (size_t i = 0; i < size; i++) {
        l->set(i, i < i_count);
    }
    ASSERT_EQ(l->size(), size);
    ASSERT_EQ(l->p_sum(), i_count);
    for (size_t i = 0; i < size; i++) {
        ASSERT_EQ(l->at(i), i < i_count);
        ASSERT_EQ(l->rank(i), i < i_count ? i : i_count);
        
    }
    for (size_t i = 0; i < i_count; i++) {
        ASSERT_EQ(l->select(i + 1), i);
    }
    for (size_t i = 0; i < size; i++) {
        l->set(i, i >= i_count);
    }
    ASSERT_EQ(l->size(), size);
    ASSERT_EQ(l->p_sum(), i_count);
    for (size_t i = 0; i < size; i++) {
        ASSERT_EQ(l->at(i), i >= i_count);
        ASSERT_EQ(l->rank(i), i >= i_count ? i - i_count : 0);
    }
    for (size_t i = 0; i < i_count; i++) {
        ASSERT_EQ(l->select(i + 1), i + i_count);
    }
    
    a->deallocate_leaf(l);
    delete a;
}

template<class rl_l, class alloc>
void rle_leaf_cap_calculation_test() {
    alloc* a = new alloc();
    rl_l* l = a->template allocate_leaf<rl_l>(32, 10, false);
    ASSERT_TRUE(l->is_compressed());
    ASSERT_EQ(l->size(), 10u);
    ASSERT_EQ(l->p_sum(), 0u);
    ASSERT_EQ(l->desired_capacity(), 6u);
    a->deallocate_leaf(l);

    l = a->template allocate_leaf<rl_l>(32, 70, false);
    ASSERT_TRUE(l->is_compressed());
    ASSERT_EQ(l->size(), 70u);
    ASSERT_EQ(l->p_sum(), 0u);
    ASSERT_EQ(l->desired_capacity(), 8u);
    a->deallocate_leaf(l);

    l = a->template allocate_leaf<rl_l>(32, 1048576, false);
    ASSERT_TRUE(l->is_compressed());
    ASSERT_EQ(l->size(), 1048576u);
    ASSERT_EQ(l->p_sum(), 0u);
    ASSERT_EQ(l->desired_capacity(), 10u);
    a->deallocate_leaf(l);

    l = a->template allocate_leaf<rl_l>(32, 1073741824, false);
    ASSERT_TRUE(l->is_compressed());
    ASSERT_EQ(l->size(), 1073741824u);
    ASSERT_EQ(l->p_sum(), 0u);
    ASSERT_EQ(l->desired_capacity(), 12u);
    a->deallocate_leaf(l);

    delete a;
}

template<class rl_l, class alloc>
void rle_leaf_set_calculations_test() {
    alloc* a = new alloc();
    rl_l* l = a->template allocate_leaf<rl_l>(4, 10, false);
    ASSERT_TRUE(l->is_compressed());
    ASSERT_EQ(l->size(), 10u);
    ASSERT_EQ(l->p_sum(), 0u);
    ASSERT_EQ(l->need_realloc(), false);
    for (size_t i = 0; i < 15; i++) {
        l->insert(0, false);
    }
    ASSERT_EQ(l->need_realloc(), true);
    a->deallocate_leaf(l);

    delete a;
}

template<class rl_l, class alloc>
void rle_leaf_clear_first_test() {
    alloc* a = new alloc();
    rl_l* l = a->template allocate_leaf<rl_l>(32, 1000, false);
    ASSERT_TRUE(l->is_compressed());
    ASSERT_EQ(l->size(), 1000u);
    ASSERT_EQ(l->p_sum(), 0u);
    for (size_t i = 0; i < 500; i++) {
        l->set(i, true);
    }
    ASSERT_EQ(l->size(), 1000u);
    ASSERT_EQ(l->p_sum(), 500u);

    l->insert(24, false);
    l->insert(670, true);

    ASSERT_EQ(l->size(), 1002u);
    ASSERT_EQ(l->p_sum(), 501u);

    l->clear_first(400);

    ASSERT_EQ(l->size(), 602u);
    ASSERT_EQ(l->p_sum(), 102u);

    l->clear_first(200);

    ASSERT_EQ(l->size(), 402u);
    ASSERT_EQ(l->p_sum(), 1u);

    a->deallocate_leaf(l);
    delete a;
}

template<class rl_l, class alloc>
void rle_leaf_transfer_capacity_test() {
    alloc* a = new alloc();
    rl_l* l = a->template allocate_leaf<rl_l>(32, 8388608, false);
    ASSERT_TRUE(l->is_compressed());
    rl_l* lb = a->template allocate_leaf<rl_l>(32);
    ASSERT_EQ(l->size(), 8388608u);
    ASSERT_EQ(l->p_sum(), 0u);
    lb->transfer_capacity(l);
    ASSERT_EQ(l->size(), 8388608u / 2);
    ASSERT_EQ(l->p_sum(), 0u);
    ASSERT_EQ(lb->size(), 8388608u / 2);
    ASSERT_EQ(lb->p_sum(), 0u);
    a->deallocate_leaf(lb);
    a->deallocate_leaf(l);

    l = a->template allocate_leaf<rl_l>(32, 8388608, false);
    lb = a->template allocate_leaf<rl_l>(32);
    for (size_t i = 0; i < 20; i++) {
        l->insert(0, true);
    }
    ASSERT_EQ(l->size(), 8388608u + 20u);
    ASSERT_EQ(l->p_sum(), 20u);
    lb->transfer_capacity(l);
    ASSERT_EQ(l->size(), 8388608u / 2);
    ASSERT_EQ(l->p_sum(), 0u);
    ASSERT_EQ(lb->size(), 20u + 8388608u / 2);
    ASSERT_EQ(lb->p_sum(), 20u);
    a->deallocate_leaf(lb);
    a->deallocate_leaf(l);

    delete a;
}

template<class rl_l, class alloc>
void rle_leaf_clear_last_test() {
    alloc* a = new alloc();
    rl_l* l = a->template allocate_leaf<rl_l>(32, 1000, false);
    ASSERT_TRUE(l->is_compressed());
    ASSERT_EQ(l->size(), 1000u);
    ASSERT_EQ(l->p_sum(), 0u);
    for (size_t i = 500; i < 1000; i++) {
        l->set(i, true);
    }
    ASSERT_EQ(l->size(), 1000u);
    ASSERT_EQ(l->p_sum(), 500u);

    l->insert(200, true);
    l->insert(900, false);

    ASSERT_EQ(l->size(), 1002u);
    ASSERT_EQ(l->p_sum(), 501u);
    l->clear_last(400);
    ASSERT_EQ(l->size(), 602u);
    ASSERT_EQ(l->p_sum(), 102u);

    l->clear_last(200);

    ASSERT_EQ(l->size(), 402u);
    ASSERT_EQ(l->p_sum(), 1u);

    a->deallocate_leaf(l);
    delete a;
}

template<class rl_l, class alloc>
void rle_leaf_append_all_test() {
    alloc* a = new alloc();
    rl_l* la = a->template allocate_leaf<rl_l>(32);
    for (size_t i = 0; i < 100; i++) {
        la->insert(0, i % 2);
    }
    rl_l* lb = a->template allocate_leaf<rl_l>(32, 200, false);
    ASSERT_FALSE(la->is_compressed());
    ASSERT_TRUE(lb->is_compressed());
    ASSERT_EQ(la->size(), 100u);
    ASSERT_EQ(la->p_sum(), 50u);
    ASSERT_EQ(lb->size(), 200u);
    ASSERT_EQ(lb->p_sum(), 0u);
    la->append_all(lb);
    ASSERT_EQ(la->size(), 300u);
    ASSERT_EQ(la->p_sum(), 50u);
    for (size_t i = 0; i < 300; i++) {
        ASSERT_EQ(la->at(i), (i < 100) && !(i % 2));
        ASSERT_EQ(la->rank(i), i >= 100 ? 50 : (1 + i) / 2) << "i = " << i;
    }
    a->deallocate_leaf(la);
    a->deallocate_leaf(lb);

    la = a->template allocate_leaf<rl_l>(32, 100, true);
    lb = a->template allocate_leaf<rl_l>(32, 200, false);
    ASSERT_TRUE(la->is_compressed());
    ASSERT_TRUE(lb->is_compressed());
    ASSERT_EQ(la->size(), 100u);
    ASSERT_EQ(la->p_sum(), 100u);
    ASSERT_EQ(lb->size(), 200u);
    ASSERT_EQ(lb->p_sum(), 0u);
    la->append_all(lb);
    ASSERT_FALSE(la->is_compressed());
    ASSERT_EQ(la->size(), 300u);
    ASSERT_EQ(la->p_sum(), 100u);
    for (size_t i = 0; i < 300; i++) {
        ASSERT_EQ(la->at(i), i < 100);
        ASSERT_EQ(la->rank(i), i >= 100 ? 100 : i);
    }

    a->deallocate_leaf(la);
    a->deallocate_leaf(lb);
    delete a;
}

template<class rl_l, class alloc>
void rle_leaf_transfer_append_test() {
    alloc* a = new alloc();
    rl_l* la = a->template allocate_leaf<rl_l>(32, 100, true);
    la->insert(20, 0);
    la->insert(70, 0);
    rl_l* lb = a->template allocate_leaf<rl_l>(32, 200, false);
    lb->insert(50, 1);
    lb->insert(150, 1);
    ASSERT_TRUE(la->is_compressed());
    ASSERT_TRUE(lb->is_compressed());
    ASSERT_EQ(la->size(), 102u);
    ASSERT_EQ(la->p_sum(), 100u);
    ASSERT_EQ(lb->size(), 202u);
    ASSERT_EQ(lb->p_sum(), 2u);

    la->transfer_append(lb, 100);
    ASSERT_EQ(la->size(), 202u);
    ASSERT_EQ(la->p_sum(), 101u);
    ASSERT_EQ(lb->size(), 102u);
    ASSERT_EQ(lb->p_sum(), 1u);

    la->transfer_append(lb, 100);
    ASSERT_EQ(la->size(), 302u);
    ASSERT_EQ(la->p_sum(), 102u);
    ASSERT_EQ(lb->size(), 2u);
    ASSERT_EQ(lb->p_sum(), 0u);

    a->deallocate_leaf(la);
    a->deallocate_leaf(lb);
    delete a;
}

template<class rl_l, class alloc>
void rle_leaf_transfer_prepend_test() {
    alloc* a = new alloc();
    rl_l* la = a->template allocate_leaf<rl_l>(32, 100, true);
    la->insert(20, 0);
    la->insert(70, 0);
    rl_l* lb = a->template allocate_leaf<rl_l>(32, 200, false);
    lb->insert(50, 1);
    lb->insert(150, 1);
    ASSERT_TRUE(la->is_compressed());
    ASSERT_TRUE(lb->is_compressed());
    ASSERT_EQ(la->size(), 102u);
    ASSERT_EQ(la->p_sum(), 100u);
    ASSERT_EQ(lb->size(), 202u);
    ASSERT_EQ(lb->p_sum(), 2u);

    la->transfer_prepend(lb, 100);
    ASSERT_EQ(la->size(), 202u);
    ASSERT_EQ(la->p_sum(), 101u);
    ASSERT_EQ(lb->size(), 102u);
    ASSERT_EQ(lb->p_sum(), 1u);

    la->transfer_prepend(lb, 100);
    ASSERT_EQ(la->size(), 302u);
    ASSERT_EQ(la->p_sum(), 102u);
    ASSERT_EQ(lb->size(), 2u);
    ASSERT_EQ(lb->p_sum(), 0u);

    a->deallocate_leaf(la);
    a->deallocate_leaf(lb);
    delete a;
}

template<class rl_l, class alloc>
void rle_leaf_conversion_test() {
    alloc* a = new alloc();
    rl_l* la = a->template allocate_leaf<rl_l>(32, 980, false);
    ASSERT_TRUE(la->is_compressed());
    ASSERT_EQ(la->size(), 980u);
    ASSERT_EQ(la->p_sum(), 0u);
    for (size_t i = 1; i < 980; i += 2) {
        la->set(i, 1);
    }
    ASSERT_FALSE(la->is_compressed());
    ASSERT_EQ(la->size(), 980u);
    ASSERT_EQ(la->p_sum(), 490u);
    for (size_t i = 0; i < 980; i += 2) {
        la->set(i, 1);
    }
    for (size_t i = 0; i < 20; i++) {
        la->insert(0, 1);
    }
    ASSERT_TRUE(la->is_compressed());
    ASSERT_EQ(la->size(), 1000u);
    ASSERT_EQ(la->p_sum(), 1000u);

    a->deallocate_leaf(la);
    delete a;
}

TEST(RleLeaf, InitZeros) { rle_leaf_init_zeros_test<rll, ma>(10000); }

TEST(RleLeaf, InitOnes) { rle_leaf_init_ones_test<rll, ma>(10000); }

TEST(RleLeaf, Insert) { rle_leaf_insert_test<rll, ma>(10000, 100); }

TEST(RleLeaf, InsertMiddle) {
    rle_leaf_insert_middle_test<rll, ma>(10000, 100);
}

TEST(RleLeaf, InsertEnd) { rle_leaf_insert_end_test<rll, ma>(10000, 100); }

TEST(RleLeaf, Remove) { rle_leaf_remove_test<rll, ma>(200, 100); }

TEST(RleLeaf, Set) { rle_leaf_set_test<rll, ma>(200, 100); }

TEST(RleLeaf, CapCalculation) { rle_leaf_cap_calculation_test<rll, ma>(); }

TEST(RleLeaf, SetCalculations) { rle_leaf_set_calculations_test<rll, ma>(); }

TEST(RleLeaf, ClearFirst) { rle_leaf_clear_first_test<rll, ma>(); }

TEST(RleLeaf, TransferCapacity) { rle_leaf_transfer_capacity_test<rll, ma>(); }

TEST(RleLeaf, ClearLast) { rle_leaf_clear_last_test<rll, ma>(); }

TEST(RleLeaf, AppendAll) { rle_leaf_append_all_test<rll, ma>(); }

TEST(RleLeaf, TransferAppend) { rle_leaf_transfer_append_test<rll, ma>(); }

TEST(RleLeaf, TransferPrepend) { rle_leaf_transfer_prepend_test<rll, ma>(); }

TEST(RleLeaf, Convert) { rle_leaf_conversion_test<rll, ma>(); }

#endif