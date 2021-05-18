#ifndef TEST_HELPERS_HPP
#define TEST_HELPERS_HPP

#include <iostream>
#include <cstdint>
#include "../deps/googletest/googletest/include/gtest/gtest.h"

template<class leaf, class alloc>
void leaf_insert_test(uint64_t n) {
    alloc* allocator = new alloc();
    leaf* l = allocator->template allocate_leaf<leaf>(8);
    uint64_t hp = n / 2;
    for (uint64_t i = 0; i < hp; i++) {
        l->insert(i, bool(0));
        if (l->need_realloc()) {
            uint64_t cap = l->capacity();
            l = allocator->template reallocate_leaf<leaf>(l, cap, 2 * cap);
        }
        uint64_t val = l->p_sum();
        uint64_t expected = 0;
        EXPECT_EQ(expected, val) << "Sum should be " << expected << " after " << (i + 1) << " 0-insertions.";
    }
    for (uint64_t i = hp; i < n; i++) {
        l->insert(hp / 2, i % 2);
        if (l->need_realloc()) {
            uint64_t cap = l->capacity();
            l = allocator->template reallocate_leaf<leaf>(l, cap, 2 * cap);
        }
        uint64_t val = l->p_sum();
        uint64_t expected = (1 + i) / 2 - hp / 2;
        EXPECT_EQ(expected, val) << "Sum should be " << expected << " after " << (1 + i - hp) << " alternating insertions.";
    }

    for (uint64_t i = 0; i < hp / 2; i ++) {
        EXPECT_EQ(false, l->at(i)) << "Initial " << (hp / 2) << " values should be 0";
    }

    bool naw = !bool(n % 2);
    for (uint64_t i = hp/2; i < n - hp + hp/2; i++) {
        EXPECT_EQ(naw, l->at(i)) << "Alternating value should be " << !naw << " at " << i << ".";
        naw = !naw;
    }

    for (uint64_t i = hp/2 + n - hp; i < n; i++) {
        EXPECT_EQ(false, l->at(i)) << "Final " << (hp / 2) << " values should be 0";
    }

    allocator->template deallocate_leaf<leaf>(l);
    delete allocator;
}

template<class leaf, class alloc>
void leaf_remove_test(uint64_t n) {
    alloc* allocator = new alloc();
    leaf* l = allocator->template allocate_leaf<leaf>(8);
    uint64_t hp = n / 2;
    for (uint64_t i = 0; i < hp; i++) {
        l->insert(i, 0);
        if (l->need_realloc()) {
            uint64_t cap = l->capacity();
            l = allocator->template reallocate_leaf<leaf>(l, cap, 2 * cap);
        }
    }
    uint64_t ex = hp;
    uint64_t val = l->size();
    EXPECT_EQ(val, hp) << "Should have " << hp << " elements pushed";
    ex = 0;
    val = l->p_sum();
    EXPECT_EQ(val, ex) << "Should have 0 ones pushed";

    for (uint64_t i = hp; i < n; i++) {
        l->insert(i, 1);
        if (l->need_realloc()) {
            uint64_t cap = l->capacity();
            l = allocator->template reallocate_leaf<leaf>(l, cap, 2 * cap);
        }
    }
    ex = n;
    val = l->size();
    EXPECT_EQ(val, ex) << "Should have " << n << " elements pushed";
    ex = n - hp;
    val = l->p_sum();
    EXPECT_EQ(val, ex) << "Should have " << (n - hp) << " ones pushed";

    for (uint64_t i = 0; i < n / 2; i++) {
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
        
    }

    allocator->template deallocate_leaf<leaf>(l);
    delete allocator;
}

template<class leaf, class alloc>
void leaf_rank_test(uint64_t n) {
    alloc* allocator = new alloc();
    leaf* l = allocator->template allocate_leaf<leaf>(8);
    for (uint64_t i = 0; i < n; i++) {
        l->insert(0, bool(i & uint64_t(1)));
        if (l->need_realloc()) {
            uint64_t cap = l->capacity();
            l = allocator->template reallocate_leaf<leaf>(l, cap, 2 * cap);
        }
    }

    uint64_t plus = (n - 1) % 2;
    for (uint64_t i = 0; i <= n; i++) {
        uint64_t ex = (i + plus) >> 1;
        uint64_t val = l->rank(i);
        ASSERT_EQ(val, ex) << "Rank(" << i << ") should be " << ex;
    }

    allocator->template deallocate_leaf<leaf>(l);
    delete allocator;
}

