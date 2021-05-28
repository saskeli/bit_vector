#ifndef BV_HPP
#define BV_HPP

#include "internal/allocator.hpp"
#include "internal/leaf.hpp"
#include "internal/node.hpp"
#include "internal/bit_vector.hpp"

namespace bv {

typedef bit_vector<simple_leaf<8>, simple_node<simple_leaf<8>, 8192>, malloc_alloc, 8192> simple_bv;

}

#endif