#ifndef TEST_NODE_HPP
#define TEST_NODE_HPP

#include <cstdint>
#include <iostream>

#include "../deps/googletest/googletest/include/gtest/gtest.h"

template <class node, class leaf, class alloc>
void node_add_child_test(uint64_t b) {
    alloc* a = new alloc();
    node* ndl = a->template allocate_node<node>();
    ndl->has_leaves(true);
    for (uint64_t i = 0; i < b; i++) {
        leaf* l = a->template allocate_leaf<leaf>(4);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, j % 2 == 0);
        }
        ndl->append_child(l);
    }

    ASSERT_EQ(b, ndl->child_count());
    ASSERT_EQ(b * 128, ndl->size());
    ASSERT_EQ(b * 64, ndl->p_sum());
    auto* counts = ndl->child_sizes();
    auto* sums = ndl->child_sums();
    for (uint64_t i = 0; i < b; i++) {
        ASSERT_EQ((i + 1) * 128, counts->get(i));
        ASSERT_EQ((i + 1) * 64, sums->get(i));
    }

    ndl->deallocate(a);
    a->deallocate_node(ndl);
    delete (a);
}

template <class node, class leaf, class alloc>
void node_add_node_test(uint64_t b) {
    alloc* a = new alloc();
    node* ndl = a->template allocate_node<node>();
    ndl->has_leaves();
    for (uint64_t i = 0; i < b; i++) {
        node* n = a->template allocate_node<node>();
        n->has_leaves(true);
        leaf* l = a->template allocate_leaf<leaf>(4);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, j % 2 == 0);
        }
        n->append_child(l);
        ndl->append_child(n);
    }

    ASSERT_EQ(b, ndl->child_count());
    ASSERT_EQ(b * 128, ndl->size());
    ASSERT_EQ(b * 64, ndl->p_sum());
    auto* counts = ndl->child_sizes();
    auto* sums = ndl->child_sums();
    for (uint64_t i = 0; i < b; i++) {
        ASSERT_EQ((i + 1) * 128, counts->get(i));
        ASSERT_EQ((i + 1) * 64, sums->get(i));
    }

    ndl->deallocate(a);
    a->deallocate_node(ndl);
    delete (a);
}

template <class node, class leaf, class alloc>
void node_split_leaves_test(uint64_t b) {
    alloc* a = new alloc();
    node* ndl = a->template allocate_node<node>();
    ndl->has_leaves(true);
    for (uint64_t i = 0; i < b; i++) {
        leaf* l = a->template allocate_leaf<leaf>(4);
        for (uint64_t j = 0; j < 128; j++) {
            l->insert(0, j % 2 == 0);
        }
        ndl->append_child(l);
    }

    node* sibling = ndl->split(a);

    ASSERT_EQ(true, ndl->has_leaves());
    ASSERT_EQ(b / 2, ndl->child_count());
    ASSERT_EQ(b / 2, sibling->child_count());
    ASSERT_EQ(b / 2 * 128, ndl->size());
    ASSERT_EQ(b / 2 * 64, ndl->p_sum());
    auto* counts = ndl->child_sizes();
    auto* sums = ndl->child_sums();
    for (uint64_t i = 0; i < b / 2; i++) {
        ASSERT_EQ((i + 1) * 128, counts->get(i));
        ASSERT_EQ((i + 1) * 64, sums->get(i));
    }

    counts = sibling->child_sizes();
    sums = sibling->child_sums();
    for (uint64_t i = 0; i < b / 2; i++) {
        ASSERT_EQ((i + 1) * 128, counts->get(i));
        ASSERT_EQ((i + 1) * 64, sums->get(i));
    }

    sibling->deallocate(a);
    ndl->deallocate(a);
    a->deallocate_node(ndl);
    a->deallocate_node(sibling);
    delete (a);
}

