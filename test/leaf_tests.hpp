#ifndef TEST_LEAF_HPP
#define TEST_LEAF_HPP

#include <cstdint>
#include <iostream>

#include "../deps/googletest/googletest/include/gtest/gtest.h"

template <class leaf, class alloc>
void leaf_insert_test(uint64_t n) {
    alloc* allocator = new alloc();
    leaf* l = allocator->template allocate_leaf<leaf>(8);
    uint64_t hp = n / 2;
    for (uint64_t i = 0; i < hp; i++) {
        l->insert(i, false);
        if (l->need_realloc()) {
            uint64_t cap = l->capacity();
            l = allocator->template reallocate_leaf<leaf>(l, cap, 2 * cap);
        }
        uint64_t val = l->p_sum();
        uint64_t expected = 0;
        ASSERT_EQ(expected, val) << "Sum should be " << expected << " after "
                                 << (i + 1) << " 0-insertions.";
    }
    for (uint64_t i = hp; i < n; i++) {
        l->insert(hp / 2, i % 2);
        if (l->need_realloc()) {
            uint64_t cap = l->capacity();
            l = allocator->template reallocate_leaf<leaf>(l, cap, 2 * cap);
        }
        uint64_t val = l->p_sum();
        uint64_t expected = (1 + i) / 2 - hp / 2;
        ASSERT_EQ(expected, val) << "Sum should be " << expected << " after "
                                 << (1 + i - hp) << " alternating insertions.";
    }
    for (uint64_t i = 0; i < hp / 2; i++) {
        ASSERT_EQ(false, l->at(i))
            << "Initial " << (hp / 2) << " values should be 0";
    }

    bool naw = !bool(n % 2);
    for (uint64_t i = hp / 2; i < n - hp + hp / 2; i++) {
        ASSERT_EQ(naw, l->at(i))
            << "Alternating value should be " << !naw << " at " << i << ".";
        naw = !naw;
    }

    for (uint64_t i = hp / 2 + n - hp; i < n; i++) {
        ASSERT_EQ(false, l->at(i))
            << "Final " << (hp / 2) << " values should be 0";
    }

    allocator->template deallocate_leaf<leaf>(l);
    delete allocator;
}

template <class leaf, class alloc>
void leaf_append_test(uint8_t buffer_size) {
    alloc* allocator = new alloc();
    leaf* l = allocator->template allocate_leaf<leaf>(4);
    for (uint64_t i = 0; i < 4 * 64; i++) {
        l->insert(i, i % 2);
    }
    ASSERT_TRUE(l->need_realloc());
    for (uint64_t i = 0; i < buffer_size / 2; i++) {
        l->remove(64);
    }
    ASSERT_FALSE(l->need_realloc());
    ASSERT_EQ(l->size(), 4u * 64 - buffer_size / 2);
    for (uint64_t i = l->size(); i < 4 * 64; i++) {
        l->insert(i, i % 2);
    }
    ASSERT_TRUE(l->need_realloc())
        << l->size() << " < " << (l->capacity() * 64);
    for (uint64_t i = 0; i < 4 * 64; i++) {
        ASSERT_EQ(i % 2, l->at(i));
    }
}

template <class leaf, class alloc>
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
    ASSERT_EQ(val, ex) << "Should have " << hp << " elements pushed";
    ex = 0;
    val = l->p_sum();
    ASSERT_EQ(val, ex) << "Should have 0 ones pushed";

    for (uint64_t i = hp; i < n; i++) {
        l->insert(i, 1);
        if (l->need_realloc()) {
            uint64_t cap = l->capacity();
            l = allocator->template reallocate_leaf<leaf>(l, cap, 2 * cap);
        }
    }
    ex = n;
    val = l->size();
    ASSERT_EQ(val, ex) << "Should have " << ex << " elements pushed";
    ex = n - hp;
    val = l->p_sum();
    ASSERT_EQ(val, ex) << "Should have " << ex << " ones pushed";

    ex = n;
    uint64_t ex_ones = n / 2;
    uint64_t r_idx = hp - 1;
    for (uint64_t i = 0; i < n / 2; i++) {
        l->remove(0);
        --ex;
        val = l->size();
        ASSERT_EQ(val, ex) << "Size should be " << ex << " after "
                           << (2 * i + 1) << " removals.";
        val = l->p_sum();
        ASSERT_EQ(val, ex_ones) << " Should have " << ex_ones << " ones after "
                           << (2 * i + 1) << " removals.";
        
        l->remove(r_idx--);
        --ex;
        --ex_ones;
        val = l->size();
        ASSERT_EQ(val, ex) << "Size should be " << ex << " after "
                           << (2 * i + 2) << " removals.";
        val = l->p_sum();
        ASSERT_EQ(val, ex_ones) << " Should have " << ex_ones << " ones after "
                           << (2 * i + 2) << " removals.";
        for (uint64_t j = 0; j < l->size(); j++) {
            auto v = l->at(j);
            ASSERT_EQ(v, j > r_idx) << "i=" << i << ", j=" << j << ", r_idx=" << r_idx;
        }
    }

    allocator->template deallocate_leaf<leaf>(l);
    delete allocator;
}

