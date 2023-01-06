#include "../bit_vector/bv.hpp"
#include "../bit_vector/internal/allocator.hpp"
#include "../bit_vector/internal/bit_vector.hpp"
#include "../bit_vector/internal/branch_selection.hpp"
#include "../bit_vector/internal/leaf.hpp"
#include "../bit_vector/internal/node.hpp"
#include "../bit_vector/internal/query_support.hpp"
#include "../bit_vector/internal/gap_leaf.hpp"
#include "../deps/DYNAMIC/include/dynamic/dynamic.hpp"
#include "../deps/googletest/googletest/include/gtest/gtest.h"

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
typedef leaf<16, SIZE, true, true> rll;
typedef node<rll, uint64_t, SIZE, 64, true, true> rl_node;
typedef simple_bv<16, SIZE, 64, true, true, true> rle_bv;
typedef gap_leaf<SIZE, 32, 7> g_leaf;

// Tests for buffered leaf
#include "leaf_tests.hpp"

// Tests for hybrid RLE leaves.
#include "rle_leaf_test.hpp"

// Branch selection tests
#include "branch_selection_test.hpp"

// Node tests
#include "node_tests.hpp"

// Bit vector tests
#include "bv_tests.hpp"

// Rle hybrid management tests
#include "rle_management_test.hpp"

// Query support structure tests
#include "query_support_test.hpp"

// Packed array tests
#include "packed_array_test.hpp"

// Gap leaf tests
#include "gap_leaf_test.hpp"

// Run tests
#include "run_tests.hpp"