template <class node, class leaf, class alloc>
void node_split_nodes_test(uint64_t b) {
    alloc* a = new alloc();
    node* ndl = a->template allocate_node<node>();
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
        ndl->append_child(n);
    }

    node* sibling = ndl->split(a);

    ASSERT_EQ(false, ndl->has_leaves());
    ASSERT_EQ(b / 2, ndl->child_count());
    ASSERT_EQ(b / 2, sibling->child_count());
    ASSERT_EQ(b / 2 * 128 * 2 * (b / 3), ndl->size());
    ASSERT_EQ(b / 2 * 64 * 2 * (b / 3), ndl->p_sum());
    ASSERT_EQ(b / 2 * 128 * 2 * (b / 3), sibling->size());
    ASSERT_EQ(b / 2 * 64 * 2 * (b / 3), sibling->p_sum());
    auto* counts = ndl->child_sizes();
    auto* sums = ndl->child_sums();
    for (uint64_t i = 0; i < b / 2; i++) {
        ASSERT_EQ((i + 1) * 128 * 2 * (b / 3), counts->get(i));
        ASSERT_EQ((i + 1) * 64 * 2 * (b / 3), sums->get(i));
    }

    counts = sibling->child_sizes();
    sums = sibling->child_sums();
    for (uint64_t i = 0; i < b / 2; i++) {
        ASSERT_EQ((i + 1) * 128 * 2 * (b / 3), counts->get(i));
        ASSERT_EQ((i + 1) * 64 * 2 * (b / 3), sums->get(i));
    }

    sibling->deallocatealloc>(a);
    ndl->deallocate(a);
    a->deallocate_node(ndl);
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

    auto* p_sums = n1->child_sums();
    auto* sizes = n1->child_sizes();
    for (uint64_t i = 0; i < 2 * (b / 3); i++) {
        ASSERT_EQ(i >= (b / 3) ? (i - (b / 3 - 1)) * 64 : 0, p_sums->get(i));
        ASSERT_EQ((i + 1) * 128, sizes->get(i));
    }

    n1->deallocate(a);
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
    leaf** leaves = (leaf**)malloc((b - 2) * sizeof(leaf));
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

    n->deallocate(a);
    a->deallocate_node(n);
    ASSERT_EQ(b / 2, a->live_allocations());
    for (uint64_t i = b - 2 - b / 2; i < b - 2; i++) {
        a->deallocate_leaf(leaves[i]);
    }
    delete(a);
    free(leaves);
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

    n1->deallocate(a);
    n2->deallocate(a);
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
    leaf** leaves = (leaf**)malloc((b - 2) * sizeof(leaf));
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

    n->deallocate(a);
    a->deallocate_node(n);
    ASSERT_EQ(b / 2, a->live_allocations());
    for (uint64_t i = 0; i < b / 2; i++) {
        a->deallocate_leaf(leaves[i]);
    }
    delete (a);
    free(leaves);
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

    n1->deallocate(a);
    n2->deallocate(a);
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
    n->deallocate(a);
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
    n->deallocate(a);
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
    n->deallocate(a);
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
            n->set(i * 128 + j, (j % 2) ^ (i % 2), a);
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

    n->deallocate(a);
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
            n->set(i * 128 + j, (j % 2) ^ (i % 2), a);
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

    n->deallocate(a);
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

    n->deallocate(a);
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

    n->deallocate(a);
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

    n->deallocate(a);
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

    n->deallocate(a);
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

    n->deallocate(a);
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

    n->deallocate(a);
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
        n->insert(0, i % 2, a);
    }

    ASSERT_EQ(1u * 64, n->p_sum());
    ASSERT_EQ(1u * 128, n->size());
    ASSERT_EQ(1u, n->child_count());

    for (uint64_t i = 0; i < 128; i++) {
        ASSERT_EQ(i % 2 == 0, n->at(i));
    }

    n->deallocate(a);
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
        n->insert(0, i % 2, a);
    }

    ASSERT_EQ(1u * 64, n->p_sum());
    ASSERT_EQ(1u * 128, n->size());
    ASSERT_EQ(1u, n->child_count());

    for (uint64_t i = 0; i < 128; i++) {
        ASSERT_EQ(i % 2 == 0, n->at(i));
    }

    n->deallocate(a);
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class node, class leaf, class alloc>
void node_insert_single_leaf_split_test(uint64_t size) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    for (uint8_t k = 0; k < 2; k++) {
        leaf* l = a->template allocate_leaf<leaf>(size / 64);
        for (uint64_t i = 0; i < size - 2; i++) {
            l->insert(i, i % 2);
        }
        n->append_child(l);
    }

    ASSERT_EQ(size - 2, n->p_sum());
    ASSERT_EQ(size * 2 - 4, n->size());
    ASSERT_EQ(2u, n->child_count());

    for (uint64_t i = 0; i < 2 * size - 4; i++) {
        ASSERT_EQ(i % 2, n->at(i)) << "i = " << i;
    }

    for (uint64_t i = 0; i < 4; i++) {
        n->insert(i, i % 2, a);
    }

    ASSERT_EQ(size, n->p_sum());
    ASSERT_EQ(size * 2, n->size());
    ASSERT_EQ(3u, n->child_count());

    for (uint64_t i = 0; i < 2 * size; i++) {
        ASSERT_EQ(i % 2, n->at(i)) << "i = " << i;
    }

    n->deallocate(a);
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

    n->insert(size + 5, true, a);

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

    n->deallocate(a);
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class node, class leaf, class alloc>
void node_insert_leaf_rebalance_test(uint64_t size) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    for (uint64_t i = 0; i < 2; i++) {
        leaf* l = a->template allocate_leaf<leaf>(size / (2 * 64));
        for (uint64_t j = 0; j < size / 2; j++) {
            l->insert(j, j % 2);
        }
        n->append_child(l);
    }

    ASSERT_EQ(size / 2, n->p_sum());
    ASSERT_EQ(size, n->size());
    ASSERT_EQ(2u, n->child_count());

    for (uint64_t i = 0; i < size; i++) {
        ASSERT_EQ(i % 2, n->at(i)) << "i = " << i;
    }

    for (uint64_t i = 0; i < size; i++) {
        n->insert(0, i % 2, a);
    }

    ASSERT_EQ(size, n->p_sum());
    ASSERT_EQ(2 * size, n->size());
    ASSERT_EQ(3u, n->child_count());

    for (uint64_t i = 0; i < size; i++) {
        ASSERT_EQ((i + 1) % 2, n->at(i)) << "i = " << i;
    }

    for (uint64_t i = size; i < 2 * size; i++) {
        ASSERT_EQ(i % 2, n->at(i)) << "i = " << i;
    }

    n->deallocate(a);
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete(a);
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
        n->insert(i, true, a);
    }

    ASSERT_EQ(6u, n->child_count());
    ASSERT_EQ(6u * 128 * 2, n->size());
    ASSERT_EQ(6u * 128, n->p_sum());

    for (uint64_t i = 0; i < 6u * 128 * 2; i++) {
        ASSERT_EQ(i % 2, n->at(i));
    }

    n->deallocate(a);
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
            for (uint64_t k = 0; k < 128; k++) {
                l->insert(k, false);
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
        n->insert(i, true, a);
    }

    ASSERT_EQ(3u, n->child_count());
    ASSERT_EQ(4u * 128 * b, n->size());
    ASSERT_EQ(2u * 128 * b, n->p_sum());

    for (uint64_t i = 0; i < 4u * 128 * b; i++) {
        ASSERT_EQ(i % 2, n->at(i));
    }

    n->deallocate(a);
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
        n->remove(i, a);
    }

    n->validate();

    ASSERT_EQ(2u, n->child_count());
    ASSERT_EQ(1u * size, n->size());
    ASSERT_EQ(1u * 0, n->p_sum());

    for (uint64_t i = 0; i < size / 2; i++) {
        ASSERT_EQ(false, n->at(i));
    }

    n->deallocate(a);
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

    n->remove(5 * size / 6, a);
    n->remove(5 * size / 6, a);

    ASSERT_EQ(2u, n->child_count());
    ASSERT_EQ(m - 2, n->size());
    ASSERT_EQ(m / 2 - 1, n->p_sum());
    for (uint64_t j = 0; j < m - 2; j++) {
        ASSERT_EQ(j % 2, n->at(j));
    }

    n->deallocate(a);
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

    n->remove(size / 2, a);
    n->remove(size / 2, a);

    ASSERT_EQ(1u, n->child_count());
    ASSERT_EQ(m - 2, n->size());
    ASSERT_EQ(m / 2 - 1, n->p_sum());
    for (uint64_t j = 0; j < m - 2; j++) {
        ASSERT_EQ(j % 2, n->at(j));
    }

    n->deallocate(a);
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

    n->remove(size + 5 * size / 6, a);
    n->remove(size + 5 * size / 6, a);

    ASSERT_EQ(3u, n->child_count());
    ASSERT_EQ(m - 2, n->size());
    ASSERT_EQ(m / 2 - 1, n->p_sum());
    for (uint64_t j = 0; j < m - 2; j++) {
        ASSERT_EQ(j % 2, n->at(j));
    }

    n->deallocate(a);
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

    n->remove(size + size / 2, a);
    n->remove(size + size / 2, a);

    ASSERT_EQ(2u, n->child_count());
    ASSERT_EQ(m - 2, n->size());
    ASSERT_EQ(m / 2 - 1, n->p_sum());
    for (uint64_t j = 0; j < m - 2; j++) {
        ASSERT_EQ(j % 2, n->at(j));
    }

    n->deallocate(a);
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

    n->remove(0, a);
    n->remove(0, a);

    ASSERT_EQ(2u, n->child_count());
    ASSERT_EQ(m - 2, n->size());
    ASSERT_EQ(m / 2 - 1, n->p_sum());
    for (uint64_t j = 0; j < m - 2; j++) {
        ASSERT_EQ(j % 2, n->at(j));
    }

    n->deallocate(a);
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

    n->remove(0, a);
    n->remove(0, a);

    ASSERT_EQ(1u, n->child_count());
    ASSERT_EQ(m - 2, n->size());
    ASSERT_EQ(m / 2 - 1, n->p_sum());
    for (uint64_t j = 0; j < m - 2; j++) {
        ASSERT_EQ(j % 2, n->at(j));
    }

    n->deallocate(a);
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class node, class leaf, class alloc>
void node_remove_leaf_g_test(uint64_t size) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    for (uint64_t i = 0; i < 2; i++) {
        leaf* l = a->template allocate_leaf<leaf>(size / (64));
        for (uint64_t j = 0; j < size; j++) {
            l->insert(j, j % 2);
        }
        n->append_child(l);
    }

    ASSERT_EQ(2u, n->child_count());
    ASSERT_EQ(size * 2, n->size());
    ASSERT_EQ(size, n->p_sum());
    for (uint64_t j = 0; j < size * 2; j++) {
        ASSERT_EQ(j % 2, n->at(j)) << "j = " << j;
    }

    for (uint64_t i = 0; i < size; i++) {
        n->remove(0, a);
    }

    ASSERT_EQ(2u, n->child_count());
    ASSERT_EQ(size, n->size());
    ASSERT_EQ(size / 2, n->p_sum());
    for (uint64_t j = 0; j < size; j++) {
        ASSERT_EQ(j % 2, n->at(j)) << "j = " << j;
    }

    n->deallocate(a);
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

