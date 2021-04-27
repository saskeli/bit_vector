#ifndef TEST_HELPERS_HPP
#define TEST_HELPERS_HPP

#include <iostream>
#include <cstdint>
#include "../deps/googletest/googletest/include/gtest/gtest.h"

template<class leaf, class alloc, class data_type>
void leaf_pushback_test(data_type n) {
    alloc* allocator = new alloc();
    leaf* l = allocator->template allocate_leaf<leaf>(8);
    for (size_t i = 0; i < n; i++) {
        l->push_back(i % 2);
        if (l->need_realloc()) {
            data_type cap = l->capacity();
            l = allocator->template reallocate_leaf<leaf>(l, cap, 2 * cap);
        }
        data_type val = l->p_sum();
        data_type expected = (i + 1) / 2;
        EXPECT_EQ(expected, val) << "Sum should be " << expected << " after " << (i + 1) << " insertions.";
        val = l->size();
        expected = i + 1;
        EXPECT_EQ(expected, val) << "Size should be " << expected << " after " << expected << " insertions.";
    }

    for (data_type i = 0; i < n; i++) {
        bool val = l->at(i);
        bool expected = i % 2;
        EXPECT_EQ(expected, val) << "Alternating pattern should have " << expected << " at " << i << ".";
    }

    allocator->template deallocate_leaf<leaf>(l);
    delete allocator;
}

template<class leaf, class alloc, class data_type>
void leaf_insert_test(data_type n) {
    alloc* allocator = new alloc();
    leaf* l = allocator->template allocate_leaf<leaf>(8);
    data_type hp = n / 2;
    for (data_type i = 0; i < hp; i++) {
        l->insert(i, bool(0));
        if (l->need_realloc()) {
            data_type cap = l->capacity();
            l = allocator->template reallocate_leaf<leaf>(l, cap, 2 * cap);
        }
        data_type val = l->p_sum();
        data_type expected = 0;
        EXPECT_EQ(expected, val) << "Sum should be " << expected << " after " << (i + 1) << " 0-insertions.";
    }
    for (data_type i = hp; i < n; i++) {
        l->insert(hp / 2, i % 2);
        if (l->need_realloc()) {
            data_type cap = l->capacity();
            l = allocator->template reallocate_leaf<leaf>(l, cap, 2 * cap);
        }
        data_type val = l->p_sum();
        data_type expected = (1 + i) / 2 - hp / 2;
        EXPECT_EQ(expected, val) << "Sum should be " << expected << " after " << (1 + i - hp) << " alternating insertions.";
    }

    for (data_type i = 0; i < hp / 2; i ++) {
        EXPECT_EQ(false, l->at(i)) << "Initial " << (hp / 2) << " values should be 0";
    }

    bool naw = !bool(n % 2);
    for (data_type i = hp/2; i < n - hp + hp/2; i++) {
        EXPECT_EQ(naw, l->at(i)) << "Alternating value should be " << !naw << " at " << i << ".";
        naw = !naw;
    }

    for (data_type i = hp/2 + n - hp; i < n; i++) {
        EXPECT_EQ(false, l->at(i)) << "Final " << (hp / 2) << " values should be 0";
    }

    allocator->template deallocate_leaf<leaf>(l);
    delete allocator;
}

template<class leaf, class alloc, class data_type>
void leaf_remove_test(data_type n) {
    alloc* allocator = new alloc();
    leaf* l = allocator->template allocate_leaf<leaf>(8);
    data_type hp = n / 2;
    for (data_type i = 0; i < hp; i++) {
        l->push_back(0);
        if (l->need_realloc()) {
            data_type cap = l->capacity();
            l = allocator->template reallocate_leaf<leaf>(l, cap, 2 * cap);
        }
    }
    data_type ex = hp;
    data_type val = l->size();
    EXPECT_EQ(val, hp) << "Should have " << hp << " elements pushed";
    ex = 0;
    val = l->p_sum();
    EXPECT_EQ(val, ex) << "Should have 0 ones pushed";

    for (data_type i = hp; i < n; i++) {
        l->push_back(1);
        if (l->need_realloc()) {
            data_type cap = l->capacity();
            l = allocator->template reallocate_leaf<leaf>(l, cap, 2 * cap);
        }
    }
    ex = n;
    val = l->size();
    EXPECT_EQ(val, ex) << "Should have " << n << " elements pushed";
    ex = n - hp;
    val = l->p_sum();
    EXPECT_EQ(val, ex) << "Should have " << (n - hp) << " ones pushed";

    for (data_type i = 0; i < n / 2; i++) {
        l->remove(0);
        ex = n - 2 * i - 1;
        val = l->size();
        EXPECT_EQ(val, ex) << "Size should be " << ex << " after " << (2 * i + 1) << " removals.";
        ex = n / 2 - i;
        val = l->p_sum();
        EXPECT_EQ(val, ex) << " Should have " << ex << " ones after " << (2 * i + 1) << " removals.";

        l->remove(l->size() / 2);
        ex = n - 2 * (i + 1);
        val = l->size();
        EXPECT_EQ(val, ex) << "Size should be " << ex << " after " << (2 * (i + 1)) << " removals.";
        ex = n / 2 - i - 1;
        val = l->p_sum();
        EXPECT_EQ(val, ex) << " Should have " << ex << " ones after " << (2 * (i + 1)) << " removals.";
        
        // MASSIVELY SLOW O(n^2)
        //for (data_type j = 0; j < l->size() / 2; j++) {
        //    bool val_at = l->at(j);
        //    EXPECT_EQ(val_at, false) << "Value at " << j << " should be " << 0 << " for size " << l->size();
        //}
        //for (data_type j = l->size() / 2; j < l->size(); j++) {
        //    bool val_at = l->at(j);
        //    EXPECT_EQ(val_at, true) << "Value at " << j << " should be " << 1 << " for size " << l->size();;
        //}
    }

    allocator->template deallocate_leaf<leaf>(l);
    delete allocator;
}

template<class leaf, class alloc, class data_type>
void leaf_rank_test(data_type n) {
    alloc* allocator = new alloc();
    leaf* l = allocator->template allocate_leaf<leaf>(8);
    for (data_type i = 0; i < n; i++) {
        l->insert(0, bool(i & data_type(1)));
        if (l->need_realloc()) {
            data_type cap = l->capacity();
            l = allocator->template reallocate_leaf<leaf>(l, cap, 2 * cap);
        }
    }

    data_type plus = (n - 1) % 2;
    for (data_type i = 0; i <= n; i++) {
        data_type ex = (i + plus) >> 1;
        data_type val = l->rank(i);
        ASSERT_EQ(val, ex) << "Rank(" << i << ") should be " << ex;
    }

    allocator->template deallocate_leaf<leaf>(l);
    delete allocator;
}

template<class leaf, class alloc, class data_type>
void leaf_select_test(data_type n) {
    alloc* allocator = new alloc();
    leaf* l = allocator->template allocate_leaf<leaf>(8);
    for (data_type i = 0; i < n; i++) {
        l->insert(0, bool(i & data_type(1)));
        if (l->need_realloc()) {
            data_type cap = l->capacity();
            l = allocator->template reallocate_leaf<leaf>(l, cap, 2 * cap);
        }
    }

    data_type is_first = (n - 1) % 2;
    for (data_type i = 1; i <= n / 2; i++) {
        data_type ex = (i - is_first) * 2 + is_first - 1;
        data_type val = l->select(i);
        ASSERT_EQ(val, ex) << "Select(" << i << ") should be " << ex;
    }

    allocator->template deallocate_leaf<leaf>(l);
    delete allocator;
}

#endif