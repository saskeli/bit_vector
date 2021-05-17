#include "../deps/googletest/googletest/include/gtest/gtest.h"

#include "../bit_vector/simple_leaf.hpp"
#include "../bit_vector/allocator.hpp"
#include "../bit_vector/simple_node.hpp"

#include "helpers.hpp"

typedef malloc_alloc ma;
typedef simple_leaf<8> sl;

TEST(SimpleLeaf, PushBack) {
    leaf_pushback_test<sl, ma>(100000); 
}

TEST(SimpleLeaf, Insert) {
    leaf_insert_test<sl, ma>(100000);
}

TEST(SimpleLeaf, Remove) {
    leaf_remove_test<sl, ma>(100000);
}

TEST(SimpleLeaf, Rank) {
    leaf_rank_test<sl, ma>(100000);
}

TEST(SimpleLeaf, RankOffset) {
    leaf_rank_test<sl, ma>(11);
}

TEST(SimpleLeaf, Select) {
    leaf_select_test<sl, ma>(100000);
}

TEST(SimpleLeaf, SelectOffset) {
    leaf_select_test<sl, ma>(11);
}