#include "../bit_vector/bv.hpp"
#include "../bit_vector/internal/allocator.hpp"
#include "../bit_vector/internal/bit_vector.hpp"
#include "../bit_vector/internal/leaf.hpp"
#include "../bit_vector/internal/branch_selection.hpp"
#include "../bit_vector/internal/node.hpp"
#include "../bit_vector/internal/query_support.hpp"
#include "../deps/DYNAMIC/include/dynamic/dynamic.hpp"
#include "../deps/googletest/googletest/include/gtest/gtest.h"
#include "bv_tests.hpp"
#include "leaf_tests.hpp"
#include "branch_selection_test.hpp"
#include "node_tests.hpp"
#include "run_tests.hpp"
#include "query_support_test.hpp"

#define SIZE 16384
#define BRANCH 16
#define BUFFER_SIZE 8

using namespace bv;

typedef malloc_alloc ma;
typedef leaf<BUFFER_SIZE> sl;
typedef leaf<0> ubl;
typedef node<sl, uint64_t, SIZE, BRANCH> nd;
typedef branchless_scan<uint64_t, BRANCH> branch;
typedef bit_vector<sl, nd, ma, SIZE, BRANCH, uint64_t> test_bv;
typedef query_support<uint64_t, sl, SIZE / 3> qs;

// Tests for buffered leaf
TEST(SimpleLeaf, Insert) { leaf_insert_test<sl, ma>(10000); }

TEST(SimpleLeaf, OverfullAppend) { leaf_append_test<leaf<8>, ma>(8); }

TEST(SimpleLeaf, Remove) { leaf_remove_test<sl, ma>(10000); }

TEST(SimpleLeaf, Rank) { leaf_rank_test<sl, ma>(10000); }

TEST(SimpleLeaf, RankOffset) { leaf_rank_test<sl, ma>(11); }

TEST(SimpleLeaf, RankBlock) { leaf_rank_offset_test<sl, ma>(10000); }

TEST(SimpleLeaf, Select) { leaf_select_test<sl, ma>(10000); }

TEST(SimpleLeaf, SelectOffset) { leaf_select_test<sl, ma>(11); }

TEST(SimpleLeaf, Set) { leaf_set_test<sl, ma>(10000); }

TEST(SimpleLeaf, ClearFirst) { leaf_clear_start_test<sl, ma>(); }

TEST(SimpleLeaf, TransferAppend) { leaf_transfer_append_test<sl, ma>(); }

TEST(SimpleLeaf, ClearLast) { leaf_clear_end_test<sl, ma>(); }

TEST(SimpleLeaf, TransferPrepend) { leaf_transfer_prepend_test<sl, ma>(); }

TEST(SimpleLeaf, AppendAll) { leaf_append_all_test<sl, ma>(); }

TEST(SimpleLeaf, BufferHit) { leaf_hit_buffer_test<sl, ma>(); }

TEST(SimpleLeaf, Commit) { leaf_commit_test<sl, ma>(SIZE); }

// A couple of tests to check that unbuffered works as well.
TEST(SimpleLeafUnb, Insert) { leaf_insert_test<ubl, ma>(10000); }

TEST(SimpleLeafUnb, Remove) { leaf_remove_test<ubl, ma>(10000); }

TEST(SimpleLeafUnb, Rank) { leaf_rank_test<ubl, ma>(10000); }

TEST(SimpleLeafUnb, RankOffset) { leaf_rank_test<ubl, ma>(11); }

TEST(SimpleLeafUnb, Select) { leaf_select_test<ubl, ma>(10000); }

TEST(SimpleLeafUnb, SelectOffset) { leaf_select_test<ubl, ma>(11); }

TEST(SimpleLeafUnb, Set) { leaf_set_test<ubl, ma>(10000); }

// Branch selection tests
TEST(SimpleBranch, Access) { branching_set_access_test<branch, BRANCH>(); }

TEST(SimpleBranch, Increment) { branching_increment_test<branch, BRANCH>(); }

TEST(SimpleBranch, ClearFirst) { branching_delete_first_n_test<branch, BRANCH>(); }

TEST(SimpleBranch, Appending) { branching_append_n_test<branch, BRANCH>(); }

TEST(SimpleBranch, ClearLast) { branching_delete_last_n_test<branch, BRANCH>(); }

TEST(SimpleBranch, Prepending) { branching_prepend_n_test<branch, BRANCH>(); }

