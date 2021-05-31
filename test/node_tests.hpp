#ifndef TEST_NODE_HPP
#define TEST_NODE_HPP

#include <cstdint>
#include <iostream>

#include "../deps/googletest/googletest/include/gtest/gtest.h"

template <class node, class leaf, class alloc>
void node_add_child_test(uint64_t b) {
    alloc* a = new alloc();
    node* nd = a->template allocate_node<node>();
    nd->has_leaves(true);
    for (uint64_t i = 0; i < b; i++) {
        leaf* l = a->template allocate_leaf<leaf>(4);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, j % 2 == 0);
        }
        nd->append_child(l);
    }

    ASSERT_EQ(b, nd->child_count());
    ASSERT_EQ(b * 128, nd->size());
    ASSERT_EQ(b * 64, nd->p_sum());
    uint64_t* counts = nd->child_sizes();
    uint64_t* sums = nd->child_sums();
    for (uint64_t i = 0; i < b; i++) {
        ASSERT_EQ((i + 1) * 128, counts[i]);
        ASSERT_EQ((i + 1) * 64, sums[i]);
    }

    nd->template deallocate<alloc>();
    a->deallocate_node(nd);
    delete (a);
}

template <class node, class leaf, class alloc>
void node_add_node_test(uint64_t b) {
    alloc* a = new alloc();
    node* nd = a->template allocate_node<node>();
    nd->has_leaves();
    for (uint64_t i = 0; i < b; i++) {
        node* n = a->template allocate_node<node>();
        n->has_leaves(true);
        leaf* l = a->template allocate_leaf<leaf>(4);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, j % 2 == 0);
        }
        n->append_child(l);
        nd->append_child(n);
    }

    ASSERT_EQ(b, nd->child_count());
    ASSERT_EQ(b * 128, nd->size());
    ASSERT_EQ(b * 64, nd->p_sum());
    uint64_t* counts = nd->child_sizes();
    uint64_t* sums = nd->child_sums();
    for (uint64_t i = 0; i < b; i++) {
        ASSERT_EQ((i + 1) * 128, counts[i]);
        ASSERT_EQ((i + 1) * 64, sums[i]);
    }

    nd->template deallocate<alloc>();
    a->deallocate_node(nd);
    delete (a);
}

template <class node, class leaf, class alloc>
void node_split_leaves_test(uint64_t b) {
    alloc* a = new alloc();
    node* nd = a->template allocate_node<node>();
    nd->has_leaves(true);
    for (uint64_t i = 0; i < b; i++) {
        leaf* l = a->template allocate_leaf<leaf>(4);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, j % 2 == 0);
        }
        nd->append_child(l);
    }

    node* sibling = nd->template split<alloc>();

    ASSERT_EQ(true, nd->has_leaves());
    ASSERT_EQ(b / 2, nd->child_count());
    ASSERT_EQ(b / 2, sibling->child_count());
    ASSERT_EQ(b / 2 * 128, nd->size());
    ASSERT_EQ(b / 2 * 64, nd->p_sum());
    uint64_t* counts = nd->child_sizes();
    uint64_t* sums = nd->child_sums();
    for (uint64_t i = 0; i < b / 2; i++) {
        ASSERT_EQ((i + 1) * 128, counts[i]);
        ASSERT_EQ((i + 1) * 64, sums[i]);
    }

    counts = sibling->child_sizes();
    sums = sibling->child_sums();
    for (uint64_t i = 0; i < b / 2; i++) {
        ASSERT_EQ((i + 1) * 128, counts[i]);
        ASSERT_EQ((i + 1) * 64, sums[i]);
    }

    sibling->template deallocate<alloc>();
    nd->template deallocate<alloc>();
    a->deallocate_node(nd);
    a->deallocate_node(sibling);
    delete (a);
}

