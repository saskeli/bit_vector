#include "../bit_vector/bv.hpp"
#include "../bit_vector/internal/allocator.hpp"
#include "../bit_vector/internal/bit_vector.hpp"
#include "../bit_vector/internal/branch_selection.hpp"
#include "../bit_vector/internal/leaf.hpp"
#include "../bit_vector/internal/node.hpp"
#include "../bit_vector/internal/query_support.hpp"
#include "../deps/DYNAMIC/include/dynamic/dynamic.hpp"
#include "../deps/googletest/googletest/include/gtest/gtest.h"
#include "branch_selection_test.hpp"
#include "bv_tests.hpp"
#include "leaf_tests.hpp"
#include "node_tests.hpp"
#include "query_support_test.hpp"
#include "rle_leaf_test.hpp"
#include "rle_management_test.hpp"
#include "run_tests.hpp"

#define SIZE 16384
#define BRANCH 16
#define BUFFER_SIZE 8

using namespace bv;

typedef malloc_alloc ma;
typedef leaf<BUFFER_SIZE, SIZE> sl;
typedef leaf<0, SIZE> ubl;
typedef node<sl, uint64_t, SIZE, BRANCH> nd;
typedef branchless_scan<uint64_t, BRANCH> branch;
typedef bit_vector<sl, nd, ma, SIZE, BRANCH, uint64_t> test_bv;
typedef query_support<uint64_t, sl, 2048> qs;

// Tests for buffered leaf
TEST(SimpleLeaf, Insert) { leaf_insert_test<sl, ma>(10000); }

TEST(SimpleLeaf, OverfullAppend) { leaf_append_test<leaf<8, SIZE>, ma>(8); }

TEST(SimpleLeaf, Remove) { leaf_remove_test<sl, ma>(10000); }

TEST(SimpleLeaf, Rank) { leaf_rank_test<sl, ma>(10000); }

TEST(SimpleLeaf, RankOffset) { leaf_rank_test<sl, ma>(11); }

TEST(SimpleLeaf, RankBlock) { leaf_rank_offset_test<sl, ma>(10000); }

TEST(SimpleLeaf, Select) { leaf_select_test<sl, ma>(10000); }

TEST(SimpleLeaf, SelectOffset) { leaf_select_test<sl, ma>(11); }

TEST(SimpleLeaf, SelectBlock) { leaf_select_offset_test<sl, ma>(3000); }

TEST(SimpleLeaf, Select0) { leaf_select0_test<sl, ma>(10000); }

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

// Tests for hybrid RLE leaves.
typedef leaf<16, SIZE, true, true> rll;

TEST(RleLeaf, InitZeros) { rle_leaf_init_zeros_test<rll, ma>(10000); }

TEST(RleLeaf, InitOnes) { rle_leaf_init_ones_test<rll, ma>(10000); }

TEST(RleLeaf, Insert) { rle_leaf_insert_test<rll, ma>(10000, 100); }

TEST(RleLeaf, InsertMiddle) {
    rle_leaf_insert_middle_test<rll, ma>(10000, 100);
}

TEST(RleLeaf, InsertEnd) { rle_leaf_insert_end_test<rll, ma>(10000, 100); }

TEST(RleLeaf, Remove) { rle_leaf_remove_test<rll, ma>(200, 100); }

TEST(RleLeaf, Set) { rle_leaf_set_test<rll, ma>(200, 100); }

TEST(RleLeaf, CapCalculation) { rle_leaf_cap_calculation_test<rll, ma>(); }

TEST(RleLeaf, SetCalculations) { rle_leaf_set_calculations_test<rll, ma>(); }

TEST(RleLeaf, ClearFirst) { rle_leaf_clear_first_test<rll, ma>(); }

TEST(RleLeaf, TransferCapacity) { rle_leaf_transfer_capacity_test<rll, ma>(); }

TEST(RleLeaf, ClearLast) { rle_leaf_clear_last_test<rll, ma>(); }

TEST(RleLeaf, AppendAll) { rle_leaf_append_all_test<rll, ma>(); }

TEST(RleLeaf, TransferAppend) { rle_leaf_transfer_append_test<rll, ma>(); }

TEST(RleLeaf, TransferPrepend) { rle_leaf_transfer_prepend_test<rll, ma>(); }

TEST(RleLeaf, Convert) { rle_leaf_conversion_test<rll, ma>(); }

// Branch selection tests
TEST(SimpleBranch, Access) { branching_set_access_test<branch, BRANCH>(); }

TEST(SimpleBranch, Increment) { branching_increment_test<branch, BRANCH>(); }

