#include "../deps/googletest/googletest/include/gtest/gtest.h"

#include "../bit_vector/simple_leaf.hpp"
#include "../bit_vector/allocator.hpp"
#include "../bit_vector/simple_node.hpp"

#include "helpers.hpp"

typedef malloc_alloc ma;
typedef simple_leaf<8> sl;
typedef simple_leaf<0> ubl;

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

TEST(SimpleLeaf, UnbInsert) {
    leaf_insert_test<ubl, ma>(10000);
}

TEST(SimpleLeaf, UnbRemove) {
    leaf_remove_test<ubl, ma>(10000);
}

TEST(SimpleLeaf, UnbRank) {
    leaf_rank_test<ubl, ma>(10000);
}

TEST(SimpleLeaf, UnbRankOffset) {
    leaf_rank_test<ubl, ma>(11);
}

TEST(SimpleLeaf, UnbSelect) {
    leaf_select_test<ubl, ma>(10000);
}

TEST(SimpleLeaf, UnbSelectOffset) {
    leaf_select_test<ubl, ma>(11);
}

TEST(SimpleLeaf, UnbSet) {
    leaf_set_test<ubl, ma>(10000);
}