template <class node, class leaf, class alloc>
void node_split_nodes_test(uint64_t b) {
    alloc* a = new alloc();
    node* nd = a->template allocate_node<node>();
    for (uint64_t i = 0; i < b; i++) {
        node* n = a->template allocate_node<node>();
        n->has_leaves(true);
        for (uint64_t j = 0; j < 2 * (b / 3); j++) {
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
    ASSERT_EQ(b / 2, nd->child_count());
    ASSERT_EQ(b / 2, sibling->child_count());
    ASSERT_EQ(b / 2 * 128 * 2 * (b / 3), nd->size());
    ASSERT_EQ(b / 2 * 64 * 2 * (b / 3), nd->p_sum());
    ASSERT_EQ(b / 2 * 128 * 2 * (b / 3), sibling->size());
    ASSERT_EQ(b / 2 * 64 * 2 * (b / 3), sibling->p_sum());
    uint64_t* counts = nd->child_sizes();
    uint64_t* sums = nd->child_sums();
    for (uint64_t i = 0; i < b / 2; i++) {
        ASSERT_EQ((i + 1) * 128 * 2 * (b / 3), counts[i]);
        ASSERT_EQ((i + 1) * 64 * 2 * (b / 3), sums[i]);
    }

    counts = sibling->child_sizes();
    sums = sibling->child_sums();
    for (uint64_t i = 0; i < b / 2; i++) {
        ASSERT_EQ((i + 1) * 128 * 2 * (b / 3), counts[i]);
        ASSERT_EQ((i + 1) * 64 * 2 * (b / 3), sums[i]);
    }

    sibling->template deallocate<alloc>();
    nd->template deallocate<alloc>();
    a->deallocate_node(nd);
    a->deallocate_node(sibling);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class node, class leaf, class alloc>
void node_append_all_test(uint64_t b) {
    alloc* a = new alloc();
    node* n1 = a->template allocate_node<node>();
    node* n2 = a->template allocate_node<node>();
    n1->has_leaves(true);
    n2->has_leaves(true);
    for (uint64_t i = 0; i < b / 3; i++) {
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
    ASSERT_EQ(b / 3 * 128, n1->size());
    ASSERT_EQ(b / 3, n1->child_count());
    ASSERT_EQ(b / 3 * 64, n2->p_sum());
    ASSERT_EQ(b / 3 * 128, n2->size());
    ASSERT_EQ(b / 3, n2->child_count());

    n1->append_all(n2);

    ASSERT_EQ(b / 3 * 64, n1->p_sum());
    ASSERT_EQ(2 * (b / 3) * 128, n1->size());
    ASSERT_EQ(2 * (b / 3), n1->child_count());

    uint64_t* p_sums = n1->child_sums();
    uint64_t* sizes = n1->child_sizes();
    for (uint64_t i = 0; i < 2 * (b / 3); i++) {
        ASSERT_EQ(i >= (b / 3) ? (i - (b / 3 - 1)) * 64 : 0, p_sums[i]);
        ASSERT_EQ((i + 1) * 128, sizes[i]);
    }

    n1->template deallocate<alloc>();
    a->deallocate_node(n1);
    a->deallocate_node(n2);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class node, class leaf, class alloc>
void node_clear_last_test(uint64_t b) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    leaf* leaves[b - 2];
    for (uint64_t i = 0; i < b - 2; i++) {
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, j % 2 == 1);
        }
        leaves[i] = l;
        n->append_child(l);
    }
    ASSERT_EQ((b - 2) * 64, n->p_sum());
    ASSERT_EQ((b - 2) * 128, n->size());
    ASSERT_EQ((b - 2), n->child_count());

    n->clear_last(b / 2);

    ASSERT_EQ((b - 2 - b / 2) * 64, n->p_sum());
    ASSERT_EQ((b - 2 - b / 2) * 128, n->size());
    ASSERT_EQ((b - 2 - b / 2), n->child_count());

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(b / 2, a->live_allocations());
    for (uint64_t i = b - 2 - b / 2; i < b - 2; i++) {
        a->deallocate_leaf(leaves[i]);
    }
    delete (a);
}

template <class node, class leaf, class alloc>
void node_transfer_prepend_test(uint64_t b) {
    alloc* a = new alloc();
    node* n1 = a->template allocate_node<node>();
    node* n2 = a->template allocate_node<node>();
    n1->has_leaves(true);
    n2->has_leaves(true);
    for (uint64_t i = 0; i < b - 2; i++) {
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, j % 2 == 1);
        }
        n1->append_child(l);
    }
    for (uint64_t i = 0; i < b / 3; i++) {
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, j % 2 == 1);
        }
        n2->append_child(l);
    }
    ASSERT_EQ((b - 2) * 64, n1->p_sum());
    ASSERT_EQ((b - 2) * 128, n1->size());
    ASSERT_EQ(b - 2, n1->child_count());
    ASSERT_EQ((b / 3) * 64, n2->p_sum());
    ASSERT_EQ((b / 3) * 128, n2->size());
    ASSERT_EQ((b / 3), n2->child_count());

    n2->transfer_prepend(n1, b / 2);

    ASSERT_EQ((b - 2 - b / 2) * 64, n1->p_sum());
    ASSERT_EQ((b - 2 - b / 2) * 128, n1->size());
    ASSERT_EQ(b - 2 - b / 2, n1->child_count());
    ASSERT_EQ((b / 3 + b / 2) * 64, n2->p_sum());
    ASSERT_EQ((b / 3 + b / 2) * 128, n2->size());
    ASSERT_EQ(b / 3 + b / 2, n2->child_count());

    n1->template deallocate<alloc>();
    n2->template deallocate<alloc>();
    a->deallocate_node(n1);
    a->deallocate_node(n2);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class node, class leaf, class alloc>
void node_clear_first_test(uint64_t b) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    leaf* leaves[b - 2];
    for (uint64_t i = 0; i < b - 2; i++) {
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, j % 2 == 1);
        }
        leaves[i] = l;
        n->append_child(l);
    }
    ASSERT_EQ((b - 2) * 64, n->p_sum());
    ASSERT_EQ((b - 2) * 128, n->size());
    ASSERT_EQ(b - 2, n->child_count());

    n->clear_first(b / 2);

    ASSERT_EQ((b - 2 - b / 2) * 64, n->p_sum());
    ASSERT_EQ((b - 2 - b / 2) * 128, n->size());
    ASSERT_EQ(b - 2 - b / 2, n->child_count());

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(b / 2, a->live_allocations());
    for (uint64_t i = 0; i < b / 2; i++) {
        a->deallocate_leaf(leaves[i]);
    }
    delete (a);
}