TEST(SimpleBranch, ClearFirst) {
    branching_delete_first_n_test<branch, BRANCH>();
}

TEST(SimpleBranch, Appending) { branching_append_n_test<branch, BRANCH>(); }

TEST(SimpleBranch, ClearLast) {
    branching_delete_last_n_test<branch, BRANCH>();
}

TEST(SimpleBranch, Prepending) { branching_prepend_n_test<branch, BRANCH>(); }

TEST(SimpleBranch, TransferPrepend) {
    branching_transfer_prepend_n_test<branch, BRANCH>();
}

TEST(SimpleBranch, Insert) { branching_insert_test<branch, BRANCH>(); }

TEST(SimpleBranch, Remove) { branch_remove_test<branch, BRANCH>(); }

TEST(SimpleBranch, AppendElem) { branch_append_elem_test<branch, BRANCH>(); }

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

TEST(SimpleBV, RemoveNodeNode) { bv_remove_node_node_test<ma, test_bv>(SIZE); }

TEST(SimpleBV, SetLeaf) { bv_set_leaf_test<ma, test_bv>(SIZE); }

TEST(SimpleBV, SetNode) { bv_set_node_test<ma, test_bv>(SIZE); }

TEST(SimpleBV, RankLeaf) { bv_rank_leaf_test<ma, test_bv>(SIZE); }

TEST(SimpleBV, RankNode) { bv_rank_node_test<ma, test_bv>(SIZE); }

TEST(SimpleBV, SelectLeaf) { bv_select_leaf_test<ma, test_bv>(SIZE); }

TEST(SimpleBV, SelectNode) { bv_select_node_test<ma, test_bv>(SIZE); }

TEST(SimpleBV, Select0) {
    bv_select_0_test<test_bv, dyn::suc_bv>(10000);
}

// Rle hybrid management tests

typedef bv::node<rll, uint64_t, SIZE, 64, true, true> rl_node;
typedef bv::simple_bv<16, SIZE, 64, true, true, true> rle_bv;

TEST(RleBv, SplitLeaf) { node_split_rle_test<ma, rl_node, rll>(); }

TEST(RleBv, SplitLeafInRoot) { root_split_rle_test<rle_bv>(); }

// Query support structure tests
TEST(QuerySupport, SingleAccess) { qs_access_single_leaf<qs, sl, ma>(SIZE); }

TEST(QuerySupport, SingleRank) { qs_rank_single_leaf<qs, sl, ma>(SIZE); }

TEST(QuerySupport, SingleSelect) { qs_select_single_leaf<qs, sl, ma>(SIZE); }

TEST(QuerySupport, DoubleAccess) { qs_access_two_leaves<qs, sl, nd, ma>(SIZE); }

TEST(QuerySupport, DoubleRank) { qs_rank_two_leaves<qs, sl, nd, ma>(SIZE); }

TEST(QuerySupport, DoubleSelect) { qs_select_two_leaves<qs, sl, nd, ma>(SIZE); }

TEST(QuerySupport, DoubleSelect2) {
    qs_select_two_leaves_two<qs, sl, nd, ma>(SIZE);
}

