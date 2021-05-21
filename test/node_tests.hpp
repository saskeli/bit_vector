#ifndef TEST_NODE_HPP
#define TEST_NODE_HPP

#include <iostream>
#include <cstdint>
#include "../deps/googletest/googletest/include/gtest/gtest.h"

template<class node, class leaf, class alloc>
void node_add_child() {
    alloc* a = new alloc();
    node* nd = a->template allocate_node<node>();
    nd->has_leaves(true);
    for (uint64_t i = 0; i < 64; i++) {
        leaf* l = a->template allocate_leaf<leaf>(4);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, j % 2 == 0);
        }
        nd->append_child(l);
    }

    ASSERT_EQ(64u, nd->child_count());
    ASSERT_EQ(64u * 128, nd->size());
    ASSERT_EQ(64u * 64, nd->p_sum());
    uint64_t* counts = nd->child_sizes();
    uint64_t* sums = nd->child_sums();
    for (uint64_t i = 0; i < 64; i++) {
        ASSERT_EQ((i + 1) * 128, counts[i]);
        ASSERT_EQ((i + 1) * 64, sums[i]);
    }

    nd->template deallocate<alloc>();
    a->deallocate_node(nd);
}

template<class node, class leaf, class alloc>
void node_add_node() {
    alloc* a = new alloc();
    node* nd = a->template allocate_node<node>();
    nd->has_leaves();
    for (uint64_t i = 0; i < 64; i++) {
        node* n = a->template allocate_node<node>();
        n->has_leaves(true);
        leaf* l = a->template allocate_leaf<leaf>(4);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, j % 2 == 0);
        }
        n->append_child(l);
        nd->append_child(n);
    }

    ASSERT_EQ(64u, nd->child_count());
    ASSERT_EQ(64u * 128, nd->size());
    ASSERT_EQ(64u * 64, nd->p_sum());
    uint64_t* counts = nd->child_sizes();
    uint64_t* sums = nd->child_sums();
    for (uint64_t i = 0; i < 64; i++) {
        ASSERT_EQ((i + 1) * 128, counts[i]);
        ASSERT_EQ((i + 1) * 64, sums[i]);
    }

    nd->template deallocate<alloc>();
    a->deallocate_node(nd);
}

#endif