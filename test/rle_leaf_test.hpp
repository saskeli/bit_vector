#ifndef TEST_RLE_LEAF_HPP
#define TEST_RLE_LEAF_HPP

#include <cstdint>
#include <iostream>

#include "../deps/googletest/googletest/include/gtest/gtest.h"

template<class rl_l, class alloc>
void rle_leaf_init_zeros_test(uint32_t size) {
    alloc* a = new alloc();
    rl_l* l = a->template allocate_leaf<rl_l>(8, size, false);
    EXPECT_TRUE(l->is_compressed());
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
void rle_leaf_init_ones_test(uint32_t size) {
    alloc* a = new alloc();
    rl_l* l = a->template allocate_leaf<rl_l>(8, size, true);
    EXPECT_TRUE(l->is_compressed());
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
void rle_leaf_insert_test(uint32_t size, uint32_t i_count) {
    alloc* a = new alloc();
    rl_l* l = a->template allocate_leaf<rl_l>(32, size, false);
    EXPECT_TRUE(l->is_compressed());
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
void rle_leaf_insert_middle_test(uint32_t size, uint32_t i_count) {
    alloc* a = new alloc();
    rl_l* l = a->template allocate_leaf<rl_l>(32, size, false);
    EXPECT_TRUE(l->is_compressed());
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
void rle_leaf_insert_end_test(uint32_t size, uint32_t i_count) {
    alloc* a = new alloc();
    rl_l* l = a->template allocate_leaf<rl_l>(32, size, false);
    EXPECT_TRUE(l->is_compressed());
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

template<class rl_l, class alloc>
void rle_leaf_remove_test(uint32_t size, uint32_t i_count) {
    alloc* a = new alloc();
    rl_l* l = a->template allocate_leaf<rl_l>(32, size, false);
    EXPECT_TRUE(l->is_compressed());
    EXPECT_EQ(l->size(), size);
    EXPECT_EQ(l->p_sum(), 0u);
    for (size_t i = 0; i < i_count; i++) {
        l->insert(size >> 1, true);
    }
    EXPECT_EQ(l->size(), size + i_count);
    EXPECT_EQ(l->p_sum(), i_count);
    for (size_t i = 0; i < (size >> 1); i++) {
        l->remove(0);
    }
    EXPECT_EQ(l->size(), size - (size >> 1) + i_count);
    EXPECT_EQ(l->p_sum(), i_count);
    for (size_t i = 0; i < i_count; i++) {
        EXPECT_EQ(l->at(i), true);
        EXPECT_EQ(l->rank(i), i);
        EXPECT_EQ(l->select(i + 1), i);
    }
    for (size_t i = i_count; i < size - (size >> 1) + i_count; i++) {
        EXPECT_EQ(l->at(i), false);
        EXPECT_EQ(l->rank(i), i_count);
    }

    for (size_t i = l->size() - 1; i < l->size(); i -= 2) {
        l->remove(i);
    }

    EXPECT_EQ(l->size(), (size - (size >> 1) + i_count) >> 1);
    EXPECT_EQ(l->p_sum(), i_count >> 1);
    for (size_t i = 0; i < i_count >> 1; i++) {
        EXPECT_EQ(l->at(i), true);
        EXPECT_EQ(l->rank(i), i);
        EXPECT_EQ(l->select(i + 1), i);
    }

    for (size_t i = i_count >> 1; i < ((size - (size >> 1) + i_count) >> 1); i++) {
        EXPECT_EQ(l->at(i), false);
        EXPECT_EQ(l->rank(i), i_count >> 1);
    }

    a->deallocate_leaf(l);
    delete a;
}

template<class rl_l, class alloc>
void rle_leaf_set_test(uint32_t size, uint32_t i_count) {
    alloc* a = new alloc();
    rl_l* l = a->template allocate_leaf<rl_l>(32, size, false);
    EXPECT_TRUE(l->is_compressed());
    EXPECT_EQ(l->size(), size);
    EXPECT_EQ(l->p_sum(), 0u);
    for (size_t i = 0; i < size; i++) {
        l->set(i, i < i_count);
    }
    EXPECT_EQ(l->size(), size);
    EXPECT_EQ(l->p_sum(), i_count);
    for (size_t i = 0; i < size; i++) {
        EXPECT_EQ(l->at(i), i < i_count);
        EXPECT_EQ(l->rank(i), i < i_count ? i : i_count);
        
    }
    for (size_t i = 0; i < i_count; i++) {
        EXPECT_EQ(l->select(i + 1), i);
    }
    for (size_t i = 0; i < size; i++) {
        l->set(i, i >= i_count);
    }
    EXPECT_EQ(l->size(), size);
    EXPECT_EQ(l->p_sum(), i_count);
    for (size_t i = 0; i < size; i++) {
        EXPECT_EQ(l->at(i), i >= i_count);
        EXPECT_EQ(l->rank(i), i >= i_count ? i - i_count : 0);
    }
    for (size_t i = 0; i < i_count; i++) {
        EXPECT_EQ(l->select(i + 1), i + i_count);
    }
    
    a->deallocate_leaf(l);
    delete a;
}

template<class rl_l, class alloc>
void rle_leaf_cap_calculation_test() {
    alloc* a = new alloc();
    rl_l* l = a->template allocate_leaf<rl_l>(32, 10, false);
    EXPECT_TRUE(l->is_compressed());
    EXPECT_EQ(l->size(), 10u);
    EXPECT_EQ(l->p_sum(), 0u);
    EXPECT_EQ(l->desired_capacity(), 6u);
    a->deallocate_leaf(l);

    l = a->template allocate_leaf<rl_l>(32, 70, false);
    EXPECT_TRUE(l->is_compressed());
    EXPECT_EQ(l->size(), 70u);
    EXPECT_EQ(l->p_sum(), 0u);
    EXPECT_EQ(l->desired_capacity(), 8u);
    a->deallocate_leaf(l);

    l = a->template allocate_leaf<rl_l>(32, 1048576, false);
    EXPECT_TRUE(l->is_compressed());
    EXPECT_EQ(l->size(), 1048576u);
    EXPECT_EQ(l->p_sum(), 0u);
    EXPECT_EQ(l->desired_capacity(), 10u);
    a->deallocate_leaf(l);

    l = a->template allocate_leaf<rl_l>(32, 1073741824, false);
    EXPECT_TRUE(l->is_compressed());
    EXPECT_EQ(l->size(), 1073741824u);
    EXPECT_EQ(l->p_sum(), 0u);
    EXPECT_EQ(l->desired_capacity(), 12u);
    a->deallocate_leaf(l);

    delete a;
}

template<class rl_l, class alloc>
void rle_leaf_set_calculations_test() {
    alloc* a = new alloc();
    rl_l* l = a->template allocate_leaf<rl_l>(4, 10, false);
    EXPECT_TRUE(l->is_compressed());
    EXPECT_EQ(l->size(), 10u);
    EXPECT_EQ(l->p_sum(), 0u);
    EXPECT_EQ(l->can_set(), true);
    for (size_t i = 0; i < 15; i++) {
        l->insert(0, false);
    }
    EXPECT_EQ(l->can_set(), false);
    a->deallocate_leaf(l);

    delete a;
}

template<class rl_l, class alloc>
void rle_leaf_clear_first_test() {
    alloc* a = new alloc();
    rl_l* l = a->template allocate_leaf<rl_l>(32, 1000, false);
    EXPECT_TRUE(l->is_compressed());
    EXPECT_EQ(l->size(), 1000u);
    EXPECT_EQ(l->p_sum(), 0u);
    for (size_t i = 0; i < 500; i++) {
        l->set(i, true);
    }
    EXPECT_EQ(l->size(), 1000u);
    EXPECT_EQ(l->p_sum(), 500u);

    l->insert(24, false);
    l->insert(670, true);

    EXPECT_EQ(l->size(), 1002u);
    EXPECT_EQ(l->p_sum(), 501u);

    l->clear_first(400);

    EXPECT_EQ(l->size(), 602u);
    EXPECT_EQ(l->p_sum(), 102u);

    l->clear_first(200);

    EXPECT_EQ(l->size(), 402u);
    EXPECT_EQ(l->p_sum(), 1u);

    a->deallocate_leaf(l);
    delete a;
}

template<class rl_l, class alloc>
void rle_leaf_transfer_capacity_test() {
    alloc* a = new alloc();
    rl_l* l = a->template allocate_leaf<rl_l>(32, 8388608, false);
    EXPECT_TRUE(l->is_compressed());
    rl_l* lb = a->template allocate_leaf<rl_l>(32);
    EXPECT_EQ(l->size(), 8388608u);
    EXPECT_EQ(l->p_sum(), 0u);
    lb->transfer_capacity(l, 2);
    EXPECT_EQ(l->size(), 8388608u / 2);
    EXPECT_EQ(l->p_sum(), 0u);
    EXPECT_EQ(lb->size(), 8388608u / 2);
    EXPECT_EQ(lb->p_sum(), 0u);
    a->deallocate_leaf(lb);
    a->deallocate_leaf(l);

    l = a->template allocate_leaf<rl_l>(32, 8388608, false);
    lb = a->template allocate_leaf<rl_l>(32);
    EXPECT_EQ(l->size(), 8388608u);
    EXPECT_EQ(l->p_sum(), 0u);
    lb->transfer_capacity(l, 4);
    EXPECT_EQ(l->size(), 8388608u / 2);
    EXPECT_EQ(l->p_sum(), 0u);
    EXPECT_EQ(lb->size(), 8388608u / 2);
    EXPECT_EQ(lb->p_sum(), 0u);
    a->deallocate_leaf(lb);
    a->deallocate_leaf(l);

    l = a->template allocate_leaf<rl_l>(32, 8388608, false);
    lb = a->template allocate_leaf<rl_l>(32);
    for (size_t i = 0; i < 20; i++) {
        l->insert(0, true);
    }
    EXPECT_EQ(l->size(), 8388608u + 20u);
    EXPECT_EQ(l->p_sum(), 20u);
    lb->transfer_capacity(l, 4);
    EXPECT_EQ(l->size(), 8388608u / 2);
    EXPECT_EQ(l->p_sum(), 0u);
    EXPECT_EQ(lb->size(), 20u + 8388608u / 2);
    EXPECT_EQ(lb->p_sum(), 20u);
    a->deallocate_leaf(lb);
    a->deallocate_leaf(l);

    delete a;
}

template<class rl_l, class alloc>
void rle_leaf_clear_last_test() {
    alloc* a = new alloc();
    rl_l* l = a->template allocate_leaf<rl_l>(32, 1000, false);
    EXPECT_TRUE(l->is_compressed());
    EXPECT_EQ(l->size(), 1000u);
    EXPECT_EQ(l->p_sum(), 0u);
    for (size_t i = 500; i < 1000; i++) {
        l->set(i, true);
    }
    EXPECT_EQ(l->size(), 1000u);
    EXPECT_EQ(l->p_sum(), 500u);

    l->insert(200, true);
    l->insert(900, false);

    EXPECT_EQ(l->size(), 1002u);
    EXPECT_EQ(l->p_sum(), 501u);

    l->clear_last(400);

    EXPECT_EQ(l->size(), 602u);
    EXPECT_EQ(l->p_sum(), 102u);

    l->clear_last(200);

    EXPECT_EQ(l->size(), 402u);
    EXPECT_EQ(l->p_sum(), 1u);

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
    EXPECT_FALSE(la->is_compressed());
    EXPECT_TRUE(lb->is_compressed());
    EXPECT_EQ(la->size(), 100u);
    EXPECT_EQ(la->p_sum(), 50u);
    EXPECT_EQ(lb->size(), 200u);
    EXPECT_EQ(lb->p_sum(), 0u);
    la->append_all(lb);
    EXPECT_EQ(la->size(), 300u);
    EXPECT_EQ(la->p_sum(), 50u);
    for (size_t i = 0; i < 300; i++) {
        EXPECT_EQ(la->at(i), (i < 100) && !(i % 2));
        EXPECT_EQ(la->rank(i), i >= 100 ? 50 : (1 + i) / 2) << "i = " << i;
    }
    a->deallocate_leaf(la);
    a->deallocate_leaf(lb);

    la = a->template allocate_leaf<rl_l>(32, 100, true);
    lb = a->template allocate_leaf<rl_l>(32, 200, false);
    EXPECT_TRUE(la->is_compressed());
    EXPECT_TRUE(lb->is_compressed());
    EXPECT_EQ(la->size(), 100u);
    EXPECT_EQ(la->p_sum(), 100u);
    EXPECT_EQ(lb->size(), 200u);
    EXPECT_EQ(lb->p_sum(), 0u);
    la->append_all(lb);
    EXPECT_FALSE(la->is_compressed());
    EXPECT_EQ(la->size(), 300u);
    EXPECT_EQ(la->p_sum(), 100u);
    for (size_t i = 0; i < 300; i++) {
        EXPECT_EQ(la->at(i), i < 100);
        EXPECT_EQ(la->rank(i), i >= 100 ? 100 : i);
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
    EXPECT_TRUE(la->is_compressed());
    EXPECT_TRUE(lb->is_compressed());
    EXPECT_EQ(la->size(), 102u);
    EXPECT_EQ(la->p_sum(), 100u);
    EXPECT_EQ(lb->size(), 202u);
    EXPECT_EQ(lb->p_sum(), 2u);

    la->transfer_append(lb, 100);
    EXPECT_EQ(la->size(), 202u);
    EXPECT_EQ(la->p_sum(), 101u);
    EXPECT_EQ(lb->size(), 102u);
    EXPECT_EQ(lb->p_sum(), 1u);

    la->transfer_append(lb, 100);
    EXPECT_EQ(la->size(), 302u);
    EXPECT_EQ(la->p_sum(), 102u);
    EXPECT_EQ(lb->size(), 2u);
    EXPECT_EQ(lb->p_sum(), 0u);

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
    EXPECT_TRUE(la->is_compressed());
    EXPECT_TRUE(lb->is_compressed());
    EXPECT_EQ(la->size(), 102u);
    EXPECT_EQ(la->p_sum(), 100u);
    EXPECT_EQ(lb->size(), 202u);
    EXPECT_EQ(lb->p_sum(), 2u);

    la->transfer_prepend(lb, 100);
    EXPECT_EQ(la->size(), 202u);
    EXPECT_EQ(la->p_sum(), 101u);
    EXPECT_EQ(lb->size(), 102u);
    EXPECT_EQ(lb->p_sum(), 1u);

    la->transfer_prepend(lb, 100);
    EXPECT_EQ(la->size(), 302u);
    EXPECT_EQ(la->p_sum(), 102u);
    EXPECT_EQ(lb->size(), 2u);
    EXPECT_EQ(lb->p_sum(), 0u);

    a->deallocate_leaf(la);
    a->deallocate_leaf(lb);
    delete a;
}

template<class rl_l, class alloc>
void rle_leaf_conversion_test() {
    alloc* a = new alloc();
    rl_l* la = a->template allocate_leaf<rl_l>(32, 980, false);
    EXPECT_TRUE(la->is_compressed());
    EXPECT_EQ(la->size(), 980u);
    EXPECT_EQ(la->p_sum(), 0u);
    for (size_t i = 1; i < 980; i += 2) {
        la->set(i, 1);
    }
    EXPECT_FALSE(la->is_compressed());
    EXPECT_EQ(la->size(), 980u);
    EXPECT_EQ(la->p_sum(), 490u);
    for (size_t i = 0; i < 980; i += 2) {
        la->set(i, 1);
    }
    for (size_t i = 0; i < 20; i++) {
        la->insert(0, 1);
    }
    EXPECT_TRUE(la->is_compressed());
    EXPECT_EQ(la->size(), 1000u);
    EXPECT_EQ(la->p_sum(), 1000u);

    a->deallocate_leaf(la);
    delete a;
}

#endif