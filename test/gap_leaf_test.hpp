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
    allocator->template deallocate_leaf<leaf>(l);
    delete allocator;
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
    allocator->template deallocate_leaf<leaf>(l);
    delete allocator;
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
    allocator->template deallocate_leaf<leaf>(l);
    delete allocator;
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
    allocator->template deallocate_leaf<leaf>(l);
    delete allocator;
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
    allocator->template deallocate_leaf<leaf>(l);
    delete allocator;
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
    allocator->template deallocate_leaf<leaf>(l);
    delete allocator;
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
    allocator->template deallocate_leaf<leaf>(l);
    delete allocator;
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
    allocator->template deallocate_leaf<leaf>(l);
    delete allocator;
}

template <class leaf, class alloc>
void gap_leaf_pop_left_test(uint32_t elems) {
    alloc* allocator = new alloc();
    leaf* l = allocator->template allocate_leaf<leaf>();
    for (uint32_t i = 0; i < elems; i++) {
        if (l->need_realloc()) {
            l = allocator->template reallocate_leaf<leaf>(l, l->capacity(), l->desired_capacity());
        }
        l->insert(i, i % 2);
    }

    bool val = false;
    for (uint32_t i = 0; i < elems / 2; i++) {
        ASSERT_EQ(l->remove(0), val);
        val = !val;
    }
    ASSERT_EQ(l->size(), elems - elems/2);
    ASSERT_EQ(l->p_sum(), elems / 2 - (elems / 2) / 2);
    bool r_val = val;
    for (uint32_t i = 0; i < elems - elems / 2; i++) {
        ASSERT_EQ(l->at(i), r_val);
        r_val = !r_val;
    }

    for (uint32_t i = elems / 2; i < elems; i++) {
        ASSERT_EQ(l->remove(0), val);
        val = !val;
    }
    ASSERT_EQ(l->size(), 0u);
    ASSERT_EQ(l->p_sum(), 0u);
    allocator->template deallocate_leaf<leaf>(l);
    delete allocator;
}

template <class leaf, class alloc>
void gap_leaf_pop_right_test(uint32_t elems) {
    alloc* allocator = new alloc();
    leaf* l = allocator->template allocate_leaf<leaf>();
    for (uint32_t i = 0; i < elems; i++) {
        if (l->need_realloc()) {
            l = allocator->template reallocate_leaf<leaf>(l, l->capacity(), l->desired_capacity());
        }
        l->insert(0, i % 2);
    }

    bool val = false;
    for (uint32_t i = 0; i < elems / 2; i++) {
        ASSERT_EQ(l->remove(elems - 1 - i), val);
        val = !val;
    }
    ASSERT_EQ(l->size(), elems - elems/2);
    ASSERT_EQ(l->p_sum(), elems / 2 - (elems / 2) / 2);
    bool r_val = val;
    for (uint32_t i = 0; i < elems - elems / 2; i++) {
        ASSERT_EQ(l->at(elems - 1 - elems / 2 - i), r_val);
        r_val = !r_val;
    }

    for (uint32_t i = elems / 2; i < elems; i++) {
        ASSERT_EQ(l->remove(elems - i - 1), val);
        val = !val;
    }
    ASSERT_EQ(l->size(), 0u);
    ASSERT_EQ(l->p_sum(), 0u);
    allocator->template deallocate_leaf<leaf>(l);
    delete allocator;
}