template <class node, class leaf, class alloc>
void node_transfer_append_test(uint64_t b) {
    alloc* a = new alloc();
    node* n1 = a->template allocate_node<node>();
    node* n2 = a->template allocate_node<node>();
    n1->has_leaves(true);
    n2->has_leaves(true);
    for (uint64_t i = 0; i < b / 3; i++) {
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, j % 2 == 1);
        }
        n1->append_child(l);
    }
    for (uint64_t i = 0; i < b - 2; i++) {
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, j % 2 == 1);
        }
        n2->append_child(l);
    }

    ASSERT_EQ((b / 3) * 64, n1->p_sum());
    ASSERT_EQ((b / 3) * 128, n1->size());
    ASSERT_EQ(b / 3, n1->child_count());
    ASSERT_EQ((b - 2) * 64, n2->p_sum());
    ASSERT_EQ((b - 2) * 128, n2->size());
    ASSERT_EQ(b - 2, n2->child_count());

    n1->transfer_append(n2, b / 2);

    ASSERT_EQ((b / 2 + b / 3) * 64, n1->p_sum());
    ASSERT_EQ((b / 2 + b / 3) * 128, n1->size());
    ASSERT_EQ(b / 2 + b / 3, n1->child_count());
    ASSERT_EQ((b - 2 - b / 2) * 64, n2->p_sum());
    ASSERT_EQ((b - 2 - b / 2) * 128, n2->size());
    ASSERT_EQ(b - 2 - b / 2, n2->child_count());

    n1->template deallocate<alloc>();
    n2->template deallocate<alloc>();
    a->deallocate_node(n1);
    a->deallocate_node(n2);
    ASSERT_EQ(0u, a->live_allocations());
}

template <class node, class leaf, class alloc>
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
    delete (a);
}

template <class node, class leaf, class alloc>
void node_access_leaf_test(uint64_t b) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    for (uint64_t i = 0; i < b / 3; i++) {
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, false);
        }
        n->append_child(l);
    }
    for (uint64_t i = 0; i < b / 3; i++) {
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, true);
        }
        n->append_child(l);
    }
    for (uint64_t i = 0; i < b / 3; i++) {
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, j % 2 != 0);
        }
        n->append_child(l);
    }
    ASSERT_EQ(3 * (b / 3) * 64, n->p_sum());
    ASSERT_EQ(3 * (b / 3) * 128, n->size());
    ASSERT_EQ(3 * (b / 3), n->child_count());

    for (uint64_t i = 0; i < (b / 3) * 128; i++) {
        ASSERT_EQ(false, n->at(i));
    }
    for (uint64_t i = (b / 3) * 128; i < 2 * (b / 3) * 128; i++) {
        ASSERT_EQ(true, n->at(i)) << "i = " << i;
    }
    for (uint64_t i = 2 * (b / 3) * 128; i < 3 * (b / 3) * 128; i++) {
        ASSERT_EQ(i % 2 == 0, n->at(i));
    }
    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class node, class leaf, class alloc>
void node_access_node_test(uint64_t b) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    for (uint64_t i = 0; i < b / 3; i++) {
        node* c = a->template allocate_node<node>();
        c->has_leaves(true);
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, false);
        }
        c->append_child(l);
        n->append_child(c);
    }
    for (uint64_t i = 0; i < b / 3; i++) {
        node* c = a->template allocate_node<node>();
        c->has_leaves(true);
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, true);
        }
        c->append_child(l);
        n->append_child(c);
    }
    for (uint64_t i = 0; i < b / 3; i++) {
        node* c = a->template allocate_node<node>();
        c->has_leaves(true);
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, j % 2 != 0);
        }
        c->append_child(l);
        n->append_child(c);
    }
    ASSERT_EQ(3 * (b / 3) * 64, n->p_sum());
    ASSERT_EQ(3 * (b / 3) * 128, n->size());
    ASSERT_EQ(3 * (b / 3), n->child_count());

    for (uint64_t i = 0; i < (b / 3) * 128; i++) {
        ASSERT_EQ(false, n->at(i));
    }
    for (uint64_t i = (b / 3) * 128; i < 2 * (b / 3) * 128; i++) {
        ASSERT_EQ(true, n->at(i)) << "i = " << i;
    }
    for (uint64_t i = 2 * (b / 3) * 128; i < 3 * (b / 3) * 128; i++) {
        ASSERT_EQ(i % 2 == 0, n->at(i));
    }
    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class node, class leaf, class alloc>