template <class node, class leaf, class alloc>
void node_remove_leaf_h_test(uint64_t size) {
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    for (uint64_t i = 0; i < 2; i++) {
        leaf* l = a->template allocate_leaf<leaf>(size / (64));
        for (uint64_t j = 0; j < size; j++) {
            l->insert(j, j % 2);
        }
        n->append_child(l);
    }

    ASSERT_EQ(2u, n->child_count());
    ASSERT_EQ(size * 2, n->size());
    ASSERT_EQ(size, n->p_sum());
    for (uint64_t j = 0; j < size * 2; j++) {
        ASSERT_EQ(j % 2, n->at(j)) << "j = " << j;
    }

    for (uint64_t i = 2 * size - 1; i < 2 * size; i -= 2) {
        n->remove(i, a);
    }

    ASSERT_EQ(2u, n->child_count());
    ASSERT_EQ(size, n->size());
    ASSERT_EQ(0u, n->p_sum());
    for (uint64_t j = 0; j < size; j++) {
        ASSERT_EQ(false, n->at(j)) << "j = " << j;
    }

    n->deallocate(a);
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
        for (uint64_t k = 0; k < b; k++) {
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
        n->remove(i, a);
    }

    ASSERT_EQ(2u, n->child_count());
    ASSERT_EQ(1u * b * size, n->size());
    ASSERT_EQ(1u * 0, n->p_sum());

    for (uint64_t i = 0; i < size / 2; i++) {
        ASSERT_EQ(false, n->at(i));
    }

    n->deallocate(a);
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

    n->remove(n->size() - 1, a);

    ASSERT_EQ(2u, n->child_count());
    ASSERT_EQ(b * size + (b / 3) * size - 1, n->size());
    ASSERT_EQ(b / 2 * size + (b / 3) * size / 2 - 1, n->p_sum());
    node* n1 = reinterpret_cast<node*>(n->child(0));
    node* n2 = reinterpret_cast<node*>(n->child(1));
    ASSERT_NEAR(n1->child_count(), n2->child_count(), 1);
    for (uint64_t j = 0; j < b * size + (b / 3) * size - 1; j++) {
        ASSERT_EQ(j % 2, n->at(j)) << "j = " << j;
    }

    n->deallocate(a);
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

    n->remove(n->size() - 1, a);

    ASSERT_EQ(1u, n->child_count());
    ASSERT_EQ(2u * (b / 3) * size - 1, n->size());
    ASSERT_EQ(1u * (b / 3) * size - 1, n->p_sum());
    for (uint64_t j = 0; j < 2u + (b / 3) * size - 1; j++) {
        ASSERT_EQ(j % 2, n->at(j)) << "j = " << j;
    }

    n->deallocate(a);
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

    n->remove(n->size() - 1, a);

    ASSERT_EQ(3u, n->child_count());
    ASSERT_EQ(2u * b * size + (b / 3) * size - 1, n->size());
    ASSERT_EQ(1u * b * size + (b / 3) * size / 2 - 1, n->p_sum());
    node* n1 = reinterpret_cast<node*>(n->child(1));
    node* n2 = reinterpret_cast<node*>(n->child(2));
    ASSERT_NEAR(n1->child_count(), n2->child_count(), 1);
    for (uint64_t j = 0; j < 2u * b * size + (b / 3) * size - 1; j++) {
        ASSERT_EQ(j % 2, n->at(j)) << "j = " << j;
    }

    n->deallocate(a);
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

    n->remove(0, a);

    ASSERT_EQ(2u, n->child_count());
    ASSERT_EQ(b * size + (b / 3) * size - 1, n->size());
    ASSERT_EQ((b * size + (b / 3) * size) / 2, n->p_sum());
    node* n1 = reinterpret_cast<node*>(n->child(0));
    node* n2 = reinterpret_cast<node*>(n->child(1));
    ASSERT_NEAR(n1->child_count(), n2->child_count(), 1);
    for (uint64_t j = 0; j < b * size + (b / 3) * size - 1; j++) {
        ASSERT_EQ(j % 2 == 0, n->at(j)) << "j = " << j;
    }

    n->deallocate(a);
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

    n->remove(0, a);

    ASSERT_EQ(1u, n->child_count());
    ASSERT_EQ(2u * (b / 3) * size - 1, n->size());
    ASSERT_EQ(1u * (b / 3) * size, n->p_sum());
    for (uint64_t j = 0; j < 2u + (b / 3) * size - 1; j++) {
        ASSERT_EQ(j % 2 == 0, n->at(j)) << "j = " << j;
    }

    n->deallocate(a);
    a->deallocate_node(n);
    ASSERT_EQ(0u, a->live_allocations());
    delete (a);
}