template <class leaf, class alloc>
void gap_leaf_pop_loc_test(uint32_t elems, uint32_t loc) {
    ASSERT_GT(elems, loc) << "Loc should be smalelr than elems";
    alloc* allocator = new alloc();
    leaf* l = allocator->template allocate_leaf<leaf>();
    for (uint32_t i = 0; i < elems; i++) {
        if (l->need_realloc()) {
            l = allocator->template reallocate_leaf<leaf>(l, l->capacity(), l->desired_capacity());
        }
        l->insert(i, i % 2);
    }

    bool val = loc % 2;
    uint32_t half_way = elems - (loc + elems) / 2;
    for (uint32_t i = 0; i < half_way; i++) {
        ASSERT_EQ(l->remove(loc), val) << "i = " << i;
        val = !val;
    }
    ASSERT_EQ(l->size(), elems - half_way);
    uint32_t expected_sum = elems / 2;
    expected_sum -= half_way / 2;
    expected_sum += half_way % 2 && !val ? 1 : 0;
    ASSERT_EQ(l->p_sum(), expected_sum);
    for (uint32_t i = 0; i < loc; i++) {
        ASSERT_EQ(l->at(i), i % 2);
    }
    bool r_val = val;
    for (uint32_t i = loc; i < elems - half_way; i++) {
        ASSERT_EQ(l->at(i), r_val);
        r_val = !r_val;
    }

    while (l->size() > loc) {
        ASSERT_EQ(l->remove(loc), val);
        val = !val;
    }
    ASSERT_EQ(l->size(), loc);
    ASSERT_EQ(l->p_sum(), loc / 2);
    allocator->template deallocate_leaf<leaf>(l);
    delete allocator;
}

template <class leaf, class alloc>
void gap_leaf_set_test(uint64_t n) {
    alloc* allocator = new alloc();
    leaf* l = allocator->template allocate_leaf<leaf>();
    for (uint32_t i = 0; i < n; i++) {
        if (l->need_realloc()) {
            l = allocator->template reallocate_leaf<leaf>(l, l->capacity(), l->desired_capacity());
        }
        l->insert(i, 0);
    }

    for (uint32_t i = 1; i < n; i += 2) {
        ASSERT_EQ(l->set(i, true), 1);
    }
    for (uint32_t i = 0; i < n; i++) {
        ASSERT_EQ(l->at(i), bool(i % 2));
    }
    ASSERT_EQ(l->size(), n);
    ASSERT_EQ(l->p_sum(), n / 2);

    for (uint32_t i = 0; i < n; i += 2) {
        ASSERT_EQ(l->set(i, false), 0);
    }
    for (uint32_t i = 0; i < n; i++) {
        ASSERT_EQ(l->at(i), bool(i % 2));
    }
    ASSERT_EQ(l->size(), n);
    ASSERT_EQ(l->p_sum(), n / 2);

    for (uint32_t i = 0; i < n; i += 2) {
        ASSERT_EQ(l->set(i, true), 1);
    }
    for (uint32_t i = 0; i < n; i++) {
        ASSERT_EQ(l->at(i), true);
    }
    ASSERT_EQ(l->size(), n);
    ASSERT_EQ(l->p_sum(), n);

    for (uint32_t i = 1; i < n; i += 2) {
        ASSERT_EQ(l->set(i, false), -1);
    }
    for (uint32_t i = 0; i < n; i++) {
        ASSERT_EQ(l->at(i), bool(i % 2 == 0));
    }
    ASSERT_EQ(l->size(), n);
    ASSERT_EQ(l->p_sum(), n / 2);

    for (uint32_t i = 1; i < n; i += 2) {
        ASSERT_EQ(l->set(i, false), 0);
    }
    for (uint32_t i = 0; i < n; i++) {
        ASSERT_EQ(l->at(i), bool(i % 2 == 0));
    }
    ASSERT_EQ(l->size(), n);
    ASSERT_EQ(l->p_sum(), n / 2);

    for (uint32_t i = 0; i < n; i += 2) {
        ASSERT_EQ(l->set(i, false), -1);
    }
    for (uint32_t i = 0; i < n; i++) {
        ASSERT_EQ(l->at(i), false);
    }
    ASSERT_EQ(l->size(), n);
    ASSERT_EQ(l->p_sum(), 0u);
    
    allocator->template deallocate_leaf<leaf>(l);
    delete allocator;
}

