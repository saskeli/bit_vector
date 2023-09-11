#ifndef TEST_RLE_MANAGE_HPP
#define TEST_RLE_MANAGE_HPP

#include <cstdint>
#include <iostream>

#include "../deps/googletest/googletest/include/gtest/gtest.h"

template<class alloc, class node, class leaf>
void node_split_rle_test() {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);

    leaf* l = a->template allocate_leaf<leaf>(32, (~uint32_t(0)) >> 1, false);
    n->append_child(l);
    l = a->template allocate_leaf<leaf>(32, 300, false);
    n->append_child(l);
    EXPECT_EQ(n->child_count(), 2u);
    n->insert(300, true, a);
    EXPECT_EQ(n->child_count(), 3u);
    EXPECT_EQ(n->size(), ((~uint32_t(0)) >> 1) + 301u);
    EXPECT_EQ(n->p_sum(), 1u);
    EXPECT_EQ(n->at(300), true);
    for (size_t i = 0; i < n->size(); i += 10000) {
        EXPECT_EQ(n->at(i), false);
        EXPECT_EQ(n->rank(i), (i > 300) * 1u);
    }
    EXPECT_EQ(n->select(1), 300u);

    //n->deallocate(a);
    a->deallocate_leaf(n);
    delete a;
}

template<class r_bv>
void root_split_rle_test() {
    r_bv bv((~uint32_t(0)) >> 1, true);
    bv.validate();
    EXPECT_EQ(bv.size(), (~uint32_t(0)) >> 1);
    EXPECT_EQ(bv.sum(), (~uint32_t(0)) >> 1);

    bv.insert(500000, false);

    EXPECT_EQ(bv.size(), 1 + ((~uint32_t(0)) >> 1));
    EXPECT_EQ(bv.sum(), (~uint32_t(0)) >> 1);

}

TEST(RleBv, SplitLeaf) { node_split_rle_test<ma, rl_node, rll>(); }

TEST(RleBv, SplitLeafInRoot) { root_split_rle_test<rle_bv>(); }

TEST(RleBv, Bug1) {
    rle_bv bv;
    uint32_t n = 2000;
    uint32_t sigma = 30;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t t_val = i % 20;
        for (uint32_t s = 0; s < sigma; s++) {
            bv.insert(s * (i + 1) + i, t_val == s);
        }
    }

    uint32_t n_ex = n;
    
    for (uint32_t s = 0; s < sigma; s++) {
        uint32_t trg = s >= 20 ? 1000000 : s % 20;
        for (uint32_t i = 0; i < n; i++) {
            bv.set(n * s + i, false);
            if (i == trg) {
                n_ex -= 1;
                trg += 20;
            }
            ASSERT_EQ(bv.size(), n * sigma);
            ASSERT_EQ(bv.sum(), n_ex) << "i=" << i << ", s=" << s;
        }
    }
}

#endif