TEST(SimpleNode, AddLeaf) { node_add_child_test<nd, sl, ma>(BRANCH); }

TEST(SimpleNode, AddNode) { node_add_node_test<nd, sl, ma>(BRANCH); }

TEST(SimpleNode, AppendAll) { node_append_all_test<nd, sl, ma>(BRANCH); }

TEST(SimpleNode, ClearLast) { node_clear_last_test<nd, sl, ma>(BRANCH); }

TEST(SimpleNode, TransferPrepend) {
    node_transfer_prepend_test<nd, sl, ma>(BRANCH);
}

TEST(SimpleNode, ClearFirst) { node_clear_first_test<nd, sl, ma>(BRANCH); }

TEST(SimpleNode, TransferAppend) {
    node_transfer_append_test<nd, sl, ma>(BRANCH);
}

TEST(SimpleNode, AccessSingleLeaf) {
    node_access_single_leaf_test<nd, sl, ma>();
}

TEST(SimpleNode, AccessLeaf) { node_access_leaf_test<nd, sl, ma>(BRANCH); }

TEST(SimpleNode, AccessNode) { node_access_node_test<nd, sl, ma>(BRANCH); }

TEST(SimpleNode, SetLeaf) { node_set_leaf_test<nd, sl, ma>(BRANCH); }

