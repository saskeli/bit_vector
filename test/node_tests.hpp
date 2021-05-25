#ifndef TEST_NODE_HPP
#define TEST_NODE_HPP

#include <iostream>
#include <cstdint>
#include "../deps/googletest/googletest/include/gtest/gtest.h"

template<class node, class leaf, class alloc>
void node_add_child_test() {
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
    delete(a);
}

template<class node, class leaf, class alloc>
void node_add_node_test() {
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
    delete(a);
}

template<class node, class leaf, class alloc>
void node_split_leaves_test() {
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

    node* sibling = nd->template split<alloc>();

    ASSERT_EQ(true, nd->has_leaves());
    ASSERT_EQ(32u, nd->child_count());
    ASSERT_EQ(32u, sibling->child_count());
    ASSERT_EQ(32u * 128, nd->size());
    ASSERT_EQ(32u * 64, nd->p_sum());
    uint64_t* counts = nd->child_sizes();
    uint64_t* sums = nd->child_sums();
    for (uint64_t i = 0; i < 32; i++) {
        ASSERT_EQ((i + 1) * 128, counts[i]);
        ASSERT_EQ((i + 1) * 64, sums[i]);
    }

    counts = sibling->child_sizes();
    sums = sibling->child_sums();
    for (uint64_t i = 0; i < 32; i++) {
        ASSERT_EQ((i + 1) * 128, counts[i]);
        ASSERT_EQ((i + 1) * 64, sums[i]);
    }

    sibling->template deallocate<alloc>();
    nd->template deallocate<alloc>();
    a->deallocate_node(nd);
    a->deallocate_node(sibling);
    delete(a);
}

template<class node, class leaf, class alloc>
void node_split_nodes_test() {
    alloc* a = new alloc();
    node* nd = a->template allocate_node<node>();
    for (uint64_t i = 0; i < 64; i++) {
        node* n = a->template allocate_node<node>();
        n->has_leaves(true);
        for (uint64_t j = 0; j < 40; j++) {
            leaf* l = a->template allocate_leaf<leaf>(4);
            for (uint64_t k = 0; k < 128; k++) {
                l->insert(0, k % 2 == 0);
            }
            n->append_child(l);
        }
        nd->append_child(n);
    }

    node* sibling = nd->template split<alloc>();

    ASSERT_EQ(false, nd->has_leaves());
    ASSERT_EQ(32u, nd->child_count());
    ASSERT_EQ(32u, sibling->child_count());
    ASSERT_EQ(32u * 128 * 40, nd->size());
    ASSERT_EQ(32u * 64 * 40, nd->p_sum());
    ASSERT_EQ(32u * 128 * 40, sibling->size());
    ASSERT_EQ(32u * 64 * 40, sibling->p_sum());
    uint64_t* counts = nd->child_sizes();
    uint64_t* sums = nd->child_sums();
    for (uint64_t i = 0; i < 32; i++) {
        ASSERT_EQ((i + 1) * 128 * 40, counts[i]);
        ASSERT_EQ((i + 1) * 64 * 40, sums[i]);
    }

    counts = sibling->child_sizes();
    sums = sibling->child_sums();
    for (uint64_t i = 0; i < 32; i++) {
        ASSERT_EQ((i + 1) * 128 * 40, counts[i]);
        ASSERT_EQ((i + 1) * 64 * 40, sums[i]);
    }

    sibling->template deallocate<alloc>();
    nd->template deallocate<alloc>();
    a->deallocate_node(nd);
    a->deallocate_node(sibling);
    ASSERT_EQ(0u, a->live_allocations());
    delete(a);
}

