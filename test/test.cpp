#include "../deps/googletest/googletest/include/gtest/gtest.h"

#include "../bit_vector/simple_leaf.hpp"
#include "../bit_vector/allocator.hpp"
#include "../bit_vector/simple_node.hpp"

#include "leaf_tests.hpp"
#include "node_tests.hpp"

typedef malloc_alloc ma;
typedef simple_leaf<8> sl;
typedef simple_leaf<0> ubl;
typedef simple_node<sl, 16384> nd;

//Tests for buffered leaf
TEST(SimpleLeaf, Insert) {
    leaf_insert_test<sl, ma>(10000);
}

TEST(SimpleLeaf, Remove) {
    leaf_remove_test<sl, ma>(10000);
}

TEST(SimpleLeaf, Rank) {
    leaf_rank_test<sl, ma>(10000);
}

TEST(SimpleLeaf, RankOffset) {
    leaf_rank_test<sl, ma>(11);
}

TEST(SimpleLeaf, Select) {
    leaf_select_test<sl, ma>(10000);
}

TEST(SimpleLeaf, SelectOffset) {
    leaf_select_test<sl, ma>(11);
}

TEST(SimpleLeaf, Set) {
    leaf_set_test<sl, ma>(10000);
}

TEST(SimpleLeaf, ClearFirst) {
    leaf_clear_start_test<sl, ma>();
}

TEST(SimpleLeaf, TransferAppend) {
    leaf_transfer_append_test<sl, ma>();
}

TEST(SimpleLeaf, ClearLast) {
    leaf_clear_end_test<sl, ma>();
}

TEST(SimpleLeaf, TransferPrepend) {
    leaf_transfer_prepend_test<sl, ma>();
}

TEST(SimpleLeaf, AppendAll) {
    leaf_append_all_test<sl, ma>();
}

TEST(SimpleLeaf, BufferHit) {
    leaf_hit_buffer_test<sl, ma>();
}

//A couple of tests to check that unbuffered works as well.
TEST(SimpleLeafUnb, Insert) {
    leaf_insert_test<ubl, ma>(10000);
}

TEST(SimpleLeafUnb, Remove) {
    leaf_remove_test<ubl, ma>(10000);
}

TEST(SimpleLeafUnb, Rank) {
    leaf_rank_test<ubl, ma>(10000);
}

TEST(SimpleLeafUnb, RankOffset) {
    leaf_rank_test<ubl, ma>(11);
}

TEST(SimpleLeafUnb, Select) {
    leaf_select_test<ubl, ma>(10000);
}

TEST(SimpleLeafUnb, SelectOffset) {
    leaf_select_test<ubl, ma>(11);
}

TEST(SimpleLeafUnb, Set) {
    leaf_set_test<ubl, ma>(10000);
}

//Node tests
TEST(SimpleNode, AddLeaf) {
    node_add_child_test<nd, sl, ma>();
}

TEST(SimpleNode, AddNode) {
    node_add_node_test<nd, sl, ma>();
}

TEST(SimpleNode, SplitLeaves) {
    node_split_leaves_test<nd, sl, ma>();
}

TEST(SimpleNode, SplitNodes) {
    node_split_nodes_test<nd, sl, ma>();
}

TEST(SimpleNode, AppendAll) {
    node_append_all_test<nd, sl, ma>();
}

TEST(SimpleNode, ClearLast) {
    node_clear_last_test<nd, sl, ma>();
}

TEST(SimpleNode, TransferPrepend) {
    node_transfer_prepend_test<nd, sl, ma>();
}

TEST(SimpleNode, ClearFirst) {
    node_clear_first_test<nd, sl, ma>();
}

TEST(SimpleNode, TransferAppend) {
    node_transfer_append_test<nd, sl, ma>();
}

TEST(SimpleNode, AccessSingleLeaf) {
    node_access_single_leaf_test<nd, sl, ma>();
}

TEST(SimpleNode, AccessLeaf) {
    node_access_leaf_test<nd, sl, ma>();
}

TEST(SimpleNode, AccessNode) {
    node_access_node_test<nd, sl, ma>();
}