TEST(SimpleNode, SetNode) { node_set_node_test<nd, sl, ma>(BRANCH); }

TEST(SimpleNode, RankLeaf) { node_rank_leaf_test<nd, sl, ma>(BRANCH); }

TEST(SimpleNode, RankNode) { node_rank_node_test<nd, sl, ma>(BRANCH); }

TEST(SimpleNode, SelectSingleLeaf) {
    node_select_single_leaf_test<nd, sl, ma>();
}

TEST(SimpleNode, SelectLeaf) { node_select_leaf_test<nd, sl, ma>(BRANCH); }

TEST(SimpleNode, SelectNode) { node_select_node_test<nd, sl, ma>(BRANCH); }

TEST(SimpleNode, InsertSingleLeaf) {
    node_insert_single_leaf_test<nd, sl, ma>();
}

TEST(SimpleNode, InsertSingleLeafRealloc) {
    node_insert_single_leaf_realloc_test<nd, sl, ma>();
}

TEST(SimpleNode, InsertSingleLeafSplit) {
    node_insert_single_leaf_split_test<nd, sl, ma>(SIZE);
}

TEST(SimpleNode, InsertLeafSplit) {
    node_insert_leaf_split_test<nd, sl, ma>(SIZE);
}

TEST(SimpleNode, InsertLeafRebalance) {
    node_insert_leaf_rebalance_test<nd, sl, ma>(SIZE);
}