template<class node, class leaf, class alloc>
void node_append_all_test() {
    alloc* a = new alloc();
    node* n1 = a->template allocate_node<node>();
    node* n2 = a->template allocate_node<node>();
    n1->has_leaves(true);
    n2->has_leaves(true);
    for (uint64_t i = 0; i < 20; i++) {
        leaf* l1 = a->template allocate_leaf<leaf>(3);
        leaf* l2 = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l1->insert(0, false);
            l2->insert(0, j % 2 != 0);
        }
        n1->append_child(l1);
        n2->append_child(l2);
    }

    ASSERT_EQ(0u, n1->p_sum());
    ASSERT_EQ(20u * 128, n1->size());
    ASSERT_EQ(20u, n1->child_count());
    ASSERT_EQ(20u * 64, n2->p_sum());
    ASSERT_EQ(20u * 128, n2->size());
    ASSERT_EQ(20u, n2->child_count());
    
    n1->append_all(n2);

    ASSERT_EQ(20u * 64, n1->p_sum());
    ASSERT_EQ(40u * 128, n1->size());
    ASSERT_EQ(40u, n1->child_count());

    uint64_t* p_sums = n1->child_sums();
    uint64_t* sizes = n1->child_sizes();
    for (uint64_t i = 0; i < 40; i++) {
        ASSERT_EQ(i >= 20 ? (i - 19) * 64 : 0, p_sums[i]);
        ASSERT_EQ((i + 1) * 128, sizes[i]);
    }

    n1->template deallocate<alloc>();
    a->deallocate_node(n1);
    a->deallocate_node(n2);
    ASSERT_EQ(0u, a->live_allocations());
    delete(a);
}

template<class node, class leaf, class alloc>
void node_clear_last_test() {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    leaf* leaves[60];
    for (uint64_t i = 0; i < 60; i++) {
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, j % 2 == 1);
        }
        leaves[i] = l;
        n->append_child(l);
    }
    ASSERT_EQ(60u * 64, n->p_sum());
    ASSERT_EQ(60u * 128, n->size());
    ASSERT_EQ(60u, n->child_count());

    n->clear_last(28);

    ASSERT_EQ(32u * 64, n->p_sum());
    ASSERT_EQ(32u * 128, n->size());
    ASSERT_EQ(32u, n->child_count());

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(28u, a->live_allocations());
    for (uint64_t i = 32; i < 60; i++) {
        a->deallocate_leaf(leaves[i]);
    }
    delete(a);
}

template<class node, class leaf, class alloc>
void node_transfer_prepend_test() {
    alloc* a = new alloc();
    node* n1 = a->template allocate_node<node>();
    node* n2 = a->template allocate_node<node>();
    n1->has_leaves(true);
    n2->has_leaves(true);
    for (uint64_t i = 0; i < 60; i++) {
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, j % 2 == 1);
        }
        n1->append_child(l);
    }
    for (uint64_t i = 0; i < 20; i++) {
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, j % 2 == 1);
        }
        n2->append_child(l);
    }
    ASSERT_EQ(60u * 64, n1->p_sum());
    ASSERT_EQ(60u * 128, n1->size());
    ASSERT_EQ(60u, n1->child_count());
    ASSERT_EQ(20u * 64, n2->p_sum());
    ASSERT_EQ(20u * 128, n2->size());
    ASSERT_EQ(20u, n2->child_count());

    n2->transfer_prepend(n1, 28);

    ASSERT_EQ(32u * 64, n1->p_sum());
    ASSERT_EQ(32u * 128, n1->size());
    ASSERT_EQ(32u, n1->child_count());
    ASSERT_EQ(48u * 64, n2->p_sum());
    ASSERT_EQ(48u * 128, n2->size());
    ASSERT_EQ(48u, n2->child_count());

    n1->template deallocate<alloc>();
    n2->template deallocate<alloc>();
    a->deallocate_node(n1);
    a->deallocate_node(n2);
    ASSERT_EQ(0u, a->live_allocations());
    delete(a);
}