TEST(QuerySupport, SparseSelect) {
    qs_sparse_bv_select_test<bv::bv>(100000, 34);
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

TEST(Run, E) {
    uint64_t a[] = {160, 1, 37, 2, 81, 0, 0, 8, 1, 1, 64};
    run_test<bv::simple_bv<8, 256, 8>, dyn::suc_bv>(a, 11);
}

TEST(Run, F) {
    uint64_t a[] = {91, 0,  41, 1,  2,  69, 0,  2,  8, 0,  0, 61, 0,
                    2,  47, 0,  1,  20, 2,  11, 0,  1, 90, 2, 30, 1,
                    1,  27, 2,  49, 0,  2,  80, 1,  1, 5,  1, 6,  1,
                    58, 0,  19, 0,  1,  53, 1,  78, 2, 13, 1, 1,  62};
    run_test<bv::simple_bv<8, 256, 8>, dyn::suc_bv>(a, 52);
}

TEST(RunSup, A) {
    uint64_t a[] = {326};
    run_sup_test<bv::bv>(a, 1);
}

TEST(RunSup, B) {
    uint64_t a[] = {5000};
    run_sup_test<bv::bv>(a, 1);
}

TEST(RunSup, C) {
    uint64_t a[] = {16387, 2,     7184, 1,    1,    9890,  1,    10662,
                    1,     11795, 1,    7332, 0,    14649, 1,    2,
                    14480, 1,     2,    6282, 0,    2,     4190, 1,
                    2,     16170, 0,    1,    12969};
    run_sup_test<bv::bv>(a, 29);
}

TEST(RunSup, D) {
    uint64_t a[] = {10920, 0, 1316, 0, 2, 9930, 0, 0, 2561, 0};
    run_sup_test<bv::bv>(a, 10);
}

TEST(RunSdsl, A) {
    uint64_t a[] = {26198, 1, 8440};
    run_sdsl_test<bv::bv>(a, 3);
}

typedef bv::simple_bv<16, 16384, 64, true, true, true> run_len_bv;

TEST(RunRle, A) {
    uint64_t a[] = {9585};
    run_test<run_len_bv, bv::bv>(a, 1, 1);
}

TEST(RunRle, B) {
    uint64_t a[] = {
        8718, 2,    5318, 0,    0,    7604, 0,    2,    7026, 0,    1,    8152,
        2,    7216, 0,    2,    6821, 0,    0,    5820, 1,    0,    7773, 0,
        1,    7272, 0,    7439, 0,    0,    2802, 0,    2,    6857, 0,    1,
        1330, 2,    3683, 0,    0,    5150, 0,    2,    466,  0,    2,    7529,
        0,    1,    689,  1,    120,  2,    1823, 0,    1,    6937, 1,    1889,
        1,    5818, 1,    7356, 1,    7652, 0,    8122, 0,    1,    7630, 2,
        6821, 0,    1,    8004, 0,    1395, 0,    2,    1669, 0,    0,    1080,
        0,    0,    4176, 0,    1,    3352, 2,    4156, 0,    0,    8306, 0,
        1,    329,  1,    5436, 1,    7487, 0,    3793, 0,    2,    4161, 0,
        2,    2957, 0,    2,    324,  0,    1,    2545, 1,    6531, 0,    3862,
        0,    1,    2998, 2,    7888, 0,    1,    8363, 0,    1337, 0,    1,
        2831, 1,    1209, 1,    4861, 2,    267,  0,    0,    6099, 0,    2,
        3670, 0,    0,    5251, 0,    0,    7846, 0,    0,    8327, 0,    1,
        7581, 0,    8515, 0};
    run_test<run_len_bv, bv::bv>(a, 160, 1);
}

TEST(RunRle, C) {
    uint64_t a[] = {
        111, 2,   81, 0,   2,  105, 0,  2,  66,  0,   2,  90, 0,  1,  109, 0,
        89,  0,   0,  42,  0,  0,   72, 0,  2,   107, 0,  0,  69, 0,  2,   104,
        0,   0,   61, 0,   2,  31,  0,  2,  59,  0,   2,  10, 1,  0,  37,  0,
        0,   14,  0,  2,   85, 0,   0,  51, 0,   2,   82, 0,  2,  89, 0,   2,
        55,  0,   0,  91,  0,  1,   16, 2,  80,  0,   1,  34, 2,  87, 0,   0,
        30,  0,   2,  104, 0,  0,   86, 0,  1,   41,  1,  19, 1,  84, 2,   58,
        0,   2,   81, 0,   0,  55,  0,  2,  113, 0,   0,  11, 0,  0,  25,  0,
        2,   103, 0,  1,   69, 1,   52, 2,  29,  0,   0,  11, 1,  1,  27,  2,
        93,  0,   1,  92,  2,  92,  0,  1,  53,  2,   36, 0,  2,  85, 0,   0,
        4,   0,   1,  114, 2,  26,  0,  1,  66,  0,   93, 0,  0,  5,  0,   2,
        83,  0,   2,  106, 0,  1,   15, 0,  89,  0,   0,  13, 0};
    run_test<run_len_bv, bv::bv>(a, 173, 1);
}

TEST(RunRle, D) {
    uint64_t a[] = {10000, 0,  1,  1, 0,  2,  0,  0,  3,  1,  0,  4, 0,  0,
                    5,      1,  0,  6, 0,  0,  7,  1,  0,  8,  0,  0, 9,  1,
                    0,      10, 0,  0, 11, 1,  0,  12, 0,  0,  13, 1, 0,  14,
                    0,      0,  15, 1, 0,  0,  1,  0,  2,  0,  0,  4, 1,  0,
                    6,      0,  0,  8, 1,  0,  10, 0,  0,  12, 1,  0, 14, 0,
                    0,      16, 1,  0, 18, 0,  0,  20, 1,  0,  22, 0, 0,  24,
                    1,      0,  26, 0, 0,  28, 1,  0,  30, 0};
    run_test<run_len_bv, bv::bv>(a, 94, 1);
}