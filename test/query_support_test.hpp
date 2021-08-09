#ifndef TEST_QUERY_SUPORT_HPP
#define TEST_QUERY_SUPORT_HPP

#include <cstdint>
#include <iostream>

#include "../deps/googletest/googletest/include/gtest/gtest.h"

template <class qs, class leaf, class alloc>
void qs_access_single_leaf(uint64_t size) {
    alloc* a = new alloc();
    leaf* l = a->template allocate_leaf<leaf>(size / 64);
    for (size_t i = 0; i < size; i++) {
        l->insert(i, i % 2);
    }
    qs* q = new qs();
    q->append(l);
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
    leaf* l = a->template allocate_leaf<leaf>(size / 64);
    for (size_t i = 0; i < size; i++) {
        l->insert(i, i % 2);
    }
    qs* q = new qs();
    q->append(l);
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
    leaf* l = a->template allocate_leaf<leaf>(size / 64);
    for (size_t i = 0; i < size; i++) {
        l->insert(i, i % 2);
    }
    qs* q = new qs();
    q->append(l);
    for (size_t i = 0; i < size / 2; i++) {
        ASSERT_EQ(q->select(i + 1), l->select(i + 1));
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
    leaf* l = a->template allocate_leaf<leaf>(size / 64);
    for (size_t i = 0; i < 5 * size / 6; i++) {
        l->insert(i, i % 2);
    }
    n->append_child(l);
    l = a->template allocate_leaf<leaf>(size / 64);
    for (size_t i = 0; i < size / 2; i++) {
        l->insert(i, (i + 5 * size / 6) % 2);
    }
    n->append_child(l);

    qs* q = new qs();
    n->generate_query_structure(q);
    for (size_t i = 0; i < n->size(); i++) {
        ASSERT_EQ(n->at(i), i % 2) << "i = " << i;
        ASSERT_EQ(q->at(i), i % 2) << "i = " << i;
    }
    n->deallocate(a);
    a->deallocate_node(n);
    delete(a);
}

template<class qs, class leaf, class node, class alloc>
void qs_rank_two_leaves(uint64_t size) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    leaf* l = a->template allocate_leaf<leaf>(size / 64);
    for (size_t i = 0; i < 5 * size / 6; i++) {
        l->insert(i, i % 2);
    }
    n->append_child(l);
    l = a->template allocate_leaf<leaf>(size / 64);
    for (size_t i = 0; i < size / 2; i++) {
        l->insert(i, (i + 5 * size / 6) % 2);
    }
    n->append_child(l);

    qs* q = new qs();
    n->generate_query_structure(q);
    for (size_t i = 0; i < n->size(); i++) {
        ASSERT_EQ(n->rank(i), q->rank(i)) << "i = " << i;
    }
    n->deallocate(a);
    a->deallocate_node(n);
    delete(a);
}

template<class qs, class leaf, class node, class alloc>
void qs_select_two_leaves(uint64_t size) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    leaf* l = a->template allocate_leaf<leaf>(size / 64);
    for (size_t i = 0; i < 5 * size / 6; i++) {
        l->insert(i, i % 2);
    }
    n->append_child(l);
    l = a->template allocate_leaf<leaf>(size / 64);
    for (size_t i = 0; i < size / 2; i++) {
        l->insert(i, (i + 5 * size / 6) % 2);
    }
    n->append_child(l);

    qs* q = new qs();
    n->generate_query_structure(q);
    for (size_t i = 0; i < n->size() / 2; i++) {
        ASSERT_EQ(n->select(i + 1), q->select(i + 1)) << "i = " << i;
    }
    n->deallocate(a);
    a->deallocate_node(n);
    delete(a);
}

template<class qs, class leaf, class node, class alloc>
void qs_select_two_leaves_two(uint64_t size) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    leaf* l = a->template allocate_leaf<leaf>(size / 64);
    for (size_t i = 0; i < size / 2; i++) {
        l->insert(i, i % 2);
    }
    n->append_child(l);
    l = a->template allocate_leaf<leaf>(size / 64);
    for (size_t i = 0; i < size / 2; i++) {
        l->insert(i, (i + size / 2) % 2);
    }
    n->append_child(l);

    qs* q = new qs();
    n->generate_query_structure(q);
    for (size_t i = 0; i < n->size() / 2; i++) {
        ASSERT_EQ(n->select(i + 1), q->select(i + 1)) << "i = " << i;
    }
    n->deallocate(a);
    a->deallocate_node(n);
    delete(a);
}

#endif