template <class leaf, class alloc>
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

    uint64_t first = (n - 1) % 2;
    for (uint64_t i = 0; i <= n; i++) {
        ASSERT_EQ((i + first) / 2, l->rank(i)) << "Rank(" << i << ") failed";
    }

    allocator->template deallocate_leaf<leaf>(l);
    delete allocator;
}

template <class leaf, class alloc>
void leaf_select_test(uint64_t n) {
    alloc* allocator = new alloc();
    leaf* l = allocator->template allocate_leaf<leaf>(8);
    for (uint64_t i = 0; i < n; i++) {
        l->insert(0, i % 2);
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

template <class leaf, class alloc>
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

template <class leaf, class alloc>
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

template <class leaf, class alloc>
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

template <class leaf, class alloc>
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

template <class leaf, class alloc>
void leaf_transfer_prepend_test() {
    alloc* allocator = new alloc();
    leaf* a = allocator->template allocate_leaf<leaf>(10);
    leaf* b = allocator->template allocate_leaf<leaf>(6);

    uint64_t u = 64;

    for (uint64_t i = 0; i < 64 * 5; i++) {
        a->insert(0, false);
        b->insert(0, true);
    }
    for (uint64_t i = 0; i < 64; i++) {
        b->insert(0, false);
    }

    a->transfer_prepend(b, 64);
    ASSERT_EQ(u * 6, a->size());
    ASSERT_EQ(u * 1, a->p_sum());
    ASSERT_EQ(u * 5, b->size());
    ASSERT_EQ(u * 4, b->p_sum());
    for (uint64_t i = 0; i < u * 6; i++) {
        ASSERT_EQ(i < u * 1, a->at(i));
    }
    for (uint64_t i = 0; i < u * 5; i++) {
        ASSERT_EQ(i >= 64, b->at(i));
    }

    a->transfer_prepend(b, 80);
    ASSERT_EQ(u * 7 + 16, a->size());
    ASSERT_EQ(u * 2 + 16, a->p_sum());
    ASSERT_EQ(u * 4 - 16, b->size());
    ASSERT_EQ(u * 3 - 16, b->p_sum());
    for (uint64_t i = 0; i < u * 7 + 16; i++) {
        ASSERT_EQ(i < u * 2 + 16, a->at(i));
    }
    for (uint64_t i = 0; i < u * 4 - 16; i++) {
        ASSERT_EQ(i >= 64, b->at(i));
    }

    a->transfer_prepend(b, 64);
    ASSERT_EQ(u * 8 + 16, a->size());
    ASSERT_EQ(u * 3 + 16, a->p_sum());
    ASSERT_EQ(u * 3 - 16, b->size());
    ASSERT_EQ(u * 2 - 16, b->p_sum());
    for (uint64_t i = 0; i < u * 8 + 16; i++) {
        ASSERT_EQ(i < u * 3 + 16, a->at(i));
    }
    for (uint64_t i = 0; i < u * 3 - 16; i++) {
        ASSERT_EQ(i >= 64, b->at(i));
    }

    a->transfer_prepend(b, 80);
    ASSERT_EQ(u * 9 + 32, a->size());
    ASSERT_EQ(u * 4 + 32, a->p_sum());
    ASSERT_EQ(u * 2 - 32, b->size());
    ASSERT_EQ(u * 1 - 32, b->p_sum());
    for (uint64_t i = 0; i < u * 9 + 32; i++) {
        ASSERT_EQ(i < u * 4 + 32, a->at(i)) << "i = " << i;
    }
    for (uint64_t i = 0; i < u * 2 - 32; i++) {
        ASSERT_EQ(i >= 64, b->at(i));
    }

    allocator->template deallocate_leaf<leaf>(a);
    allocator->template deallocate_leaf<leaf>(b);
    delete allocator;
}

template <class leaf, class alloc>
void leaf_append_all_test() {
    alloc* allocator = new alloc();
    leaf* a = allocator->template allocate_leaf<leaf>(10);
    leaf* b = allocator->template allocate_leaf<leaf>(5);
    leaf* c = allocator->template allocate_leaf<leaf>(5);

    for (uint64_t i = 0; i < 96; i++) {
        a->insert(0, false);
        b->insert(0, true);
        c->insert(0, i % 2 == 1);
    }

    ASSERT_EQ(96u, a->size());
    ASSERT_EQ(0u, a->p_sum());
    ASSERT_EQ(96u, b->size());
    ASSERT_EQ(96u, b->p_sum());
    ASSERT_EQ(96u, c->size());
    ASSERT_EQ(48u, c->p_sum());

    for (uint64_t i = 0; i < 96; i++) {
        ASSERT_EQ(false, a->at(i));
        ASSERT_EQ(true, b->at(i));
        ASSERT_EQ(i % 2 == 0, c->at(i)) << "i = " << i;
    }

    a->append_all(b);
    ASSERT_EQ(96u * 2, a->size());
    ASSERT_EQ(96u, a->p_sum());
    for (uint64_t i = 0; i < 96 * 2; i++) {
        ASSERT_EQ(i >= 96, a->at(i)) << "i = " << i;
    }

    a->append_all(c);
    ASSERT_EQ(96u * 3, a->size());
    ASSERT_EQ(96u + 48, a->p_sum());
    for (uint64_t i = 0; i < 96 * 2; i++) {
        ASSERT_EQ(i >= 96, a->at(i));
    }
    for (uint64_t i = 96 * 2; i < 96 * 3; i++) {
        ASSERT_EQ(i % 2 == 0, a->at(i));
    }

    allocator->template deallocate_leaf<leaf>(a);
    allocator->template deallocate_leaf<leaf>(b);
    allocator->template deallocate_leaf<leaf>(c);
    delete allocator;
}

template <class leaf, class alloc>
void leaf_hit_buffer_test() {
    alloc* allocator = new alloc();
    leaf* a = allocator->template allocate_leaf<leaf>(10);

    // Put in some ones:
    for (uint64_t i = 0; i < 128; i++) {
        a->insert(0, true);
    }

    // Make sure the buffer is empty
    a->flush();

    a->set(20, true);
    ASSERT_EQ(128u, a->size());
    ASSERT_EQ(128u, a->p_sum());
    for (uint64_t i = 0; i < 128u; i++) {
        ASSERT_EQ(true, a->at(i));
    }

    // Operations (on buffer) while checking integrity
    a->insert(37, false);
    ASSERT_EQ(128u + 1, a->size());
    ASSERT_EQ(128u, a->p_sum());
    for (uint64_t i = 0; i < 128u + 1; i++) {
        ASSERT_EQ(i != 37u, a->at(i));
    }

    a->set(37, true);
    ASSERT_EQ(128u + 1, a->size());
    ASSERT_EQ(128u + 1, a->p_sum());
    for (uint64_t i = 0; i < 128u + 1; i++) {
        ASSERT_EQ(true, a->at(i));
    }

    a->set(37, true);
    ASSERT_EQ(128u + 1, a->size());
    ASSERT_EQ(128u + 1, a->p_sum());
    for (uint64_t i = 0; i < 128u + 1; i++) {
        ASSERT_EQ(true, a->at(i));
    }

    a->remove(37);
    ASSERT_EQ(128u, a->size());
    ASSERT_EQ(128u, a->p_sum());
    for (uint64_t i = 0; i < 128u; i++) {
        ASSERT_EQ(true, a->at(i));
    }

    a->insert(12, false);
    a->insert(36, false);
    a->insert(20, false);
    ASSERT_EQ(128u + 3, a->size());
    ASSERT_EQ(128u, a->p_sum());
    for (uint64_t i = 0; i < 128u + 3; i++) {
        ASSERT_EQ(i != 12 && i != 20 && i != 37, a->at(i)) << "i = " << i;
    }

    a->remove(17);
    a->set(17, false);
    ASSERT_EQ(128u + 2, a->size());
    ASSERT_EQ(128u - 2, a->p_sum());
    for (uint64_t i = 0; i < 128u + 2; i++) {
        ASSERT_EQ(i != 12 && i != 19 && i != 36 && i != 17, a->at(i))
            << "i = " << i;
    }

    a->insert(128u + 2, true);
    ASSERT_EQ(128u + 3, a->size());
    ASSERT_EQ(128u - 1, a->p_sum());
    for (uint64_t i = 0; i < 128u + 3; i++) {
        ASSERT_EQ(i != 12 && i != 19 && i != 36 && i != 17, a->at(i))
            << "i = " << i;
    }

    ASSERT_EQ(128u - 1, a->rank(128u + 3));
    a->insert(80, false);
    ASSERT_EQ(64u + 3, a->select(64u));

    // Make sure the buffer is empty
    a->flush();

    // Buffer offset calculation test for committing
    a->insert(110, false);
    a->insert(112, false);
    a->remove(114);
    a->remove(115);
    a->remove(116);
    a->remove(117);
    a->insert(120, false);
    a->insert(122, false);
    a->insert(124, false);

    ASSERT_EQ(128u + 5, a->size());
    ASSERT_EQ(128u - 5, a->p_sum());
    for (uint64_t i = 0; i < 128u + 5; i++) {
        bool v = true;
        v = i == 12 ? false : v;
        v = i == 17 ? false : v;
        v = i == 19 ? false : v;
        v = i == 36 ? false : v;
        v = i == 80 ? false : v;
        v = i == 110 ? false : v;
        v = i == 112 ? false : v;
        v = i == 120 ? false : v;
        v = i == 122 ? false : v;
        v = i == 124 ? false : v;
        ASSERT_EQ(v, a->at(i)) << "i = " << i;
    }

    allocator->template deallocate_leaf<leaf>(a);
    delete allocator;
}

template <class leaf, class alloc>
void leaf_commit_test(uint64_t size) {
    alloc* allocator = new alloc();
    leaf* l = allocator->template allocate_leaf<leaf>(size / (2 * 64));
    if (5461 >= size / 2) return;

    for (int i = 0; i < 5464; i++) {
        l->insert(i, i % 2);
    }

    ASSERT_EQ(5464u, l->size());
    ASSERT_EQ(5464u / 2, l->p_sum());
    for (uint64_t i = 0; i < 5464; i++) {
        ASSERT_EQ(i % 2, l->at(i));
    }

    l->remove(0);
    l->remove(0);
    l->remove(0);

    l->flush();

    ASSERT_EQ(5461u, l->size());
    ASSERT_EQ(5464u / 2 - 1, l->p_sum());
    for (uint64_t i = 0; i < 5461; i++) {
        ASSERT_EQ(i % 2 == 0, l->at(i));
    }

    allocator->template deallocate_leaf<leaf>(l);
    delete allocator;
}

TEST(SimpleLeaf, Insert) { leaf_insert_test<sl, ma>(10000); }

TEST(SimpleUnsLeaf, Insert) {leaf_insert_test<uns_buf_leaf, ma>(10000); }

TEST(SimpleLeaf, OverfullAppend) { leaf_append_test<leaf<8, SIZE>, ma>(8); }

TEST(SimpleUnsLeaf, OverfullAppend) { leaf_append_test<uns_buf_leaf, ma>(8); }

TEST(SimpleLeaf, Remove) { leaf_remove_test<sl, ma>(10000); }

TEST(SimpleUnsLeaf, Remove) {leaf_remove_test<uns_buf_leaf, ma>(10000); }

TEST(SimpleLeaf, Rank) { leaf_rank_test<sl, ma>(10000); }

TEST(SimpleUnsLeaf, Rank) {leaf_rank_test<uns_buf_leaf, ma>(10000); }

TEST(SimpleLeaf, RankOffset) { leaf_rank_test<sl, ma>(11); }

TEST(SimpleUnsLeaf, RankOffset) { leaf_rank_test<uns_buf_leaf, ma>(11); }

TEST(SimpleLeaf, Select) { leaf_select_test<sl, ma>(10000); }

TEST(SimpleUnsLeaf, Select) { leaf_select_test<uns_buf_leaf, ma>(10000); }

TEST(SimpleLeaf, Set) { leaf_set_test<sl, ma>(10000); }

TEST(SimpleUnsLeaf, Set) { leaf_set_test<uns_buf_leaf, ma>(10000); }

TEST(SimpleLeaf, ClearFirst) { leaf_clear_start_test<sl, ma>(); }

TEST(SimpleUnsLeaf, ClearFirst) { leaf_clear_start_test<uns_buf_leaf, ma>(); }

TEST(SimpleLeaf, TransferAppend) { leaf_transfer_append_test<sl, ma>(); }

TEST(SimpleUnsLeaf, TransferAppend) { leaf_transfer_append_test<uns_buf_leaf, ma>(); }

TEST(SimpleLeaf, ClearLast) { leaf_clear_end_test<sl, ma>(); }

TEST(SimpleUnsLeaf, ClearLast) { leaf_clear_end_test<uns_buf_leaf, ma>(); }

TEST(SimpleLeaf, TransferPrepend) { leaf_transfer_prepend_test<sl, ma>(); }

TEST(SimpleUnsLeaf, TransferPrepend) { leaf_transfer_prepend_test<uns_buf_leaf, ma>(); }

TEST(SimpleLeaf, AppendAll) { leaf_append_all_test<sl, ma>(); }

TEST(SimpleUnsLeaf, AppendAll) { leaf_append_all_test<uns_buf_leaf, ma>(); }

TEST(SimpleLeaf, BufferHit) { leaf_hit_buffer_test<sl, ma>(); }

TEST(SimpleUnsLeaf, BufferHit) { leaf_hit_buffer_test<uns_buf_leaf, ma>(); }

TEST(SimpleLeaf, Commit) { leaf_commit_test<sl, ma>(SIZE); }

TEST(SimpleUnsLeaf, Commit) { leaf_commit_test<uns_buf_leaf, ma>(SIZE); }

TEST(SimpleUnsLeaf, LbufInsert) {
    typedef leaf<1024, 16384, true, false, false> lefa;
    ma alloc;
    lefa* l = alloc.template allocate_leaf<lefa>(256);
    for (uint32_t i = 0; i < 5000; i++) {
        l->insert(i, i % 2);
    }
    ASSERT_EQ(l->size(), 5000u);
    ASSERT_EQ(l->p_sum(), 2500u);
    for (uint32_t i = 0; i < 5000; i++) {
        ASSERT_EQ(l->at(i), i % 2);
    }

    for (uint32_t i = 0; i < 5000; i++) {
        l->insert(2500, true);
    }
    ASSERT_EQ(l->size(), 10000u);
    ASSERT_EQ(l->p_sum(), 7500u);
    for (uint32_t i = 0; i < 1000; i++) {
        bool ex = i % 2 || (i >= 2500 && i < 7500);
        ASSERT_EQ(l->at(i), ex);
    }
    
    alloc.deallocate_leaf(l);
}

// A couple of tests to check that unbuffered works as well.
TEST(SimpleLeafUnb, Insert) { leaf_insert_test<ubl, ma>(10000); }

TEST(SimpleLeafUnb, Remove) { leaf_remove_test<ubl, ma>(10000); }

TEST(SimpleLeafUnb, Rank) { leaf_rank_test<ubl, ma>(10000); }

TEST(SimpleLeafUnb, RankOffset) { leaf_rank_test<ubl, ma>(11); }

TEST(SimpleLeafUnb, Select) { leaf_select_test<ubl, ma>(10000); }

TEST(SimpleLeafUnb, SelectOffset) { leaf_select_test<ubl, ma>(11); }

TEST(SimpleLeafUnb, Set) { leaf_set_test<ubl, ma>(10000); }

#endif