template<class node, class leaf, class alloc>
void node_clear_first_test() {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    leaf* leaves[60];
    for (uint64_t i = 0; i < 60; i++) {
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, j % 2 == 1);
        }
        leaves[i] = l;
        n->append_child(l);
    }
    ASSERT_EQ(60u * 64, n->p_sum());
    ASSERT_EQ(60u * 128, n->size());
    ASSERT_EQ(60u, n->child_count());

    n->clear_first(28);

    ASSERT_EQ(32u * 64, n->p_sum());
    ASSERT_EQ(32u * 128, n->size());
    ASSERT_EQ(32u, n->child_count());

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(28u, a->live_allocations());
    for (uint64_t i = 0; i < 28; i++) {
        a->deallocate_leaf(leaves[i]);
    }
    delete(a);
}

template<class node, class leaf, class alloc>
void node_transfer_append_test() {
    alloc* a = new alloc();
    node* n1 = a->template allocate_node<node>();
    node* n2 = a->template allocate_node<node>();
    n1->has_leaves(true);
    n2->has_leaves(true);
    for (uint64_t i = 0; i < 20; i++) {
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, j % 2 == 1);
        }
        n1->append_child(l);
    }
    for (uint64_t i = 0; i < 60; i++) {
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, j % 2 == 1);
        }
        n2->append_child(l);
    }
    
    ASSERT_EQ(20u * 64, n1->p_sum());
    ASSERT_EQ(20u * 128, n1->size());
    ASSERT_EQ(20u, n1->child_count());
    ASSERT_EQ(60u * 64, n2->p_sum());
    ASSERT_EQ(60u * 128, n2->size());
    ASSERT_EQ(60u, n2->child_count());

    n1->transfer_append(n2, 28);

    ASSERT_EQ(48u * 64, n1->p_sum());
    ASSERT_EQ(48u * 128, n1->size());
    ASSERT_EQ(48u, n1->child_count());
    ASSERT_EQ(32u * 64, n2->p_sum());
    ASSERT_EQ(32u * 128, n2->size());
    ASSERT_EQ(32u, n2->child_count());

    n1->template deallocate<alloc>();
    n2->template deallocate<alloc>();
    a->deallocate_node(n1);
    a->deallocate_node(n2);
    ASSERT_EQ(0u, a->live_allocations());
}

template<class node, class leaf, class alloc>
void node_access_single_leaf_test() {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    leaf* l = a->template allocate_leaf<leaf>(3);
    for (uint64_t j = 0; j < 128; j++) {
        l->insert(0, j % 2 != 0);
    }
    n->append_child(l);
    
    ASSERT_EQ(64u, n->p_sum());
    ASSERT_EQ(128u, n->size());
    ASSERT_EQ(1u, n->child_count());

    for (uint64_t i = 0; i < 128; i++) {
        ASSERT_EQ(i % 2 == 0, n->at(i));
    }
    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete(a);
}

template<class node, class leaf, class alloc>
void node_access_leaf_test() {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    for (uint64_t i = 0; i < 20; i++) {
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, false);
        }
        n->append_child(l);
    }
    for (uint64_t i = 0; i < 20; i++) {
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, true);
        }
        n->append_child(l);
    }
    for (uint64_t i = 0; i < 20; i++) {
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, j % 2 != 0);
        }
        n->append_child(l);
    }
    ASSERT_EQ(30u * 128, n->p_sum());
    ASSERT_EQ(60u * 128, n->size());
    ASSERT_EQ(60u, n->child_count());

    for (uint64_t i = 0; i < 20u * 128; i++) {
        ASSERT_EQ(false, n->at(i));
    }
    for (uint64_t i = 20u * 128; i < 40u * 128; i++) {
        ASSERT_EQ(true, n->at(i)) << "i = " << i;
    }
    for (uint64_t i = 40u * 128; i < 60u * 128; i++) {
        ASSERT_EQ(i % 2 == 0, n->at(i));
    }
    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete(a);
}

