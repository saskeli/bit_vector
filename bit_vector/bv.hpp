#ifndef BV_HPP
#define BV_HPP

#include "internal/allocator.hpp"
#include "internal/bit_vector.hpp"
#include "internal/leaf.hpp"
#include "internal/node.hpp"

namespace bv {

template <uint8_t buffer_size, uint64_t leaf_size, uint8_t branching_factor>
using simple_bv = bit_vector<simple_leaf<buffer_size>,
                             simple_node<simple_leaf<buffer_size>, uint64_t,
                                         leaf_size, branching_factor>,
                             malloc_alloc, leaf_size, branching_factor>;

template <uint8_t buffer_size, uint64_t leaf_size, uint8_t branching_factor>
using small_bv = bit_vector<simple_leaf<buffer_size>,
                            simple_node<simple_leaf<buffer_size>, uint32_t,
                                        leaf_size, branching_factor>,
                            malloc_alloc, leaf_size, branching_factor>;

}  // namespace bv

#endif