template<class leaf, class alloc>
void leaf_select_test(uint64_t n) {
    alloc* allocator = new alloc();
    leaf* l = allocator->template allocate_leaf<leaf>(8);
    for (uint64_t i = 0; i < n; i++) {
        l->insert(0, bool(i & uint64_t(1)));
        if (l->need_realloc()) {
            uint64_t cap = l->capacity();
            l = allocator->template reallocate_leaf<leaf>(l, cap, 2 * cap);
        }
    }

    uint64_t is_first = (n - 1) % 2;
    for (uint64_t i = 1; i <= n / 2; i++) {
        uint64_t ex = (i - is_first) * 2 + is_first - 1;
        uint64_t val = l->select(i);
        ASSERT_EQ(val, ex) << "Select(" << i << ") should be " << ex;
    }

    allocator->template deallocate_leaf<leaf>(l);
    delete allocator;
}

template<class leaf, class alloc>
void leaf_set_test(uint64_t n) {
    alloc* allocator = new alloc();
    leaf* l = allocator->template allocate_leaf<leaf>(1 + n / 64);
    for (uint64_t i = 0; i < n; i++) {
        l->insert(i, 0);
    }
    for (uint64_t i = 0; i < 6; i++) {
        l->insert(n / 2, 0);
    }
    for (uint64_t i = 1; i < n; i += 2) {
        l->set(i, true);
    }
    for (uint64_t i = 0; i < n; i++) {
        bool ex = i % 2;
        bool val = l->at(i);
        ASSERT_EQ(val, ex) << "at(" << i << ") should be " << ex;
    }
    for (uint64_t i = 0; i < n; i++) {
        l->set(i, !(i % 2));
    }
    for (uint64_t i = 0; i < n; i++) {
        bool ex = !(i % 2);
        bool val = l->at(i);
        ASSERT_EQ(val, ex) << "at(" << i << ") should be " << ex;
    }

    allocator->template deallocate_leaf<leaf>(l);
    delete allocator;
}

template<class leaf, class alloc>
void leaf_clear_start_test() {
    alloc* allocator = new alloc();
    leaf* l = allocator->template allocate_leaf<leaf>(10);
    uint64_t n = 10 * 64;
    for (uint64_t i = 0; i < 64; i++) {
        l->insert(i, false);
    }
    for (uint64_t i = 64; i < 128; i++) {
        l->insert(i, true);
    }
    for (uint64_t i = 128; i < n; i++) {
        l->insert(i, i % 2);
    }
    uint64_t num = n / 2;
    ASSERT_EQ(num, l->p_sum());

    l->clear_first(64);
    ASSERT_EQ(num, l->p_sum());
    ASSERT_EQ(n - 64, l->size());
    for (uint64_t i = 0; i < 64; i++) {
        ASSERT_EQ(l->at(i), true);
    }
    for (uint64_t i = 64; i < n - 64; i++) {
        ASSERT_EQ(l->at(i), i % 2 == 1);
    }

    l->clear_first(10);
    ASSERT_EQ(num - 10, l->p_sum());
    ASSERT_EQ(n - 64 - 10, l->size());
    for (uint64_t i = 0; i < 54; i++) {
        ASSERT_EQ(l->at(i), true);
    }
    for (uint64_t i = 54; i < n - 64 - 10; i++) {
        ASSERT_EQ(l->at(i), i % 2 == 1);
    }

    l->clear_first(64 * 2 - 10);
    ASSERT_EQ(num - 96, l->p_sum());
    ASSERT_EQ(n - 192, l->size());
    for (uint64_t i = 0; i < n - 3 * 64; i++) {
        ASSERT_EQ(l->at(i), i % 2 == 1);
    }

    allocator->template deallocate_leaf<leaf>(l);
    delete allocator;
}

template<class leaf, class alloc>
void leaf_transfer_append_test() {
    alloc* allocator = new alloc();
    leaf* a = allocator->template allocate_leaf<leaf>(10);
    leaf* b = allocator->template allocate_leaf<leaf>(5);

    uint64_t u = 64;

    for (uint64_t i = 0; i < 64 * 5; i++) {
        a->insert(0, false);
        b->insert(0, true);
    }

    a->transfer_append(b, 64);
    ASSERT_EQ(u * 6, a->size());
    ASSERT_EQ(u * 1, a->p_sum());
    ASSERT_EQ(u * 4, b->size());
    ASSERT_EQ(u * 4, b->p_sum());
    for (uint64_t i = 0; i < u * 6; i++) {
        ASSERT_EQ(i >= u * 5, a->at(i));
    }
    for (uint64_t i = 0; i < u * 4; i++) {
        ASSERT_EQ(true, b->at(i));
    }

    a->transfer_append(b, 16);
    ASSERT_EQ(u * 6 + 16, a->size());
    ASSERT_EQ(u * 1 + 16, a->p_sum());
    ASSERT_EQ(u * 4 - 16, b->size());
    ASSERT_EQ(u * 4 - 16, b->p_sum());
    for (uint64_t i = 0; i < u * 6 + 16; i++) {
        ASSERT_EQ(i >= u * 5, a->at(i));
    }
    for (uint64_t i = 0; i < u * 4 - 16; i++) {
        ASSERT_EQ(true, b->at(i));
    }

    a->transfer_append(b, 80);
    ASSERT_EQ(u * 7 + 32, a->size());
    ASSERT_EQ(u * 2 + 32, a->p_sum());
    ASSERT_EQ(u * 3 - 32, b->size());
    ASSERT_EQ(u * 3 - 32, b->p_sum());
    for (uint64_t i = 0; i < u * 7 + 32; i++) {
        ASSERT_EQ(i >= u * 5, a->at(i));
    }
    for (uint64_t i = 0; i < u * 3 - 32; i++) {
        ASSERT_EQ(true, b->at(i));
    }

    allocator->template deallocate_leaf<leaf>(a);
    allocator->template deallocate_leaf<leaf>(b);
    delete allocator;
}