template<class node, class leaf, class alloc>
void node_access_node_test() {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    for (uint64_t i = 0; i < 20; i++) {
        node* c = a->template allocate_node<node>();
        c->has_leaves(true);
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, false);
        }
        c->append_child(l);
        n->append_child(c);
    }
    for (uint64_t i = 0; i < 20; i++) {
        node* c = a->template allocate_node<node>();
        c->has_leaves(true);
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, true);
        }
        c->append_child(l);
        n->append_child(c);
    }
    for (uint64_t i = 0; i < 20; i++) {
        node* c = a->template allocate_node<node>();
        c->has_leaves(true);
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, j % 2 != 0);
        }
        c->append_child(l);
        n->append_child(c);
    }
    ASSERT_EQ(30u * 128, n->p_sum());
    ASSERT_EQ(60u * 128, n->size());
    ASSERT_EQ(60u, n->child_count());

    for (uint64_t i = 0; i < 20u * 128; i++) {
        ASSERT_EQ(false, n->at(i));
    }
    for (uint64_t i = 20u * 128; i < 40u * 128; i++) {
        ASSERT_EQ(true, n->at(i)) << "i = " << i;
    }
    for (uint64_t i = 40u * 128; i < 60u * 128; i++) {
        ASSERT_EQ(i % 2 == 0, n->at(i));
    }
    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete(a);
}

template<class node, class leaf, class alloc>
void node_set_leaf_test() {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    for (uint64_t i = 0; i < 20; i++) {
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, false);
        }
        n->append_child(l);
    }
    ASSERT_EQ(20u * 0, n->p_sum());
    ASSERT_EQ(20u * 128, n->size());
    ASSERT_EQ(20u, n->child_count());

    for (uint64_t i = 0; i < 20u; i++) {
        for (uint64_t j = 0; j < 128; j++) {
            n->set(i * 128 + j, (j % 2) ^ (i % 2));
        }
    }

    ASSERT_EQ(20u * 64, n->p_sum());
    ASSERT_EQ(20u * 128, n->size());
    ASSERT_EQ(20u, n->child_count());

    for (uint64_t i = 0; i < 20u; i++) {
        for (uint64_t j = 0; j < 128; j++) {
            ASSERT_EQ((j % 2) ^ (i % 2), n->at(i * 128 + j)) << "i = " << i << ", j = " << j;
        }
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete(a);
}

template<class node, class leaf, class alloc>
void node_set_node_test() {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    for (uint64_t i = 0; i < 20; i++) {
        node* c = a->template allocate_node<node>();
        c->has_leaves(true);
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, false);
        }
        c->append_child(l);
        n->append_child(c);
    }
    
    ASSERT_EQ(20u * 0, n->p_sum());
    ASSERT_EQ(20u * 128, n->size());
    ASSERT_EQ(20u, n->child_count());

    for (uint64_t i = 0; i < 20u; i++) {
        for (uint64_t j = 0; j < 128; j++) {
            n->set(i * 128 + j, (j % 2) ^ (i % 2));
        }
    }

    ASSERT_EQ(20u * 64, n->p_sum());
    ASSERT_EQ(20u * 128, n->size());
    ASSERT_EQ(20u, n->child_count());

    for (uint64_t i = 0; i < 20u; i++) {
        for (uint64_t j = 0; j < 128; j++) {
            ASSERT_EQ((j % 2) ^ (i % 2), n->at(i * 128 + j)) << "i = " << i << ", j = " << j;
        }
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete(a);
}

template<class node, class leaf, class alloc>
void node_rank_leaf_test() {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    for (uint64_t i = 0; i < 20; i++) {
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, (j % 2) ^ (i % 2));
        }
        n->append_child(l);
    }
    ASSERT_EQ(20u * 64, n->p_sum());
    ASSERT_EQ(20u * 128, n->size());
    ASSERT_EQ(20u, n->child_count());

    for (uint64_t i = 0; i < 20u; i++) {
        uint64_t partial = i * 64;
        uint64_t start = i % 2 == 0;
        for (uint64_t j = 0; j < 128; j++) {
            ASSERT_EQ(partial + ((j + start) >> 1), n->rank(i * 128 + j));
        }
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete(a);
}

