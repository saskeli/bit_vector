#ifndef BV_GAP_LEAF_HPP
#define BV_GAP_LEAH_HPP

#include <cassert>
#include <cstdint>
#include <cstring>

#include "uncopyable.hpp"
#include "packed_array.hpp"

namespace bv {

template <uint32_t leaf_size, uint32_t blocks, uint32_t block_bits>
class gap_leaf : uncopyable {
   private:
    static const constexpr uint64_t WORD_BITS = 64;
    static const constexpr uint64_t ONE = uint64_t(1);
    static const constexpr uint64_t LAST_BIT == ONE << (WORD_BITS - 1);
    static const constexpr uint32_t BLOCK_SIZE = leaf_size / blocks;
    static const constexpr uint32_t GAP_SIZE = (uint32_t(1) << block_bits) - 1;
    static const constexpr uint16_t BLOCK_WORDS =
        leaf_size / (WORD_BITS * blocks);

    uint64_t* _data;
    packed_array<blocks, block_bits> _gaps;
    uint32_t _size;
    uint32_t _p_sum;
    uint16_t _capacity;
    uint16_t _last_block;
    uint16_t _last_block_space;
    bool _need_realloc;

    static_assert(leaf_size % WORD_BITS == 0,
                  "Bits are packed into words. No point in leaf sizes that do "
                  "not pack well into words.");

    static_assert((leaf_size / WORD_BITS) % blocks == 0,
                  "Blocks should align to word boundaries.");

    static_assert((uint32_t(1) << block_bits) <= leaf_size / blocks,
                  "If the following fails, space is being wasted and "
                  "block_bits can be reduced with no penalties.");

    static_assert(leaf_size < (uint32_t(1) << 22),
                  "Arbitrary leaf size limit to allow word indexing with 16 "
                  "bit integers.");

    static_assert(
        leaf_size / blocks < (uint32_t(1) << 16),
        "Number of free bits in block needs to be encodable in 16 bits.");

   public:
    gap_leaf(uint16_t capacity, uint64_t* data, uint32_t elems = 0,
             bool val = false)
        : _data(data),
          _gaps(),
          _size(0),
          _p_sum(0),
          _capacity(capacity),
          _last_block(0),
          _last_block_space(leaf_size / blocks),
          _need_realloc(false) {
        assert(elems == 0);
        assert(val == false);
    }

    void insert(uint32_t index, bool v) {
        uint16_t target_block = 0;
        uint32_t target_offset = 0;
        uint32_t block_sum = 0;
        for (uint16_t i = 0; i < _last_block; i++) {
            block_sum += BLOCK_SIZE - _gaps[i];
            target_offset = block_sum > index ? target_offset : block_sum;
            target_block += block_sum > index ? 0 : 1;
        }

        if (target_block == _last_block) {
            last_block_insert(index - target_offset, v);
            [[unlikely]] return;
        }
        if (_gaps[target_block] == 0) {
            make_space(target_block)
            [[unlikely]] return insert(index, v);
        }
        block_insert(target_block, index - target_offset, v);
        _size++;
        _p_sum += v ? 1 : 0;
    }

  private:
    void last_block_insert(uint32_t index, bool v) {
        uint64_t word = index / WORD_BITS;
        word += _last_block * BLOCK_WORDS;
        uint64_t offset = index % WORD_BITS;
        uint64_t mask = (ONE << offset) - 1;
        uint64_t overflow = _data[word] & (~mask);
        _data[word] &= mask;
        _data[word] |= (v ? ONE : uint64_t(0)) << offset;
        _data[word] |= overflow << 1;
        overflow &= LAST_BIT;
        for (uint64_t i = word + 1; i < (_last_block + 1) * BLOCK_WORDS; i++) {
            uint64_t n_overflow = _data[i] & LAST_BIT;
            _data[i] <<= 1;
            _data[i] |= overflow ? ONE : uint64_t(0);
            overflow = n_overflow;
        }
        _last_block_space--;
        if (_last_block_space <= GAP_SIZE) {
            _gaps[_last_block] = _last_block_space;
            _last_block++;
            _last_block_space = BLOCK_SIZE;
        }
    }

    void block_insert(uint32_t target_block, uint32_t index, bool v) {
        uint64_t word = index / WORD_BITS;
        word += target_block * BLOCK_WORDS;
        uint64_t offset = index & WORD_BITS;
        uint64_t mask = (ONE << offset) - 1;
        uint64_t overflow = _data[word] & (~mask);
        _data[word] &= mask;
        _data[word] |= (v ? ONE : uint64_t(0)) << offset;
        _data[word] |= overflow << 1;
        overflow &= LAST_BIT;
        for (uint64_t i = word + 1; i < (target_block + 1) * BLOCK_WORDS; i++) {
            uint64_t n_overflow = _data[i] & LAST_BIT;
            _data[i] <<= 1;
            _data[i] |= overflow ? ONE : uint64_t(0);
            overflow = n_overflow;
        }
        _gaps[target_block] -= 1;
    }

    void make_space(uint32_t target_block) {
        uint32_t gap;
        if (target_block > 0) {
            gap = _gaps[target_block - 1] > 0;
            if (gap > 0) {
                uint32_t source_word = target_block * BLOCK_WORDS;
                uint64_t target_offset = BLOCK_SIZE - gap;
                uint32_t target_word = source_word + BLOCK_WORDS;
                target_word += target_offset / WORD_BITS;
                target_offset %= WORD_BITS:
                gap = gap / 2 + gap % 2;
                if constexpr ((ONE << block_bits) > WORD_BITS) {
                    while (gap >= WORD_BITS) {

                    }
                }
            }
        }
    }
};
}  // namespace bv

#endif