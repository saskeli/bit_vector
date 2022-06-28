#ifndef TEST_QUERY_SUPORT_HPP
#define TEST_QUERY_SUPORT_HPP

#include <cstdint>
#include <iostream>

#include "../deps/googletest/googletest/include/gtest/gtest.h"

template <class qs, class leaf, class alloc>
void qs_access_single_leaf(uint64_t size) {
    alloc* a = new alloc();
    leaf* l = a->template allocate_leaf<leaf>(1 + size / 64);
    for (size_t i = 0; i < size; i++) {
        l->insert(i, i % 2);
    }
    qs* q = new qs(size);
    q->append(l);
    q->finalize();
    for (size_t i = 0; i < size; i++) {
        ASSERT_EQ(q->at(i), i % 2);
    }

    a->deallocate_leaf(l);
    delete(a);
    delete(q);
}

template <class qs, class leaf, class alloc>
void qs_rank_single_leaf(uint64_t size) {
    alloc* a = new alloc();
    leaf* l = a->template allocate_leaf<leaf>(1 + size / 64);
    for (size_t i = 0; i < size; i++) {
        l->insert(i, i % 2);
    }
    qs* q = new qs(size);
    q->append(l);
    q->finalize();
    for (size_t i = 0; i < size; i++) {
        ASSERT_EQ(q->rank(i), l->rank(i));
    }

    a->deallocate_leaf(l);
    delete(a);
    delete(q);
}

template <class qs, class leaf, class alloc>
void qs_select_single_leaf(uint64_t size) {
    alloc* a = new alloc();
    leaf* l = a->template allocate_leaf<leaf>(1 + size / 64);
    for (size_t i = 0; i < size; i++) {
        l->insert(i, i % 2);
    }
    qs* q = new qs(size);
    q->append(l);
    q->finalize();
    for (size_t i = 0; i < size / 2; i++) {
        ASSERT_EQ(q->select(i + 1), l->select(i + 1)) << "i = " << i;
    }

    a->deallocate_leaf(l);
    delete(a);
    delete(q);
}

template<class qs, class leaf, class node, class alloc>
void qs_access_two_leaves(uint64_t size) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    leaf* l = a->template allocate_leaf<leaf>(1 + size / 64);
    for (size_t i = 0; i < 5 * size / 6; i++) {
        l->insert(i, i % 2);
    }
    n->append_child(l);
    l = a->template allocate_leaf<leaf>(1 + size / 64);
    for (size_t i = 0; i < size / 2; i++) {
        l->insert(i, (i + 5 * size / 6) % 2);
    }
    n->append_child(l);

    qs* q = new qs(size * 2);
    n->generate_query_structure(q);
    q->finalize();
    for (size_t i = 0; i < n->size(); i++) {
        ASSERT_EQ(n->at(i), i % 2) << "i = " << i;
        ASSERT_EQ(q->at(i), i % 2) << "i = " << i;
    }
    n->deallocate(a);
    a->deallocate_node(n);
    delete(a);
    delete(q);
}

template<class qs, class leaf, class node, class alloc>
void qs_rank_two_leaves(uint64_t size) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    leaf* l = a->template allocate_leaf<leaf>(1 + size / 64);
    for (size_t i = 0; i < 5 * size / 6; i++) {
        l->insert(i, i % 2);
    }
    n->append_child(l);
    l = a->template allocate_leaf<leaf>(1 + size / 64);
    for (size_t i = 0; i < size / 2; i++) {
        l->insert(i, (i + 5 * size / 6) % 2);
    }
    n->append_child(l);

    qs* q = new qs(size * 2);
    n->generate_query_structure(q);
    q->finalize();
    for (size_t i = 0; i < n->size(); i++) {
        ASSERT_EQ(n->rank(i), q->rank(i)) << "i = " << i;
    }
    n->deallocate(a);
    a->deallocate_node(n);
    delete(a);
    delete(q);
}

template<class qs, class leaf, class node, class alloc>
void qs_select_two_leaves(uint64_t size) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    leaf* l = a->template allocate_leaf<leaf>(1 + size / 64);
    for (size_t i = 0; i < 5 * size / 6; i++) {
        l->insert(i, i % 2);
    }
    n->append_child(l);
    l = a->template allocate_leaf<leaf>(1 + size / 64);
    for (size_t i = 0; i < size / 2; i++) {
        l->insert(i, (i + 5 * size / 6) % 2);
    }
    n->append_child(l);

    qs* q = new qs(size * 2);
    n->generate_query_structure(q);
    q->finalize();
    for (size_t i = 0; i < n->size() / 2; i++) {
        ASSERT_EQ(n->select(i + 1), q->select(i + 1)) << "i = " << i;
    }
    n->deallocate(a);
    a->deallocate_node(n);
    delete(a);
    delete(q);
}

template<class qs, class leaf, class node, class alloc>
void qs_select_two_leaves_two(uint64_t size) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    leaf* l = a->template allocate_leaf<leaf>(1 + size / 64);
    for (size_t i = 0; i < size / 2; i++) {
        l->insert(i, i % 2);
    }
    n->append_child(l);
    l = a->template allocate_leaf<leaf>(1 + size / 64);
    for (size_t i = 0; i < size / 2; i++) {
        l->insert(i, (i + size / 2) % 2);
    }
    n->append_child(l);

    qs* q = new qs(size * 2);
    n->generate_query_structure(q);
    q->finalize();
    for (size_t i = 0; i < n->size() / 2; i++) {
        ASSERT_EQ(n->select(i + 1), q->select(i + 1)) << "i = " << i;
    }
    n->deallocate(a);
    a->deallocate_node(n);
    delete a;
    delete q;
}

template<class bit_vector>
void qs_sparse_bv_select_test(uint64_t size, uint64_t ones) {
    bit_vector bv;
    uint64_t gap = 1 + size / ones;
    for (uint64_t i = 0; i < size; i++) {
        bool val = i % gap == 0;
        val = i == 0 ? false : val;
        bv.insert(0, val);
    }
    auto* qs = bv.generate_query_structure();
    ones = bv.sum();
    for (uint64_t i = 1; i <= ones; i++) {
        ASSERT_EQ(bv.select(i), qs->select(i));
    }

    delete qs;
}

TEST(QuerySupport, SingleAccess) { qs_access_single_leaf<qs, sl, ma>(SIZE); }

TEST(QuerySupport, SingleRank) { qs_rank_single_leaf<qs, sl, ma>(SIZE); }

TEST(QuerySupport, SingleSelect) { qs_select_single_leaf<qs, sl, ma>(SIZE); }

TEST(QuerySupport, DoubleAccess) { qs_access_two_leaves<qs, sl, nd, ma>(SIZE); }

TEST(QuerySupport, DoubleRank) { qs_rank_two_leaves<qs, sl, nd, ma>(SIZE); }

TEST(QuerySupport, DoubleSelect) { qs_select_two_leaves<qs, sl, nd, ma>(SIZE); }

TEST(QuerySupport, DoubleSelect2) {
    qs_select_two_leaves_two<qs, sl, nd, ma>(SIZE);
}

TEST(QuerySupport, SparseSelect) {
    qs_sparse_bv_select_test<bv::bv>(100000, 34);
}

#endif