template <class leaf, class alloc>
void gap_leaf_rank_test(uint64_t n) {
    alloc* allocator = new alloc();
    leaf* l = allocator->template allocate_leaf<leaf>(8);
    for (uint64_t i = 0; i < n; i++) {
        if (l->need_realloc()) {
            l = allocator->template reallocate_leaf<leaf>(l, l->capacity(), l->desired_capacity());
        }
        l->insert(0, bool(i & uint64_t(1)));
    }

    uint64_t first = (n - 1) % 2;
    for (uint64_t i = 0; i <= n; i++) {
        ASSERT_EQ((i + first) / 2, l->rank(i)) << "Rank(" << i << ") failed";
    }

    allocator->template deallocate_leaf<leaf>(l);
    delete allocator;
}

template <class leaf, class alloc>
void gap_leaf_select_test(uint64_t n) {
    alloc* allocator = new alloc();
    leaf* l = allocator->template allocate_leaf<leaf>(8);
    for (uint64_t i = 0; i < n; i++) {
        if (l->need_realloc()) {
            l = allocator->template reallocate_leaf<leaf>(l, l->capacity(), l->desired_capacity());
        }
        l->insert(0, bool(i & uint64_t(1)));
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

/*template <class leaf, class alloc>
void gap_leaf_rank_offset_test(uint64_t n) {
}

template <class leaf, class alloc>
void gap_leaf_select_offset_test(uint64_t n) {
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

TEST(GapLeaf, PrependMultiblockB) {
    gap_leaf_prepend_multiblock_test<gap_leaf<SIZE, 32, 5>, ma>(9754);
}

TEST(GapLeaf, Insert) {
    gap_leaf_insert_test<g_leaf, ma>();
}

TEST(GapLeaf, InsertMultiblock) {
    gap_leaf_insert_multiblock_test<g_leaf, ma>(10000);
}

TEST(GapLeaf, InsertOffset) {
    gap_leaf_insert_offset_test<g_leaf, ma>(10000, 2000);
}

TEST(GapLeaf, PopLeft) {
    gap_leaf_pop_left_test<g_leaf, ma>(10000);
}

TEST(GapLeaf, PopRight) {
    gap_leaf_pop_right_test<g_leaf, ma>(10000);
}

TEST(GapLeaf, PopLoc) {
    gap_leaf_pop_loc_test<g_leaf, ma>(10000, 1337);
}

TEST(GapLeaf, Set) {
    gap_leaf_set_test<g_leaf, ma>(10000);
}

TEST(GapLeaf, Rank) {
    gap_leaf_rank_test<g_leaf, ma>(6539);
}

TEST(GapLeaf, Select) {
    gap_leaf_select_test<g_leaf, ma>(8750);
}

TEST(GapLeaf, BlockJump) {
    ma* allocator = new ma();
    auto* l = allocator->template allocate_leaf<gap_leaf<SIZE, 32, 7>>();
    auto* cl = allocator->template allocate_leaf<leaf<16, SIZE>>();
    uint32_t block_size = gap_leaf<SIZE, 32, 7>::BLOCK_SIZE;

    for (uint32_t i = 0; i < block_size * 2; i++) {
        if (l->need_realloc()) {
            l = allocator->reallocate_leaf(l, l->capacity(), l->desired_capacity());
        }
        if (cl->need_realloc()) {
            cl = allocator->reallocate_leaf(cl, cl->capacity(), cl->desired_capacity());
        }
        l->insert(i, true);
        cl->insert(i, true);
    }
    for (uint32_t i = 0; i < block_size * 2; i++) {
        if (l->need_realloc()) {
            l = allocator->reallocate_leaf(l, l->capacity(), l->desired_capacity());
        }
        if (cl->need_realloc()) {
            cl = allocator->reallocate_leaf(cl, cl->capacity(), cl->desired_capacity());
        }
        if (l->data()[block_size / 64 - 1] >> 63) {
            l->insert(block_size, false);
            cl->insert(block_size, false);
        } else {
            l->insert(0, true);
            cl->insert(0, true);
        }
    }
    
    ASSERT_EQ(l->size(), cl->size());
    ASSERT_EQ(l->p_sum(), cl->p_sum());
    for (uint32_t i = 0; i < cl->size(); i++) {
        ASSERT_EQ(l->at(i), cl->at(i));
    }

    allocator->deallocate_leaf(l);
    allocator->deallocate_leaf(cl);
    delete allocator;
}

#endif