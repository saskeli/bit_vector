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
    static const constexpr uint16_t BLOCK_WORDS = BLOCK_SIZE / WORD_BITS;
    
    inline static uint64_t data_scratch_[leaf_size / 64];

    template <uint32_t block_size, uint32_t word_size>
    class Block : uncopyable {
      private:
        uint64_t data_[block_size / word_size];

      public:
        Block() = delete;

        void inset(uint32_t i, bool v) {
            static_assert(false, "Not yet implemented");
        }

        void append_from(Block* otherm, uint32_t elems) {
            static_assert(false, "Not yet implemented");
        }

        void prepend_from(Block* other, uint32_t elems) {
            static_assert(false, "Not yet implemented");
        }

        void dump(uint64_t* target, uint64_t offset) {
            static_assert(false, "Not yet implemented");
        }

        void read(uint64_t* source, uint64_t start, uint32_t elems) {
            static_assert(false, "Not yet implemented");
        }
    }

    uint64_t* data_;
    packed_array<blocks, block_bits> gaps_;
    uint32_t size_;
    uint32_t p_sum_;
    uint16_t capacity_;
    uint16_t last_block_;
    uint16_t last_block_space_;
    bool need_realloc_;

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
        : data_(data),
          gaps_(),
          size_(0),
          p_sum_(0),
          capacity_(capacity),
          last_block_(0),
          last_block_space_(leaf_size / blocks),
          need_realloc_(false) {
        assert(elems == 0);
        assert(val == false);
    }

    void insert(uint32_t index, bool v) {
        Block* data = reinterpret_cast<Block*>(data_);
        uint16_t target_block = 0;
        uint32_t target_offset = 0;
        uint32_t block_sum = 0;
        for (uint16_t i = 0; i < last_block_; i++) {
            block_sum += BLOCK_SIZE - gaps_[i];
            target_offset = block_sum > index ? target_offset : block_sum;
            target_block += block_sum > index ? 0 : 1;
        }

        if (target_block == last_block_) {
            data[target_block].insert(index - target_offset, v);
            last_block_space_--;
            if (last_block_space_ <= GAP_SIZE) {
                gaps_[last_block_] = last_block_space_;
                last_block_++;
                [[unlikely]] last_block_space_ = BLOCK_SIZE;
            }
            [[unlikely]] return;
        }
        if (gaps_[target_block] == 0) {
            make_space(target_block)
            [[unlikely]] return insert(index, v);
        }
        data[target_block].insert(index - target_offset, v);
        gaps_[target_block] -= 1;
        size_++;
        p_sum_ += v ? 1 : 0;
    }

  private:

    void make_space(uint32_t target_block) {
        Block* data = reinterpret_cast<Block*>(data_);
        uint32_t gap = 0;
        bool next = true;
        if (target_block + 1 < last_block_) {
            [[likely]] gap = gaps_[target_block + 1];
        } else if (target_block + 1 < blocks && target_block + 1 == last_block_){
            constexpr uint32_t elems = GAP_SIZE / 2 + GAP_SIZE % 2;
            data[target_block + 1].prepend_from(data[target_block], elems);
            gaps_[target_block] += elems;
            last_block_space_ -= elems;
            if (last_block_space_ <= GAP_SIZE) {
                gaps_[last_block_++] = last_block_space_;
                last_block_space_ = BLOCK_SIZE;
                [[unlikely]] (void(0));
            }
            [[unlikely]] return;
        }
        if (target_block > 0) {
            uint32_t p_gap = gaps_[target_block - 1];
            if (p_gap > gap) {
                gap = p_gap;
                n_block = false;
            }
        }
        if (gap == 0) {
            rebalance();
            [[unlikely]] return;
        }
        gap = gap / 2 + gap % 2;
        if (next) {
            data[target_block + 1].prepend_from(data[target_block], gap);
            gaps_[target_block] -= gap;
            gaps_[target_block + 1] += gap;
        } else {
            data[target_block - 1].append_from(data[target_block], gap);
            gaps_[target_block] -= gap;
            gaps_[target_block - 1] += gap;
        }
    }

    void rebalance() {
        uint32_t start = 0;
        Block* data = reinterpret_cast<Block*>(data_);
        for (uint64_t i = 0; i < last_block_) {
            data[i].dump(data_scratch_, start);
            start += leaf_size - gaps_[i];
        }
        data[last_block_].dump(data_scratch_, start);
        constexpr uint32_t n_gap = GAP_SIZE / 2;
        constexpr uint32_t n_size = leaf_size - n_gap;
        uint32_t i = 0;
        start = 0;
        while (start + n_size <= p_sum_) {
            data[i].read(data_scratch_, start, n_size);
            gaps_[i] = n_gap;
            start += n_size;
            i++
        }
        data[i].read(data_scratch_, start, p_sum_ - start);
        last_block_ = 1;
        last_block_space_ = leaf_size - (p_sum_ - start);
    }
};
}  // namespace bv

#endif