TEST(SimpleNode, InsertNode) { node_insert_node_test<nd, sl, ma>(); }

TEST(SimpleNode, InsertNodeSplit) {
    node_insert_node_split_test<nd, sl, ma>(BRANCH);
}

TEST(SimpleNode, RemoveLeafSimple) {
    node_remove_leaf_simple_test<nd, sl, ma>(SIZE);
}

TEST(SimpleNode, RemoveLeafA) { node_remove_leaf_a_test<nd, sl, ma>(SIZE); }

TEST(SimpleNode, RemoveLeafB) { node_remove_leaf_b_test<nd, sl, ma>(SIZE); }

TEST(SimpleNode, RemoveLeafC) { node_remove_leaf_c_test<nd, sl, ma>(SIZE); }

TEST(SimpleNode, RemoveLeafD) { node_remove_leaf_d_test<nd, sl, ma>(SIZE); }

TEST(SimpleNode, RemoveLeafE) { node_remove_leaf_e_test<nd, sl, ma>(SIZE); }

TEST(SimpleNode, RemoveLeafF) { node_remove_leaf_f_test<nd, sl, ma>(SIZE); }

TEST(SimpleNode, RemoveLeafG) { node_remove_leaf_g_test<nd, sl, ma>(SIZE); }

TEST(SimpleNode, RemoveLeafH) { node_remove_leaf_h_test<nd, sl, ma>(SIZE); }

TEST(SimpleNode, RemoveNodeSimple) {
    node_remove_node_simple_test<nd, sl, ma>(SIZE, BRANCH);
}

TEST(SimpleNode, RemoveNodeA) {
    node_remove_node_a_test<nd, sl, ma>(SIZE, BRANCH);
}

TEST(SimpleNode, RemoveNodeB) {
    node_remove_node_b_test<nd, sl, ma>(SIZE, BRANCH);
}

TEST(SimpleNode, RemoveNodeC) {
    node_remove_node_c_test<nd, sl, ma>(SIZE, BRANCH);
}

TEST(SimpleNode, RemoveNodeD) {
    node_remove_node_d_test<nd, sl, ma>(SIZE, BRANCH);
}

TEST(SimpleNode, RemoveNodeE) {
    node_remove_node_e_test<nd, sl, ma>(SIZE, BRANCH);
}

#endif