TEST(SimpleBranch, TransferPrepend) { branching_transfer_prepend_n_test<branch, BRANCH>(); }

// Node tests
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

TEST(SimpleNode, InsertNodeSplit) { node_insert_node_split_test<nd, sl, ma>(BRANCH); }

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

// Bit vector tests
TEST(SimpleBV, InstantiateWithAlloc) {
    bv_instantiation_with_allocator_test<ma, test_bv>();
}

TEST(SimpleBV, InstantiateWithoutAlloc) {
    bv_instantiation_without_allocator_test<test_bv>();
}

TEST(SimpleBV, InsertSplitDeallocA) {
    bv_insert_split_dealloc_a_test<ma, test_bv>(SIZE);
}

TEST(SimpleBV, InsertSplitDeallocB) {
    bv_insert_split_dealloc_b_test<ma, test_bv>(SIZE);
}

TEST(SimpleBV, AccessLeaf) { bv_access_leaf_test<ma, test_bv>(SIZE); }

TEST(SimpleBV, RemoveLeaf) { bv_remove_leaf_test<ma, test_bv>(SIZE); }

TEST(SimpleBV, RemoveNode) { bv_remove_node_test<ma, test_bv>(SIZE); }

TEST(SimpleBV, RemoveNodeNode) {
    bv_remove_node_node_test<ma, test_bv>(SIZE);
}

TEST(SimpleBV, SetLeaf) { bv_set_leaf_test<ma, test_bv>(SIZE); }

TEST(SimpleBV, SetNode) { bv_set_node_test<ma, test_bv>(SIZE); }

TEST(SimpleBV, RankLeaf) { bv_rank_leaf_test<ma, test_bv>(SIZE); }

TEST(SimpleBV, RankNode) { bv_rank_node_test<ma, test_bv>(SIZE); }

TEST(SimpleBV, SelectLeaf) { bv_select_leaf_test<ma, test_bv>(SIZE); }

TEST(SimpleBV, SelectNode) { bv_select_node_test<ma, test_bv>(SIZE); }

// Query support structure tests
TEST(QuerySupport, SingleAccess) {
    qs_access_single_leaf<qs, sl, ma>(SIZE);
}

TEST(QuerySupport, SingleRank) {
    qs_rank_single_leaf<qs, sl, ma>(SIZE);
}

TEST(QuerySupport, SingleSelect) {
    qs_select_single_leaf<qs, sl, ma>(SIZE);
}

TEST(QuerySupport, DoubleAccess) {
    qs_access_two_leaves<qs, sl, nd, ma>(SIZE);
}

TEST(QuerySupport, DoubleRank) {
    qs_rank_two_leaves<qs, sl, nd, ma>(SIZE);
}

TEST(QuerySupport, DoubleSelect) {
    qs_select_two_leaves<qs, sl, nd, ma>(SIZE);
}

TEST(QuerySupport, DoubleSelect2) {
    qs_select_two_leaves_two<qs, sl, nd, ma>(SIZE);
}

// Run tests
TEST(Run, A) {
    uint64_t a[] = {1570};
    run_test<simple_bv<8, 256, 8>, dyn::suc_bv>(a, 1);
}

TEST(Run, B) {
    uint64_t a[] = {4270};
    run_test<simple_bv<8, 256, 8>, dyn::suc_bv>(a, 1);
}