template<class node, class leaf, class alloc>
void node_rank_node_test() {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    for (uint64_t i = 0; i < 20; i++) {
        node* c = a->template allocate_node<node>();
        c->has_leaves(true);
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, (j % 2) ^ (i % 2));
        }
        c->append_child(l);
        n->append_child(c);
    }
    ASSERT_EQ(20u * 64, n->p_sum());
    ASSERT_EQ(20u * 128, n->size());
    ASSERT_EQ(20u, n->child_count());

    for (uint64_t i = 0; i < 20u; i++) {
        uint64_t partial = i * 64;
        uint64_t start = i % 2 == 0;
        for (uint64_t j = 0; j < 128; j++) {
            ASSERT_EQ(partial + ((j + start) >> 1), n->rank(i * 128 + j));
        }
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete(a);
}

template<class node, class leaf, class alloc>
void node_select_single_leaf_test() {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    leaf* l = a->template allocate_leaf<leaf>(3);
    for (uint64_t j = 0; j < 128; j++) {
        l->insert(0, j % 2);
    }
    n->append_child(l);

    ASSERT_EQ(1u * 64, n->p_sum());
    ASSERT_EQ(1u * 128, n->size());
    ASSERT_EQ(1u, n->child_count());

    for (uint64_t j = 1; j <= 64; j++) {
        ASSERT_EQ((j - 1) *  2, n->select(j));
    }

    for (uint64_t j = 0; j < 128; j++) {
        l->set(j, j % 2);
    }

    ASSERT_EQ(1u * 64, n->p_sum());
    ASSERT_EQ(1u * 128, n->size());
    ASSERT_EQ(1u, n->child_count());

    for (uint64_t j = 1; j <= 64; j++) {
        ASSERT_EQ(1 + (j - 1) *  2, n->select(j));
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete(a);
}

template<class node, class leaf, class alloc>
void node_select_leaf_test() {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    for (uint64_t i = 0; i < 20; i++) {
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, (j % 2) ^ (i % 2));
        }
        n->append_child(l);
    }

    ASSERT_EQ(20u * 64, n->p_sum());
    ASSERT_EQ(20u * 128, n->size());
    ASSERT_EQ(20u, n->child_count());
    for (uint64_t i = 0; i < 20; i++) {
        uint64_t partial = i * 64;
        uint64_t plus = i % 2 != 0;
        for (uint64_t j = 1; j <= 64; j++) {
            ASSERT_EQ(partial * 2 + plus + (j - 1) *  2, n->select(partial + j));
        }
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete(a);
}

template<class node, class leaf, class alloc>
void node_select_node_test() {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    for (uint64_t i = 0; i < 20; i++) {
        node* c = a->template allocate_node<node>();
        c->has_leaves(true);
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, (j % 2) ^ (i % 2));
        }
        c->append_child(l);
        n->append_child(c);
    }

    ASSERT_EQ(20u * 64, n->p_sum());
    ASSERT_EQ(20u * 128, n->size());
    ASSERT_EQ(20u, n->child_count());
    for (uint64_t i = 0; i < 20; i++) {
        uint64_t partial = i * 64;
        uint64_t plus = i % 2 != 0;
        for (uint64_t j = 1; j <= 64; j++) {
            ASSERT_EQ(partial * 2 + plus + (j - 1) *  2, n->select(partial + j));
        }
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete(a);
}