template<class leaf, class alloc>
void leaf_clear_end_test() {
    alloc* allocator = new alloc();
    leaf* l = allocator->template allocate_leaf<leaf>(10);
    uint64_t n = 10 * 64;
    for (uint64_t i = 0; i < n - 128; i++) {
        l->insert(i, i % 2);
    }
    for (uint64_t i = n - 128; i < n - 64; i++) {
        l->insert(i, true);
    }
    for (uint64_t i = n - 64; i < n; i++) {
        l->insert(i, false);
    }
    
    uint64_t num = n / 2;
    ASSERT_EQ(num, l->p_sum());

    l->clear_last(64);
    ASSERT_EQ(num, l->p_sum());
    ASSERT_EQ(n - 64, l->size());
    for (uint64_t i = 0; i < n - 128; i++) {
        ASSERT_EQ(l->at(i), i % 2 == 1);
    }
    for (uint64_t i = n - 128; i < n - 64; i++) {
        ASSERT_EQ(l->at(i), true);
    }

    l->clear_last(10);
    ASSERT_EQ(num - 10, l->p_sum());
    ASSERT_EQ(n - 64 - 10, l->size());
    for (uint64_t i = 0; i < n - 128; i++) {
        ASSERT_EQ(l->at(i), i % 2 == 1);
    }
    for (uint64_t i = n - 128; i < n - 128 + 54; i++) {
        ASSERT_EQ(l->at(i), true);
    }
    

    l->clear_last(64 * 2 - 10);
    ASSERT_EQ(num - 96, l->p_sum());
    ASSERT_EQ(n - 192, l->size());
    for (uint64_t i = 0; i < n - 3 * 64; i++) {
        ASSERT_EQ(l->at(i), i % 2 == 1);
    }

    allocator->template deallocate_leaf<leaf>(l);
    delete allocator;
}

template<class leaf, class alloc>
void leaf_transfer_prepend_test() {
    alloc* allocator = new alloc();
    leaf* a = allocator->template allocate_leaf<leaf>(10);
    leaf* b = allocator->template allocate_leaf<leaf>(5);

    uint64_t u = 64;

    for (uint64_t i = 0; i < 64 * 5; i++) {
        a->insert(0, false);
        b->insert(0, true);
    }

    a->transfer_prepend(b, 64);
    ASSERT_EQ(u * 6, a->size());
    ASSERT_EQ(u * 1, a->p_sum());
    ASSERT_EQ(u * 4, b->size());
    ASSERT_EQ(u * 4, b->p_sum());
    for (uint64_t i = 0; i < u * 6; i++) {
        ASSERT_EQ(i < u * 1, a->at(i));
    }
    for (uint64_t i = 0; i < u * 4; i++) {
        ASSERT_EQ(true, b->at(i));
    }

    a->transfer_prepend(b, 16);
    ASSERT_EQ(u * 6 + 16, a->size());
    ASSERT_EQ(u * 1 + 16, a->p_sum());
    ASSERT_EQ(u * 4 - 16, b->size());
    ASSERT_EQ(u * 4 - 16, b->p_sum());
    for (uint64_t i = 0; i < u * 6 + 16; i++) {
        ASSERT_EQ(i < u + 16, a->at(i));
    }
    for (uint64_t i = 0; i < u * 4 - 16; i++) {
        ASSERT_EQ(true, b->at(i));
    }

    a->transfer_prepend(b, 80);
    ASSERT_EQ(u * 7 + 32, a->size());
    ASSERT_EQ(u * 2 + 32, a->p_sum());
    ASSERT_EQ(u * 3 - 32, b->size());
    ASSERT_EQ(u * 3 - 32, b->p_sum());
    for (uint64_t i = 0; i < u * 7 + 32; i++) {
        ASSERT_EQ(i < u * 2 + 32, a->at(i));
    }
    for (uint64_t i = 0; i < u * 3 - 32; i++) {
        ASSERT_EQ(true, b->at(i));
    }

    allocator->template deallocate_leaf<leaf>(a);
    allocator->template deallocate_leaf<leaf>(b);
    delete allocator;
}

#endif