TEST(Run, C) {
    uint64_t a[] = {
        876, 1,   27,  2,   668, 1,   1,   594, 0,   26,  0,   2,   320, 1,
        2,   679, 1,   0,   82,  1,   0,   673, 0,   2,   201, 0,   1,   875,
        1,   157, 2,   219, 1,   0,   761, 1,   0,   531, 0,   1,   579, 1,
        738, 1,   636, 1,   571, 2,   152, 0,   0,   176, 1,   2,   453, 1,
        1,   93,  2,   795, 0,   1,   275, 2,   580, 1,   0,   406, 0,   2,
        290, 0,   1,   619, 0,   14,  0,   0,   817, 1,   0,   128, 1,   1,
        0,   2,   73,  0,   1,   333, 1,   799, 1,   352, 0,   678, 1,   0,
        667, 1,   1,   786, 0,   643, 0,   1,   295, 0,   340, 1,   0,   53,
        1,   2,   811, 1,   0,   416, 0,   2,   638, 0,   1,   36,  2,   700,
        1,   0,   278, 1,   2,   551, 0,   0,   101, 1,   1,   739, 2,   362,
        0,   0,   392, 0,   0,   608, 0,   0,   800, 1,   2,   4,   0,   1,
        798, 1,   173, 2,   417, 1,   0,   538, 1,   1,   876, 1,   111, 1,
        493, 0,   467, 1,   0,   765, 1,   1,   660, 0,   254, 1,   0,   603,
        0,   0,   499, 0,   1,   483, 1,   415, 1,   522, 2,   132, 1,   1,
        210, 0,   570, 0,   1,   613, 2,   741, 1,   0,   383, 0,   0,   0,
        1,   2,   205, 1,   1,   350, 1,   327, 1,   671, 2,   466, 0,   2,
        562, 0,   2,   728, 0,   0,   599, 1,   1,   514, 2,   762, 1,   2,
        555, 1,   1,   368, 1,   172, 2,   468, 1,   2,   762, 1,   2,   290,
        0,   2,   5,   1,   0,   242, 0,   1,   775, 0,   8,   0,   1,   799,
        0,   281, 1,   2,   242, 1,   0,   660, 1,   2,   401, 1,   0,   798,
        0,   2,   370, 1,   2,   20,  0,   2,   379, 1,   1,   436, 0,   154,
        1};
    run_test<simple_bv<8, 256, 8>, dyn::suc_bv>(a, 148);
}

TEST(Run, D) {
    uint64_t a[] = {
        377, 0,   50,  0,   0,   284, 0,   2,   357, 1,   2,   233, 1,   2,
        251, 0,   1,   148, 0,   83,  0,   1,   137, 2,   286, 0,   0,   178,
        1,   0,   352, 0,   0,   163, 1,   1,   288, 0,   158, 0,   1,   121,
        2,   263, 0,   2,   322, 1,   1,   254, 0,   57,  1,   1,   170, 0,
        353, 1,   1,   24,  0,   348, 1,   0,   65,  1,   0,   254, 1,   2,
        221, 1,   2,   187, 0,   0,   243, 0,   2,   19,  1,   1,   7,   2,
        264, 1,   1,   140, 1,   243, 0,   313, 0,   0,   362, 1,   0,   340,
        1,   2,   280, 1,   0,   304, 0,   2,   262, 0,   1,   344, 0,   320,
        0,   2,   318, 0,   2,   12,  1,   0,   352, 1,   0,   79,  0,   2,
        295, 1,   0,   7,   1,   0,   220, 1,   2,   351, 1,   1,   126, 2,
        326, 0,   2,   81,  0,   0,   107, 0,   0,   277, 1,   2,   20,  0,
        1,   341, 1,   286, 0,   127, 0,   2,   376, 0,   2,   338, 1,   1,
        270, 1,   184, 1,   15,  1,   299, 1,   45,  0,   361, 0,   2,   70,
        0,   2,   46,  0,   0,   255, 0,   2,   374, 1,   2,   310, 0,   0,
        52,  1,   0,   29,  0,   1,   124, 1,   282, 1,   145, 1,   280, 1,
        276, 2,   338, 0,   0,   239, 0,   2,   217, 0,   2,   41,  1,   0,
        381, 1,   2,   355, 1,   0,   238, 1,   0,   365, 0,   2,   155, 0,
        2,   65,  0,   2,   219, 0,   2,   168, 0,   2,   54,  0,   1,   238,
        2,   377, 1,   1,   325, 1,   364, 1,   327, 2,   343, 0,   0,   85,
        0,   1,   162, 1,   297, 2,   51,  0,   1,   94,  2,   6,   1,   1,
        162, 0,   147, 1,   2,   186, 1,   2,   19,  0,   0,   322, 1,   1,
        259, 0,   84,  0,   0,   193, 1,   0,   222, 0,   2,   240, 1,   0,
        169, 0,   2,   62,  1,   1,   247, 0,   263, 1,   0,   151, 1,   2,
        336, 0,   1,   231, 1,   276, 0,   128, 1,   0,   124, 1,   1,   299,
        0,   134, 0,   2,   235, 1,   1,   1,   2,   224, 1,   1,   79,  1,
        332, 2,   305, 0,   0,   254, 1,   0,   251, 1,   1,   236, 1,   83,
        2,   75,  0,   0,   133, 1,   2,   306, 1,   1,   23,  0,   10,  1};
    run_test<simple_bv<8, 256, 8>, dyn::suc_bv>(a, 378);
}