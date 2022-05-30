#ifndef BV_GAP_LEAF_HPP
#define BV_GAP_LEAH_HPP

#include <cassert>
#include <cstdint>
#include <cstring>

#include "uncopyable.hpp"

namespace bv {

template <uint32_t leaf_size, uint32_t blocks, uint32_t block_bits>
class gap_leaf : uncopyable {
   private:
    static const constexpr uint64_t WORD_BITS = 64;
    static const constexpr uint32_t BLOCK_SIZE = leaf_size / blocks;
    static const constexpr uint32_t GAP_SIZE = (uint32_t(1) << block_bits) - 1;
    static const constexpr uint16_t BLOCK_WORDS = leaf_size / (WORD_BITS * blocks);

    uint64_t* _data;
    uint32_t _meta[blocks * block_bits / 32];
    uint32_t _size;
    uint16_t _capacity;
    uint16_t _last_block;

    // Bits are packed into words. No point in leaf sizes that do not pack well
    // into words.
    static_assert(leaf_size % WORD_BITS == 0);

    // Setting blocks or block bits to 0 is effectively the same as using an
    // unbuffered leaf.
    static_assert(blocks > 0);
    static_assert(block_bits > 0);

    // Meta-data is packed into 32-bit integers. Space is wasted if blocks *
    // block_bits % 32 != 0.
    static_assert(blocks * block_bits % 32 == 0);

    // Blocks should align to word boundaries.
    static_assert((leaf_size / WORD_BITS) % blocks == 0);

    // If the following fails, space is being wasted and block_bits can be
    // reduced with no penalties.
    static_assert((uint32_t(1) << block_bits) <= leaf_size / blocks);

    // Arbitrary leaf size limit to allow word indexing with 16 bit integers.
    static_assert(leaf_size < (uint32_t(1) << 22));

   public:
    gap_leaf(uint16_t capacity, uint64_t* data, uint32_t elems = 0,
             bool val = false)
        : _data(data), _meta(), _size(0), _capacity(capacity), _last_block(0) {
        assert(elems == 0);
        assert(val == false);
    }

    void insert(uint32_t i, bool v) {
        uint16_t target_block = 0;
        uint32_t target_offset = 0;
        uint32_t block_sum = 0;
        for (uint16_t i = 0; i < _last_block; i++) {
            block_sum += BLOCK_SIZE - _meta[i + 1];
            target_offset = block_sum >= i ? target_offset : block_sum;
            target_block += block_sum >= i ? 0 : 1;
        }

        /*target_offset = i - target_offset;
        uint16_t target_word = target_offset / WORD_BITS;
        target_offset %= WORD_BITS;*/

        if (target_block == _last_block) {
            if (_size - )
        }
    }


};
}  // namespace bv

#endif