void node_set_leaf_test(uint64_t b) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    for (uint64_t i = 0; i < b / 3; i++) {
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, false);
        }
        n->append_child(l);
    }
    ASSERT_EQ((b / 3) * 0, n->p_sum());
    ASSERT_EQ((b / 3) * 128, n->size());
    ASSERT_EQ(b / 3, n->child_count());

    for (uint64_t i = 0; i < b / 3; i++) {
        for (uint64_t j = 0; j < 128; j++) {
            n->set(i * 128 + j, (j % 2) ^ (i % 2));
        }
    }

    ASSERT_EQ((b / 3) * 64, n->p_sum());
    ASSERT_EQ((b / 3) * 128, n->size());
    ASSERT_EQ(b / 3, n->child_count());

    for (uint64_t i = 0; i < b / 3; i++) {
        for (uint64_t j = 0; j < 128; j++) {
            ASSERT_EQ((j % 2) ^ (i % 2), n->at(i * 128 + j))
                << "i = " << i << ", j = " << j;
        }
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class node, class leaf, class alloc>
void node_set_node_test(uint64_t b) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    for (uint64_t i = 0; i < b / 3; i++) {
        node* c = a->template allocate_node<node>();
        c->has_leaves(true);
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, false);
        }
        c->append_child(l);
        n->append_child(c);
    }

    ASSERT_EQ((b / 3) * 0, n->p_sum());
    ASSERT_EQ((b / 3) * 128, n->size());
    ASSERT_EQ(b / 3, n->child_count());

    for (uint64_t i = 0; i < b / 3; i++) {
        for (uint64_t j = 0; j < 128; j++) {
            n->set(i * 128 + j, (j % 2) ^ (i % 2));
        }
    }

    ASSERT_EQ((b / 3) * 64, n->p_sum());
    ASSERT_EQ((b / 3) * 128, n->size());
    ASSERT_EQ(b / 3, n->child_count());

    for (uint64_t i = 0; i < b / 3; i++) {
        for (uint64_t j = 0; j < 128; j++) {
            ASSERT_EQ((j % 2) ^ (i % 2), n->at(i * 128 + j))
                << "i = " << i << ", j = " << j;
        }
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class node, class leaf, class alloc>
void node_rank_leaf_test(uint64_t b) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    for (uint64_t i = 0; i < b / 3; i++) {
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, j % 2);
        }
        n->append_child(l);
    }
    ASSERT_EQ((b / 3) * 64, n->p_sum());
    ASSERT_EQ((b / 3) * 128, n->size());
    ASSERT_EQ(b / 3, n->child_count());

    for (uint64_t i = 0; i <= (b / 3) * 128; i++) {
        ASSERT_EQ((1 + i) / 2, n->rank(i));
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class node, class leaf, class alloc>
void node_rank_leafb_test(uint64_t b) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    for (uint64_t i = 0; i < b / 3; i++) {
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, (j % 2) ^ (i % 2));
        }
        n->append_child(l);
    }
    ASSERT_EQ((b / 3) * 64, n->p_sum());
    ASSERT_EQ((b / 3) * 128, n->size());
    ASSERT_EQ(b / 3, n->child_count());

    for (uint64_t i = 0; i < b / 3; i++) {
        uint64_t partial = i * 64;
        uint64_t start = i % 2 == 0;
        for (uint64_t j = 0; j <= 128; j++) {
            ASSERT_EQ(partial + ((j + start) >> 1), n->rank(i * 128 + j));
        }
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class node, class leaf, class alloc>
void node_rank_node_test(uint64_t b) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    for (uint64_t i = 0; i < b / 3; i++) {
        node* c = a->template allocate_node<node>();
        c->has_leaves(true);
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, (j % 2) ^ (i % 2));
        }
        c->append_child(l);
        n->append_child(c);
    }
    ASSERT_EQ((b / 3) * 64, n->p_sum());
    ASSERT_EQ((b / 3) * 128, n->size());
    ASSERT_EQ(b / 3, n->child_count());

    for (uint64_t i = 0; i < b / 3; i++) {
        uint64_t partial = i * 64;
        uint64_t start = i % 2 == 0;
        for (uint64_t j = 0; j < 128; j++) {
            ASSERT_EQ(partial + ((j + start) >> 1), n->rank(i * 128 + j));
        }
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class node, class leaf, class alloc>
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
        ASSERT_EQ((j - 1) * 2, n->select(j));
    }

    for (uint64_t j = 0; j < 128; j++) {
        l->set(j, j % 2);
    }

    ASSERT_EQ(1u * 64, n->p_sum());
    ASSERT_EQ(1u * 128, n->size());
    ASSERT_EQ(1u, n->child_count());

    for (uint64_t j = 1; j <= 64; j++) {
        ASSERT_EQ(1 + (j - 1) * 2, n->select(j));
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class node, class leaf, class alloc>
void node_select_leaf_test(uint64_t b) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    for (uint64_t i = 0; i < b / 3; i++) {
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, (j % 2) ^ (i % 2));
        }
        n->append_child(l);
    }

    ASSERT_EQ((b / 3) * 64, n->p_sum());
    ASSERT_EQ((b / 3) * 128, n->size());
    ASSERT_EQ(b / 3, n->child_count());
    for (uint64_t i = 0; i < b / 3; i++) {
        uint64_t partial = i * 64;
        uint64_t plus = i % 2 != 0;
        for (uint64_t j = 1; j <= 64; j++) {
            ASSERT_EQ(partial * 2 + plus + (j - 1) * 2, n->select(partial + j));
        }
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class node, class leaf, class alloc>
void node_select_node_test(uint64_t b) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    for (uint64_t i = 0; i < b / 3; i++) {
        node* c = a->template allocate_node<node>();
        c->has_leaves(true);
        leaf* l = a->template allocate_leaf<leaf>(3);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, (j % 2) ^ (i % 2));
        }
        c->append_child(l);
        n->append_child(c);
    }

    ASSERT_EQ((b / 3) * 64, n->p_sum());
    ASSERT_EQ((b / 3) * 128, n->size());
    ASSERT_EQ(b / 3, n->child_count());
    for (uint64_t i = 0; i < b / 3; i++) {
        uint64_t partial = i * 64;
        uint64_t plus = i % 2 != 0;
        for (uint64_t j = 1; j <= 64; j++) {
            ASSERT_EQ(partial * 2 + plus + (j - 1) * 2, n->select(partial + j));
        }
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class node, class leaf, class alloc>
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
    delete (a);
}

