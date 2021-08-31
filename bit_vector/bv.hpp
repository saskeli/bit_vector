#ifndef BV_HPP
#define BV_HPP

#include "internal/allocator.hpp"
#include "internal/bit_vector.hpp"
#include "internal/leaf.hpp"
#include "internal/node.hpp"

namespace bv {

/**
 * @file bv.hpp
 *
 * @brief Type defintions for dynamic bit vectors.
 *
 * This file is intended to be included by projects wanting to use dynamic bit
 * vectors by providing more approachable type definitions based on the
 * bv::bit_vector class.
 *
 * Use the bv::bv type to easily get started working with reasonable default
 * values.
 *
 *      #include "bit_vector/bv.hpp"
 *      ...
 *      bv::bv bv;
 *      bv.insert(0, true);
 *      bv.insert(0, false);
 *      assert(bv.at(0) == false);
 *      assert(bv.at(1) == true);
 *      bv.remove(0);
 *      assert(bv.at(0) == true);
 *      assert(bv.size() == 1);
 */

/**
 * @brief Helper type definition template for bit vector with at most 2^63
 * elements
 *
 * Uses the default [allocator](@ref bv::malloc_alloc),
 * [internal node](@ref bv::node) and [leaf](@ref bv::leaf) implementations
 * packaged in the default [bit vector root](@ref bv::bit_vector)
 *
 * @tparam buffer_size      Size of the insert/remove buffer in the leaf
 *                          elements.\n Needs to be in the [0, 64) range.
 * @tparam leaf_size        Maximum number of elements stored in a single
 *                          leaf.\n Needs to be divisible by 64 and in the
 *                          [265, 16777215) range.
 * @tparam branching_factor Maximum number of children for an internal node.\n
 *                          Needs to be one of {8, 16, 32, 64, 128}.
 * @tparam avx              Should avx population counting be used for rank.
 */
template <uint8_t buffer_size, uint64_t leaf_size, uint8_t branching_factor,
          bool avx = true, bool aggressive_realloc = false>
using simple_bv =
    bit_vector<leaf<buffer_size, avx>,
               node<leaf<buffer_size>, uint64_t, leaf_size, branching_factor, aggressive_realloc>,
               malloc_alloc, leaf_size, branching_factor, uint64_t, aggressive_realloc>;

/**
 * @brief Helper type definition template for bit vector with at most 2^31
 * elements
 *
 * Uses the default [allocator](@ref bv::malloc_alloc),
 * [internal node](@ref bv::node) and [leaf](@ref bv::leaf) implementations
 * packaged in the default [bit vector root](@ref bv::bit_vector)
 *
 * This implementation uses slightly less space and is slightly faster than
 * bv::simple_bv due to using 32 bit words for internal bookkeeping instead of
 * 64 bit words.
 *
 * @tparam buffer_size      Size of the insert/remove buffer in the leaf
 *                          elements.\n 0 <= `buffer_size` < 64.
 * @tparam leaf_size        Maximum number of elements stored in a single
 *                          leaf.\n 265 <= `leaf_size` < 16777215.
 * @tparam branching_factor Maximum number of children for an internal node.\n
 *                          Needs to be one 8, 16, 32, 64 or 128.
 * @tparam avx              Should avx population counting be used for rank.
 */
template <uint8_t buffer_size, uint64_t leaf_size, uint8_t branching_factor,
          bool avx = true, bool aggressive_realloc = false>
using small_bv =
    bit_vector<leaf<buffer_size, avx>,
               node<leaf<buffer_size, avx>, uint32_t, leaf_size, branching_factor, aggressive_realloc>,
               malloc_alloc, leaf_size, branching_factor, uint32_t, aggressive_realloc>;

/**
 * @brief Default dynamic bit vector type.
 *
 * Convenience type based on bv::simple_bv with reasonable default template
 * parameters.
 *
 * Buffer size = 8, leaf size = 2^14 and branching factor = 64
 */
typedef simple_bv<8, 16384, 64> bv;
}  // namespace bv

#endif