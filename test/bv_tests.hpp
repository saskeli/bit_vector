#ifndef TEST_BV_HPP
#define TEST_BV_HPP

#include <cstdint>

#include "../deps/googletest/googletest/include/gtest/gtest.h"

template <class alloc, class bit_vector>
void bv_instantiation_with_allocator_test() {
    alloc* a = new alloc();
    bit_vector* bv = new bit_vector(a);
    ASSERT_EQ(0u, bv->size());
    ASSERT_EQ(0u, bv->sum());
    ASSERT_EQ(1u, a->live_allocations());
    delete (bv);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class bit_vector>
void bv_instantiation_without_allocator_test() {
    bit_vector* bv = new bit_vector();
    ASSERT_EQ(0u, bv->size());
    ASSERT_EQ(0u, bv->sum());
    delete (bv);
}

template <class alloc, class bit_vector>
void bv_insert_split_dealloc_a_test(uint64_t size) {
    alloc* a = new alloc();
    bit_vector* bv = new bit_vector(a);
    ASSERT_EQ(0u, bv->size());
    ASSERT_EQ(0u, bv->sum());
    ASSERT_EQ(1u, a->live_allocations());
    for (uint64_t i = 0; i < size + 6; i++) {
        bv->insert(i, i % 2);
    }

    ASSERT_EQ(size + 6, bv->size());
    ASSERT_EQ((size + 6) / 2, bv->sum());
    ASSERT_EQ(3u, a->live_allocations());
    delete (bv);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class alloc, class bit_vector>
void bv_insert_split_dealloc_b_test(uint64_t size) {
    alloc* a = new alloc();
    bit_vector* bv = new bit_vector(a);
    ASSERT_EQ(0u, bv->size());
    ASSERT_EQ(0u, bv->sum());
    for (uint64_t i = 0; i < size * 100; i++) {
        bv->insert(i, i % 2);
    }

    ASSERT_EQ(size * 100, bv->size());
    ASSERT_EQ((size * 100) / 2, bv->sum());
    ASSERT_LE(100u, a->live_allocations());
    delete (bv);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class alloc, class bit_vector>
void bv_access_leaf_test(uint64_t size) {
    alloc* a = new alloc();
    bit_vector* bv = new bit_vector(a);
    ASSERT_EQ(0u, bv->size());
    ASSERT_EQ(0u, bv->sum());
    ASSERT_EQ(1u, a->live_allocations());
    for (uint64_t i = 0; i < size / 2; i++) {
        bv->insert(i, i % 2);
    }

    ASSERT_EQ(size / 2, bv->size());
    ASSERT_EQ((size / 2) / 2, bv->sum());
    ASSERT_EQ(1u, a->live_allocations());

    for (uint64_t i = 0; i < size / 2; i++) {
        ASSERT_EQ(i % 2, bv->at(i));
    }

    delete (bv);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class alloc, class bit_vector>
void bv_access_node_test(uint64_t size) {
    alloc* a = new alloc();
    bit_vector* bv = new bit_vector(a);
    ASSERT_EQ(0u, bv->size());
    ASSERT_EQ(0u, bv->sum());
    ASSERT_EQ(1u, a->live_allocations());
    for (uint64_t i = 0; i < size + 6; i++) {
        bv->insert(i, i % 2);
    }

    ASSERT_EQ(size + 6, bv->size());
    ASSERT_EQ((size + 6) / 2, bv->sum());
    ASSERT_EQ(3u, a->live_allocations());

    for (uint64_t i = 0; i < size + 6; i++) {
        ASSERT_EQ(i % 2, bv->at(i));
    }

    delete (bv);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class alloc, class bit_vector>
void bv_remove_leaf_test(uint64_t size) {
    alloc* a = new alloc();
    bit_vector* bv = new bit_vector(a);
    ASSERT_EQ(0u, bv->size());
    ASSERT_EQ(0u, bv->sum());
    ASSERT_EQ(1u, a->live_allocations());
    for (uint64_t i = 0; i < size / 2; i++) {
        bv->insert(i, i % 2);
    }

    ASSERT_EQ(size / 2, bv->size());
    ASSERT_EQ((size / 2) / 2, bv->sum());
    ASSERT_EQ(1u, a->live_allocations());

    for (uint64_t i = size / 2 - 1; i < size / 2; i -= 2) {
        bv->remove(i);
    }

    ASSERT_EQ(size / 4, bv->size());
    ASSERT_EQ(0u, bv->sum());
    ASSERT_EQ(1u, a->live_allocations());

    for (uint64_t i = 0; i < size / 4; i++) {
        ASSERT_EQ(false, bv->at(i));
    }

    delete (bv);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class alloc, class bit_vector>
void bv_remove_node_test(uint64_t size) {
    alloc* a = new alloc();
    bit_vector* bv = new bit_vector(a);
    ASSERT_EQ(0u, bv->size());
    ASSERT_EQ(0u, bv->sum());
    ASSERT_EQ(1u, a->live_allocations());
    for (uint64_t i = 0; i < size + 6; i++) {
        bv->insert(i, i % 2);
    }

    ASSERT_EQ(size + 6, bv->size());
    ASSERT_EQ((size + 6) / 2, bv->sum());
    ASSERT_EQ(3u, a->live_allocations());

    uint64_t counter = 0;
    for (uint64_t i = size + 5; i < size + 6; i -= 2) {
        bv->remove(i);
        counter++;
        ASSERT_EQ(size + 6 - counter, bv->size()) << "after " << counter << " removals";
    }

    ASSERT_EQ((size + 6) / 2, bv->size());
    ASSERT_EQ(0u, bv->sum());
    ASSERT_EQ(1u, a->live_allocations());
    for (uint64_t i = 0; i < (size + 6) / 2; i++) {
        ASSERT_EQ(false, bv->at(i));
    }

    delete (bv);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class alloc, class bit_vector>
void bv_remove_node_node_test(uint64_t size) {
    alloc* a = new alloc();
    bit_vector* bv = new bit_vector(a);
    ASSERT_EQ(0u, bv->size());
    ASSERT_EQ(0u, bv->sum());
    ASSERT_EQ(1u, a->live_allocations());
    for (uint64_t i = 0; i < size * 70; i++) {
        bv->insert(i, i % 2);
    }

    ASSERT_EQ(size * 70, bv->size());
    ASSERT_EQ((size * 70) / 2, bv->sum());
    ASSERT_LE(70u, a->live_allocations());
    for (uint64_t i = 0; i < size * 70; i++) {
        ASSERT_EQ(bv->at(i), i % 2) << "i = " << i;
    }

    for (uint64_t i = 0; i < size * 64; i++) {
        bv->remove(0);
    }

    ASSERT_EQ(size * 6, bv->size());
    for (uint64_t i = 0; i < size * 6; i++) {
        ASSERT_EQ(i % 2, bv->at(i)) << "i = " << i;
    }
    ASSERT_EQ(size * 3, bv->sum());

    delete (bv);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class alloc, class bit_vector>
void bv_set_leaf_test(uint64_t size) {
    alloc* a = new alloc();
    bit_vector* bv = new bit_vector(a);
    ASSERT_EQ(0u, bv->size());
    ASSERT_EQ(0u, bv->sum());
    ASSERT_EQ(1u, a->live_allocations());

    for (uint64_t i = 0; i < size; i++) {
        bv->insert(0, i % 2);
    }

    ASSERT_EQ(size, bv->size());
    ASSERT_EQ(size / 2, bv->sum());
    ASSERT_EQ(1u, a->live_allocations());
    for (uint64_t i = 0; i < size; i++) {
        ASSERT_EQ(i % 2 == 0, bv->at(i));
    }

    for (uint64_t i = 0; i < size; i++) {
        bv->set(i, i % 2);
    }

    ASSERT_EQ(size, bv->size());
    ASSERT_EQ(size / 2, bv->sum());
    ASSERT_EQ(1u, a->live_allocations());
    for (uint64_t i = 0; i < size; i++) {
        ASSERT_EQ(i % 2, bv->at(i));
    }

    delete (bv);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class alloc, class bit_vector>
void bv_set_node_test(uint64_t size) {
    alloc* a = new alloc();
    bit_vector* bv = new bit_vector(a);
    ASSERT_EQ(0u, bv->size());
    ASSERT_EQ(0u, bv->sum());
    ASSERT_EQ(1u, a->live_allocations());

    for (uint64_t i = 0; i < 2 * size; i++) {
        bv->insert(0, i % 2);
    }

    ASSERT_EQ(2u * size, bv->size());
    ASSERT_EQ(size, bv->sum());
    ASSERT_LE(3u, a->live_allocations());
    for (uint64_t i = 0; i < 2u * size; i++) {
        ASSERT_EQ(i % 2 == 0, bv->at(i));
    }

    for (uint64_t i = 0; i < 2u * size; i++) {
        bv->set(i, i % 2);
    }

    ASSERT_EQ(2u * size, bv->size());
    ASSERT_EQ(size, bv->sum());
    ASSERT_LE(3u, a->live_allocations());
    for (uint64_t i = 0; i < 2u * size; i++) {
        ASSERT_EQ(i % 2, bv->at(i));
    }

    delete (bv);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class alloc, class bit_vector>
void bv_rank_leaf_test(uint64_t size) {
    alloc* a = new alloc();
    bit_vector* bv = new bit_vector(a);
    ASSERT_EQ(0u, bv->size());
    ASSERT_EQ(0u, bv->sum());
    ASSERT_EQ(1u, a->live_allocations());

    for (uint64_t i = 0; i < size; i++) {
        bv->insert(0, i % 2);
    }

    ASSERT_EQ(size, bv->size());
    ASSERT_EQ(size / 2, bv->sum());
    ASSERT_EQ(1u, a->live_allocations());
    for (uint64_t i = 0; i <= size; i++) {
        ASSERT_EQ((1 + i) / 2, bv->rank(i)) << "i = " << i;
    }

    for (uint64_t i = 0; i < size; i++) {
        bv->set(i, i % 2);
    }

    ASSERT_EQ(size, bv->size());
    ASSERT_EQ(size / 2, bv->sum());
    ASSERT_EQ(1u, a->live_allocations());
    for (uint64_t i = 0; i <= size; i++) {
        ASSERT_EQ(i / 2, bv->rank(i));
    }

    delete (bv);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class alloc, class bit_vector>
void bv_rank_node_test(uint64_t size) {
    alloc* a = new alloc();
    bit_vector* bv = new bit_vector(a);
    ASSERT_EQ(0u, bv->size());
    ASSERT_EQ(0u, bv->sum());
    ASSERT_EQ(1u, a->live_allocations());

    for (uint64_t i = 0; i < 2 * size; i++) {
        bv->insert(0, i % 2);
    }

    ASSERT_EQ(2u * size, bv->size());
    ASSERT_EQ(size, bv->sum());
    ASSERT_LE(3u, a->live_allocations());
    for (uint64_t i = 0; i <= 2u * size; i++) {
        ASSERT_EQ((1 + i) / 2, bv->rank(i)) << "i = " << i;
    }

    for (uint64_t i = 0; i < 2u * size; i++) {
        bv->set(i, i % 2);
    }

    ASSERT_EQ(2u * size, bv->size());
    ASSERT_EQ(size, bv->sum());
    ASSERT_LE(3u, a->live_allocations());
    for (uint64_t i = 0; i <= 2u * size; i++) {
        ASSERT_EQ(i / 2, bv->rank(i));
    }

    delete (bv);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class alloc, class bit_vector>
void bv_select_leaf_test(uint64_t size) {
    alloc* a = new alloc();
    bit_vector* bv = new bit_vector(a);
    ASSERT_EQ(0u, bv->size());
    ASSERT_EQ(0u, bv->sum());
    ASSERT_EQ(1u, a->live_allocations());

    for (uint64_t i = 0; i < size; i++) {
        bv->insert(0, i % 2);
    }

    ASSERT_EQ(size, bv->size());
    ASSERT_EQ(size / 2, bv->sum());
    ASSERT_EQ(1u, a->live_allocations());
    for (uint64_t i = 1; i <= size / 2; i++) {
        ASSERT_EQ((i - 1) * 2, bv->select(i)) << "i = " << i;
    }

    for (uint64_t i = 0; i < size; i++) {
        bv->set(i, i % 2);
    }

    ASSERT_EQ(size, bv->size());
    ASSERT_EQ(size / 2, bv->sum());
    ASSERT_EQ(1u, a->live_allocations());
    for (uint64_t i = 1; i <= size / 2; i++) {
        ASSERT_EQ(i * 2 - 1, bv->select(i));
    }

    delete (bv);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class alloc, class bit_vector>
void bv_select_node_test(uint64_t size) {
    alloc* a = new alloc();
    bit_vector* bv = new bit_vector(a);
    ASSERT_EQ(0u, bv->size());
    ASSERT_EQ(0u, bv->sum());
    ASSERT_EQ(1u, a->live_allocations());

    for (uint64_t i = 0; i < 2 * size; i++) {
        bv->insert(0, i % 2);
    }

    ASSERT_EQ(2u * size, bv->size());
    ASSERT_EQ(size, bv->sum());
    ASSERT_LE(3u, a->live_allocations());
    for (uint64_t i = 1; i <= size; i++) {
        ASSERT_EQ((i - 1) * 2, bv->select(i)) << "i = " << i;
    }

    for (uint64_t i = 0; i < 2u * size; i++) {
        bv->set(i, i % 2);
    }

    ASSERT_EQ(2u * size, bv->size());
    ASSERT_EQ(size, bv->sum());
    ASSERT_LE(3u, a->live_allocations());
    for (uint64_t i = 1; i <= size; i++) {
        ASSERT_EQ(i * 2 - 1, bv->select(i));
    }

    delete (bv);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

#endif