template <class node, class leaf, class alloc>
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
    delete (a);
}

template <class node, class leaf, class alloc>
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
    delete (a);
}

template <class node, class leaf, class alloc>
void node_insert_leaf_split_test(uint64_t size) {
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
    delete (a);
}

template <class node, class leaf, class alloc>
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
    delete (a);
}

template <class node, class leaf, class alloc>
void node_insert_node_split_test(uint64_t b) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    for (uint64_t i = 0; i < 2; i++) {
        node* c = a->template allocate_node<node>();
        c->has_leaves(true);
        for (uint64_t j = 0; j < b; j++) {
            leaf* l = a->template allocate_leaf<leaf>(4);
            for (uint64_t j = 0; j < 128; j++) {
                l->insert(j, false);
            }
            c->append_child(l);
        }
        ASSERT_EQ(b, c->child_count());
        n->append_child(c);
    }

    ASSERT_EQ(2u, n->child_count());
    ASSERT_EQ(2u * 128 * b, n->size());
    ASSERT_EQ(2u * 0, n->p_sum());

    for (uint64_t i = 2u * 128 * b; i > 0; i--) {
        n->template insert<alloc>(i, true);
    }

    ASSERT_EQ(4u, n->child_count());
    ASSERT_EQ(4u * 128 * b, n->size());
    ASSERT_EQ(2u * 128 * b, n->p_sum());

    for (uint64_t i = 0; i < 4u * 128 * b; i++) {
        ASSERT_EQ(i % 2, n->at(i));
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class node, class leaf, class alloc>
void node_remove_leaf_simple_test(uint64_t size) {
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
    delete (a);
}

template <class node, class leaf, class alloc>
void node_remove_leaf_a_test(uint64_t size) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    leaf* l = a->template allocate_leaf<leaf>(size / 64);
    uint64_t m = 0;
    for (uint64_t j = 0; j < 2 * (size / 3); j++) {
        l->insert(j, m++ % 2);
    }
    n->append_child(l);

    l = a->template allocate_leaf<leaf>(size / (3 * 64) + 1);
    for (uint64_t j = 0; j < size / 3; j++) {
        l->insert(j, m++ % 2);
    }
    n->append_child(l);

    ASSERT_EQ(2u, n->child_count());
    ASSERT_EQ(m, n->size());
    ASSERT_EQ(m / 2, n->p_sum());

    n->template remove<alloc>(5 * size / 6);
    n->template remove<alloc>(5 * size / 6);

    ASSERT_EQ(2u, n->child_count());
    ASSERT_EQ(m - 2, n->size());
    ASSERT_EQ(m / 2 - 1, n->p_sum());
    for (uint64_t j = 0; j < m - 2; j++) {
        ASSERT_EQ(j % 2, n->at(j));
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class node, class leaf, class alloc>
void node_remove_leaf_b_test(uint64_t size) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    leaf* l = a->template allocate_leaf<leaf>(size / (3 * 64) + 1);
    uint64_t m = 0;
    for (uint64_t j = 0; j < (size / 3); j++) {
        l->insert(j, m++ % 2);
    }
    n->append_child(l);

    l = a->template allocate_leaf<leaf>(size / (3 * 64) + 1);
    for (uint64_t j = 0; j < size / 3; j++) {
        l->insert(j, m++ % 2);
    }
    n->append_child(l);

    ASSERT_EQ(2u, n->child_count());
    ASSERT_EQ(m, n->size());
    ASSERT_EQ(m / 2, n->p_sum());
    for (uint64_t j = 0; j < m; j++) {
        ASSERT_EQ(j % 2, n->at(j));
    }

    n->template remove<alloc>(size / 2);
    n->template remove<alloc>(size / 2);

    ASSERT_EQ(1u, n->child_count());
    ASSERT_EQ(m - 2, n->size());
    ASSERT_EQ(m / 2 - 1, n->p_sum());
    for (uint64_t j = 0; j < m - 2; j++) {
        ASSERT_EQ(j % 2, n->at(j));
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class node, class leaf, class alloc>
void node_remove_leaf_c_test(uint64_t size) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    uint64_t m = 0;
    leaf* l = a->template allocate_leaf<leaf>(size / 64);
    for (uint64_t j = 0; j < size; j++) {
        l->insert(j, m++ % 2);
    }
    n->append_child(l);

    l = a->template allocate_leaf<leaf>(size / 64);
    for (uint64_t j = 0; j < 2 * (size / 3); j++) {
        l->insert(j, m++ % 2);
    }
    n->append_child(l);

    l = a->template allocate_leaf<leaf>(size / (3 * 64) + 1);
    for (uint64_t j = 0; j < size / 3; j++) {
        l->insert(j, m++ % 2);
    }
    n->append_child(l);

    ASSERT_EQ(3u, n->child_count());
    ASSERT_EQ(m, n->size());
    ASSERT_EQ(m / 2, n->p_sum());

    n->template remove<alloc>(size + 5 * size / 6);
    n->template remove<alloc>(size + 5 * size / 6);

    ASSERT_EQ(3u, n->child_count());
    ASSERT_EQ(m - 2, n->size());
    ASSERT_EQ(m / 2 - 1, n->p_sum());
    for (uint64_t j = 0; j < m - 2; j++) {
        ASSERT_EQ(j % 2, n->at(j));
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class node, class leaf, class alloc>
void node_remove_leaf_d_test(uint64_t size) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    leaf* l = a->template allocate_leaf<leaf>(size / 64);
    uint64_t m = 0;
    for (uint64_t j = 0; j < size; j++) {
        l->insert(j, m++ % 2);
    }
    n->append_child(l);

    l = a->template allocate_leaf<leaf>(size / (3 * 64) + 1);
    for (uint64_t j = 0; j < (size / 3); j++) {
        l->insert(j, m++ % 2);
    }
    n->append_child(l);

    l = a->template allocate_leaf<leaf>(size / (3 * 64) + 1);
    for (uint64_t j = 0; j < size / 3; j++) {
        l->insert(j, m++ % 2);
    }
    n->append_child(l);

    ASSERT_EQ(3u, n->child_count());
    ASSERT_EQ(m, n->size());
    ASSERT_EQ(m / 2, n->p_sum());
    for (uint64_t j = 0; j < m; j++) {
        ASSERT_EQ(j % 2, n->at(j));
    }

    n->template remove<alloc>(size + size / 2);
    n->template remove<alloc>(size + size / 2);

    ASSERT_EQ(2u, n->child_count());
    ASSERT_EQ(m - 2, n->size());
    ASSERT_EQ(m / 2 - 1, n->p_sum());
    for (uint64_t j = 0; j < m - 2; j++) {
        ASSERT_EQ(j % 2, n->at(j));
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class node, class leaf, class alloc>
void node_remove_leaf_e_test(uint64_t size) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    leaf* l = a->template allocate_leaf<leaf>(size / (3 * 64) + 1);
    uint64_t m = 0;
    for (uint64_t j = 0; j < size / 3; j++) {
        l->insert(j, m++ % 2);
    }
    n->append_child(l);

    l = a->template allocate_leaf<leaf>(size / 64 + 1);
    for (uint64_t j = 0; j < 2 * (size / 3); j++) {
        l->insert(j, m++ % 2);
    }
    n->append_child(l);

    ASSERT_EQ(2u, n->child_count());
    ASSERT_EQ(m, n->size());
    ASSERT_EQ(m / 2, n->p_sum());

    n->template remove<alloc>(0);
    n->template remove<alloc>(0);

    ASSERT_EQ(2u, n->child_count());
    ASSERT_EQ(m - 2, n->size());
    ASSERT_EQ(m / 2 - 1, n->p_sum());
    for (uint64_t j = 0; j < m - 2; j++) {
        ASSERT_EQ(j % 2, n->at(j));
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class node, class leaf, class alloc>
void node_remove_leaf_f_test(uint64_t size) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    leaf* l = a->template allocate_leaf<leaf>(size / (3 * 64) + 1);
    uint64_t m = 0;
    for (uint64_t j = 0; j < size / 3; j++) {
        l->insert(j, m++ % 2);
    }
    n->append_child(l);

    l = a->template allocate_leaf<leaf>(size / (3 * 64) + 1);
    for (uint64_t j = 0; j < size / 3; j++) {
        l->insert(j, m++ % 2);
    }
    n->append_child(l);

    ASSERT_EQ(2u, n->child_count());
    ASSERT_EQ(m, n->size());
    ASSERT_EQ(m / 2, n->p_sum());
    for (uint64_t j = 0; j < m; j++) {
        ASSERT_EQ(j % 2, n->at(j));
    }

    n->template remove<alloc>(0);
    n->template remove<alloc>(0);

    ASSERT_EQ(1u, n->child_count());
    ASSERT_EQ(m - 2, n->size());
    ASSERT_EQ(m / 2 - 1, n->p_sum());
    for (uint64_t j = 0; j < m - 2; j++) {
        ASSERT_EQ(j % 2, n->at(j));
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class node, class leaf, class alloc>
void node_remove_leaf_g_test(uint64_t size) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    leaf* l = a->template allocate_leaf<leaf>(size / (2 * 64));
    n->append_child(l);

    uint64_t limit = 3 * size / 2;

    for (uint64_t j = 0; j < limit; j++) {
        n->template insert<alloc>(j, j % 2);
    }

    ASSERT_EQ(2u, n->child_count());
    ASSERT_EQ(limit, n->size());
    ASSERT_EQ(limit / 2, n->p_sum());
    for (uint64_t j = 0; j < limit; j++) {
        ASSERT_EQ(j % 2, n->at(j)) << "j = " << j;
    }

    for (uint64_t i = 0; i < 2 + size / 6; i++) {
        n->template remove<alloc>(0);
    }

    ASSERT_EQ(2u, n->child_count());
    ASSERT_EQ(limit - (2 + size / 6), n->size());
    ASSERT_EQ(limit / 2 - (2 + size / 6) / 2, n->p_sum());
    for (uint64_t j = 0; j < limit - (2 + size / 6); j++) {
        ASSERT_EQ(j % 2, n->at(j)) << "j = " << j;
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class node, class leaf, class alloc>
void node_remove_node_simple_test(uint64_t size, uint64_t b) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    for (uint64_t i = 0; i < 2; i++) {
        node* c = a->template allocate_node<node>();
        c->has_leaves(true);
        for (uint64_t i = 0; i < b; i++) {
            leaf* l = a->template allocate_leaf<leaf>(size / 64);
            for (uint64_t j = 0; j < size; j++) {
                l->insert(j, j % 2);
            }
            c->append_child(l);
        }
        n->append_child(c);
    }

    ASSERT_EQ(2u, n->child_count());
    ASSERT_EQ(2u * b * size, n->size());
    ASSERT_EQ(1u * b * size, n->p_sum());

    for (uint64_t i = 2u * b * size - 1; i < 2u * b * size; i -= 2) {
        n->template remove<alloc>(i);
    }

    ASSERT_EQ(2u, n->child_count());
    ASSERT_EQ(1u * b * size, n->size());
    ASSERT_EQ(1u * 0, n->p_sum());

    for (uint64_t i = 0; i < size / 2; i++) {
        ASSERT_EQ(false, n->at(i));
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class node, class leaf, class alloc>
void node_remove_node_a_test(uint64_t size, uint64_t b) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    node* c = a->template allocate_node<node>();
    c->has_leaves(true);
    for (uint64_t i = 0; i < b; i++) {
        leaf* l = a->template allocate_leaf<leaf>(size / 64);
        for (uint64_t j = 0; j < size; j++) {
            l->insert(j, j % 2);
        }
        c->append_child(l);
    }
    n->append_child(c);

    c = a->template allocate_node<node>();
    c->has_leaves(true);
    for (uint64_t i = 0; i < b / 3; i++) {
        leaf* l = a->template allocate_leaf<leaf>(size / 64);
        for (uint64_t j = 0; j < size; j++) {
            l->insert(j, j % 2);
        }
        c->append_child(l);
    }
    n->append_child(c);

    ASSERT_EQ(2u, n->child_count());
    ASSERT_EQ(b * size + (b / 3) * size, n->size());
    ASSERT_EQ(b / 2 * size + (b / 3) * size / 2, n->p_sum());

    n->template remove<alloc>(n->size() - 1);

    ASSERT_EQ(2u, n->child_count());
    ASSERT_EQ(b * size + (b / 3) * size - 1, n->size());
    ASSERT_EQ(b / 2 * size + (b / 3) * size / 2 - 1, n->p_sum());
    node* n1 = reinterpret_cast<node*>(n->child(0));
    node* n2 = reinterpret_cast<node*>(n->child(1));
    ASSERT_NEAR(n1->child_count(), n2->child_count(), 1);
    for (uint64_t j = 0; j < b * size + (b / 3) * size - 1; j++) {
        ASSERT_EQ(j % 2, n->at(j)) << "j = " << j;
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class node, class leaf, class alloc>
void node_remove_node_b_test(uint64_t size, uint64_t b) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    node* c = a->template allocate_node<node>();
    c->has_leaves(true);
    for (uint64_t i = 0; i < b / 3; i++) {
        leaf* l = a->template allocate_leaf<leaf>(size / 64);
        for (uint64_t j = 0; j < size; j++) {
            l->insert(j, j % 2);
        }
        c->append_child(l);
    }
    n->append_child(c);

    c = a->template allocate_node<node>();
    c->has_leaves(true);
    for (uint64_t i = 0; i < b / 3; i++) {
        leaf* l = a->template allocate_leaf<leaf>(size / 64);
        for (uint64_t j = 0; j < size; j++) {
            l->insert(j, j % 2);
        }
        c->append_child(l);
    }
    n->append_child(c);

    ASSERT_EQ(2u, n->child_count());
    ASSERT_EQ(2u * (b / 3) * size, n->size());
    ASSERT_EQ(1u * (b / 3) * size, n->p_sum());

    n->template remove<alloc>(n->size() - 1);

    ASSERT_EQ(1u, n->child_count());
    ASSERT_EQ(2u * (b / 3) * size - 1, n->size());
    ASSERT_EQ(1u * (b / 3) * size - 1, n->p_sum());
    for (uint64_t j = 0; j < 2u + (b / 3) * size - 1; j++) {
        ASSERT_EQ(j % 2, n->at(j)) << "j = " << j;
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class node, class leaf, class alloc>
void node_remove_node_c_test(uint64_t size, uint64_t b) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    node* c = a->template allocate_node<node>();
    c->has_leaves(true);
    for (uint64_t i = 0; i < b; i++) {
        leaf* l = a->template allocate_leaf<leaf>(size / 64);
        for (uint64_t j = 0; j < size; j++) {
            l->insert(j, j % 2);
        }
        c->append_child(l);
    }
    n->append_child(c);

    c = a->template allocate_node<node>();
    c->has_leaves(true);
    for (uint64_t i = 0; i < b; i++) {
        leaf* l = a->template allocate_leaf<leaf>(size / 64);
        for (uint64_t j = 0; j < size; j++) {
            l->insert(j, j % 2);
        }
        c->append_child(l);
    }
    n->append_child(c);

    c = a->template allocate_node<node>();
    c->has_leaves(true);
    for (uint64_t i = 0; i < b / 3; i++) {
        leaf* l = a->template allocate_leaf<leaf>(size / 64);
        for (uint64_t j = 0; j < size; j++) {
            l->insert(j, j % 2);
        }
        c->append_child(l);
    }
    n->append_child(c);

    ASSERT_EQ(3u, n->child_count());
    ASSERT_EQ(2u * b * size + (b / 3) * size, n->size());
    ASSERT_EQ(1u * b * size + (b / 3) * size / 2, n->p_sum());

    n->template remove<alloc>(n->size() - 1);

    ASSERT_EQ(3u, n->child_count());
    ASSERT_EQ(2u * b * size + (b / 3) * size - 1, n->size());
    ASSERT_EQ(1u * b * size + (b / 3) * size / 2 - 1, n->p_sum());
    node* n1 = reinterpret_cast<node*>(n->child(1));
    node* n2 = reinterpret_cast<node*>(n->child(2));
    ASSERT_NEAR(n1->child_count(), n2->child_count(), 1);
    for (uint64_t j = 0; j < 2u * b * size + (b / 3) * size - 1; j++) {
        ASSERT_EQ(j % 2, n->at(j)) << "j = " << j;
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class node, class leaf, class alloc>
void node_remove_node_d_test(uint64_t size, uint64_t b) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    node* c = a->template allocate_node<node>();
    c->has_leaves(true);
    for (uint64_t i = 0; i < b / 3; i++) {
        leaf* l = a->template allocate_leaf<leaf>(size / 64);
        for (uint64_t j = 0; j < size; j++) {
            l->insert(j, j % 2);
        }
        c->append_child(l);
    }
    n->append_child(c);

    c = a->template allocate_node<node>();
    c->has_leaves(true);
    for (uint64_t i = 0; i < b; i++) {
        leaf* l = a->template allocate_leaf<leaf>(size / 64);
        for (uint64_t j = 0; j < size; j++) {
            l->insert(j, j % 2);
        }
        c->append_child(l);
    }
    n->append_child(c);

    ASSERT_EQ(2u, n->child_count());
    ASSERT_EQ(b * size + (b / 3) * size, n->size());
    ASSERT_EQ((b * size + (b / 3) * size) / 2, n->p_sum());

    n->template remove<alloc>(0);

    ASSERT_EQ(2u, n->child_count());
    ASSERT_EQ(b * size + (b / 3) * size - 1, n->size());
    ASSERT_EQ((b * size + (b / 3) * size) / 2, n->p_sum());
    node* n1 = reinterpret_cast<node*>(n->child(0));
    node* n2 = reinterpret_cast<node*>(n->child(1));
    ASSERT_NEAR(n1->child_count(), n2->child_count(), 1);
    for (uint64_t j = 0; j < b * size + (b / 3) * size - 1; j++) {
        ASSERT_EQ(j % 2 == 0, n->at(j)) << "j = " << j;
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class node, class leaf, class alloc>
void node_remove_node_e_test(uint64_t size, uint64_t b) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    node* c = a->template allocate_node<node>();
    c->has_leaves(true);
    for (uint64_t i = 0; i < b / 3; i++) {
        leaf* l = a->template allocate_leaf<leaf>(size / 64);
        for (uint64_t j = 0; j < size; j++) {
            l->insert(j, j % 2);
        }
        c->append_child(l);
    }
    n->append_child(c);

    c = a->template allocate_node<node>();
    c->has_leaves(true);
    for (uint64_t i = 0; i < b / 3; i++) {
        leaf* l = a->template allocate_leaf<leaf>(size / 64);
        for (uint64_t j = 0; j < size; j++) {
            l->insert(j, j % 2);
        }
        c->append_child(l);
    }
    n->append_child(c);

    ASSERT_EQ(2u, n->child_count());
    ASSERT_EQ(2u * (b / 3) * size, n->size());
    ASSERT_EQ(1u * (b / 3) * size, n->p_sum());

    n->template remove<alloc>(0);

    ASSERT_EQ(1u, n->child_count());
    ASSERT_EQ(2u * (b / 3) * size - 1, n->size());
    ASSERT_EQ(1u * (b / 3) * size, n->p_sum());
    for (uint64_t j = 0; j < 2u + (b / 3) * size - 1; j++) {
        ASSERT_EQ(j % 2 == 0, n->at(j)) << "j = " << j;
    }

    n->template deallocate<alloc>();
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

#endif