template<class node, class leaf, class alloc>
void node_insert_single_leaf_test() {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    leaf* l = a->template allocate_leaf<leaf>(3);
    n->append_child(l);
    for (uint64_t i = 0; i < 128; i++) {
        n->template insert<alloc>(0, i % 2);
    }

    ASSERT_EQ(1u * 64, n->p_sum());
    ASSERT_EQ(1u * 128, n->size());
    ASSERT_EQ(1u, n->child_count());

    for (uint64_t i = 0; i < 128; i++) {
        ASSERT_EQ(i % 2 == 0, n->at(i));
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete(a);
}

template<class node, class leaf, class alloc>
void node_insert_single_leaf_realloc_test() {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    leaf* l = a->template allocate_leaf<leaf>(1);
    n->append_child(l);
    for (uint64_t i = 0; i < 128; i++) {
        n->template insert<alloc>(0, i % 2);
    }

    ASSERT_EQ(1u * 64, n->p_sum());
    ASSERT_EQ(1u * 128, n->size());
    ASSERT_EQ(1u, n->child_count());

    for (uint64_t i = 0; i < 128; i++) {
        ASSERT_EQ(i % 2 == 0, n->at(i));
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete(a);
}

template<class node, class leaf, class alloc>
void node_insert_single_leaf_split_test(uint64_t size) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    leaf* l = a->template allocate_leaf<leaf>(size / 64);
    n->append_child(l);
    for (uint64_t i = 0; i < size + 1; i++) {
        n->template insert<alloc>(i, i % 2);
    }

    ASSERT_EQ(size / 2 + size % 2, n->p_sum());
    ASSERT_EQ(size + 1, n->size());
    ASSERT_EQ(2u, n->child_count());

    for (uint64_t i = 0; i < size + 1; i++) {
        ASSERT_EQ(i % 2, n->at(i)) << "i = " << i;
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete(a);
}

template<class node, class leaf, class alloc>
void node_inset_leaf_split_test(uint64_t size) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    for (uint64_t i = 0; i < 2; i++) {
        leaf* l = a->template allocate_leaf<leaf>(size / 64);
        for (uint64_t j = 0; j < size; j++) {
            l->insert(j, j % 2);
        }
        n->append_child(l);
    }

    n->template insert<alloc>(size + 5, true);

    ASSERT_EQ(size + 1, n->p_sum());
    ASSERT_EQ(2 * size + 1, n->size());
    ASSERT_EQ(3u, n->child_count());

    for (uint64_t i = 0; i < size + 5; i++) {
        ASSERT_EQ(i % 2, n->at(i)) << "i = " << i;
    }
    ASSERT_EQ(true, n->at(size + 5));
    for (uint64_t i = size + 6; i < 2 * size + 1; i++) {
        ASSERT_EQ((i + 1) % 2, n->at(i)) << "i = " << i;
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete(a);
}

template<class node, class leaf, class alloc>
void node_insert_node_test() {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    for (uint64_t i = 0; i < 6; i++) {
        node* c = a->template allocate_node<node>();
        c->has_leaves(true);
        leaf* l = a->template allocate_leaf<leaf>(4);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(j, false);
        }
        c->append_child(l);
        n->append_child(c);
    }
    
    ASSERT_EQ(6u, n->child_count());
    ASSERT_EQ(6u * 128, n->size());
    ASSERT_EQ(6u * 0, n->p_sum());

    for (uint64_t i = 6u * 128; i > 0; i--) {
        n->template insert<alloc>(i, true);
    }

    ASSERT_EQ(6u, n->child_count());
    ASSERT_EQ(6u * 128 * 2, n->size());
    ASSERT_EQ(6u * 128, n->p_sum());

    for (uint64_t i = 0; i < 6u * 128 * 2; i++) {
        ASSERT_EQ(i % 2, n->at(i));
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete(a);
}

template<class node, class leaf, class alloc>
void node_insert_node_split_test() {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    for (uint64_t i = 0; i < 2; i++) {
        node* c = a->template allocate_node<node>();
        c->has_leaves(true);
        for (uint64_t j = 0; j < 64; j++) {
            leaf* l = a->template allocate_leaf<leaf>(4);
            for (uint64_t j = 0; j < 128; j++) {
                l->insert(j, false);
            }
            c->append_child(l);
        }
        ASSERT_EQ(64, c->child_count());
        n->append_child(c);
    }
    
    ASSERT_EQ(2u, n->child_count());
    ASSERT_EQ(2u * 128 * 64, n->size());
    ASSERT_EQ(2u * 0, n->p_sum());

    for (uint64_t i = 2u * 128 * 64; i > 0; i--) {
        n->template insert<alloc>(i, true);
    }

    ASSERT_EQ(4u, n->child_count());
    ASSERT_EQ(4u * 128 * 64, n->size());
    ASSERT_EQ(2u * 128 * 64, n->p_sum());

    for (uint64_t i = 0; i < 4u * 128 * 64; i++) {
        ASSERT_EQ(i % 2, n->at(i));
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete(a);
}

template<class node, class leaf, class alloc>
void node_remove_leaf_simple_test(uint64_t size) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    for (uint64_t i = 0; i < 2; i++){
        leaf* l = a->template allocate_leaf<leaf>(size / 64);
        for (uint64_t j = 0; j < size; j++) {
            l->insert(j, j % 2);
        }
        n->append_child(l);
    }
    
    ASSERT_EQ(2u, n->child_count());
    ASSERT_EQ(2u * size, n->size());
    ASSERT_EQ(2u * size / 2, n->p_sum());

    for (uint64_t i = 2u * size - 1; i < 2u * size; i -= 2) {
        n->template remove<alloc>(i);
    }

    ASSERT_EQ(2u, n->child_count());
    ASSERT_EQ(1u * size, n->size());
    ASSERT_EQ(1u * 0, n->p_sum());

    for (uint64_t i = 0; i < size / 2; i++) {
        ASSERT_EQ(false, n->at(i));
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete(a);
}

template<class node, class leaf, class alloc>
void node_remove_leaf_test(uint64_t size) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    leaf* l = a->template allocate_leaf<leaf>(size / 64);
    for (uint64_t j = 0; j < 2 * (size / 3); j++) {
        l->insert(j, j % 2);
    }
    n->append_child(l);

    l = a->template allocate_leaf<leaf>(size / 64);
    for (uint64_t j = 0; j < size / 3; j++) {
        l->insert(j, j % 2);
    }
    n->append_child(l);

    l = a->template allocate_leaf<leaf>(size / 64);
    for (uint64_t j = 0; j < 2 * (size / 3); j++) {
        l->insert(j, j % 2);
    }
    n->append_child(l);
    
    ASSERT_EQ(3u, n->child_count());
    ASSERT_EQ(5 * (size / 3), n->size());
    ASSERT_EQ(5 * (size / 3) / 2, n->p_sum());

    n->template remove<alloc>(5 * size / 6);

    ASSERT_EQ(2u, n->child_count());
    ASSERT_EQ(5 * (size / 3) - 1, n->size());
    ASSERT_EQ(5 * (size / 3) / 2, n->p_sum());
    for (uint64_t j = 0; j < 2 * (size / 3); j++) {
        ASSERT_EQ(j % 2, n->at(j));
    }
    uint64_t start = 2 * (size / 3);
    uint64_t end = 5 * size / 6;
    for (uint64_t j = start; j < end; j++) {
        ASSERT_EQ((j - start) % 2, n->at(j));
    }
    start = end;
    end = 3 * (size / 3) - 1;
    for (uint64_t j = start; j < end; j++) {
        ASSERT_EQ((j - start - 1) % 2, n->at(j));
    }
    start = end;
    end = 5 * (start / 3) - 1;
    for (uint64_t j = start; j < end; j++) {
        ASSERT_EQ((j - start) % 2, n->at(j));
    }

    

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete(a);
}

#endif