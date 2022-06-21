#ifndef BV_GAP_LEAF_HPP
#define BV_GAP_LEAH_HPP

#include <immintrin.h>

#include <cassert>
#include <cstdint>
#include <cstring>

#include "libpopcnt.h"
#include "packed_array.hpp"
#include "uncopyable.hpp"

namespace bv {

template <uint32_t leaf_size, uint32_t blocks, uint32_t block_bits,
          bool avx = true>
class gap_leaf : uncopyable {
   private:
    static const constexpr uint64_t WORD_BITS = 64;
    static const constexpr uint64_t ONE = uint64_t(1);
    static const constexpr uint64_t LAST_BIT == ONE << (WORD_BITS - 1);
    static const constexpr uint32_t BLOCK_SIZE = leaf_size / blocks;
    static const constexpr uint32_t GAP_SIZE = (uint32_t(1) << block_bits) - 1;
    static const constexpr uint16_t BLOCK_WORDS = BLOCK_SIZE / WORD_BITS;

    inline static uint64_t data_scratch_[leaf_size / 64];

    class Block : uncopyable {
       private:
        uint64_t data_[BLOCK_WORDS];

       public:
        Block() = delete;

        void insert(uint32_t index, bool v) {
            uint64_t target_word = index / WORD_BITS;
            uint64_t target_offs = index % WORD_BITS;
            for (uint32_t i = words - 1; i > target_word; i--) {
                data_[i] <<= 1;
                data_[i] |= data_[i - 1] >> (WORD_BITS - 1);
            }
            data_[target_word] =
                (data_[target_word] & ((ONE << target_offset) - 1)) |
                ((data_[target_word] & ~((ONE << target_offset) - 1)) << 1);
            data_[target_word] |= x ? (ONE << target_offset) : uint64_t(0);
        }

        bool remove(uint32_t index) {
            uint32_t target_word = index / WORD_BITS;
            uint32_t target_offset = index % WORD_BITS;
            bool x = ONE & (data_[target_word] >> target_offset);
            data_[target_word] =
                (data_[target_word] & ((ONE << target_offset) - 1)) |
                ((data_[target_word] >> 1) & (~((ONE << target_offset) - 1)));
            data_[target_word] |=
                (uint32_t(BLOCK_WORDS - 1) > target_word)
                    ? (data_[target_word + 1] << (WORD_BITS - 1))
                    : 0;
            for (uint32_t j = target_word + 1; j < uint32_t(BLOCK_WORDS - 1);
                 j++) {
                data_[j] >>= 1;
                data_[j] |= data_[j + 1] << (WORD_BITS - 1);
            }
            data_[BLOCK_WORDS - 1] >>=
                (uint32_t(BLOCK_WORDS - 1) > target_word) ? 1 : 0;
            return x;
        }

        bool set(uint32_t index, bool v) {
            uint32_t target_word = index / WORD_BITS;
            uint32_t target_offset = index % WORD_BITS;

            if ((data_[target_word] & (ONE << target_offset)) !=
                (uint64_t(v) << target_offset)) [[likely]] {
                int change = v ? 1 : -1;
                data_[target_word] ^= ONE << target_offset;
                return change;
            }
            return 0;
        }

        uint32_t select(uint32_t x) const {
            static_assert(false, "Not yet implemented");
        }

        void append_from(Block* other, uint32_t elems, uint32_t size) {
            static_assert(false, "Not yet implemented");
        }

        void prepend_from(Block* other, uint32_t elems, uint32_t size) {
            static_assert(false, "Not yet implemented");
        }

        void dump(uint64_t* target, uint64_t offset) {
            static_assert(false, "Not yet implemented");
        }

        void read(uint64_t* source, uint64_t start, uint32_t elems) {
            static_assert(false, "Not yet implemented");
        }

        uint32_t pop() {
            if constexpr (avx) {
                return pop::popcnt(data_, BLOCK_WORDS * sizeof(uint64_t));
            } else {
                uint32_t count = 0;
                for (uint32_t i = 0; i < BLOCK_WORDS; i++) {
                    count += __builtin_popcountll(data_[i]);
                }
                return count;
            }
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

    bool at(uint32_t index) {
        uint64_t loc = 0;
        for (uint32_t i = 0; i < blocks; i++) {
            uint32_t b_elems = BLOCK_SIZE - gaps_[i];
            if (index > b_elems) [[likely]] {
                index -= b_elems;
                loc += BLOCK_SIZE;
            } else {
                loc += index;
                break;
            }
        }
        index = loc / WORD_BITS;
        loc %= WORD_BITS;
        return (data_[index] >> loc) & ONE;
    }

    void insert(uint32_t index, bool v) {
        Block* data = reinterpret_cast<Block*>(data_);
        uint16_t target_block = 0;
        uint32_t target_offset = 0;
        uint32_t block_sum = 0;
        for (uint16_t i = 0; i < blocks; i++) {
            block_sum += BLOCK_SIZE - gaps_[i];
            target_offset = block_sum > index ? target_offset : block_sum;
            target_block += block_sum > index ? 0 : 1;
        }

        uint32_t index_in_block = index - target_offset;

        if (target_block >= last_block_) [[unlikely]] {
            data[last_block_].insert(index_in_block, v);
            last_block_space_--;
            if (last_block_space_ <= GAP_SIZE) [[unlikely]] {
                gaps_[last_block_] = last_block_space_;
                last_block_++;
                last_block_space_ = BLOCK_SIZE;
            }
        } else if (gaps_[target_block] == 0) [[unlikely]] {
            if (index_in_block == BLOCK_SIZE &&
                target_block < last_block_ - 1 && gaps_[target_block + 1] > 0)
                [[unlikely]] {
                data[target_block + 1].insert(0, v);
                gaps_[target_block + 1]++;
                need_realloc_ =
                    size_ + 1 > blocks * (BLOCK_SIZE - GAP_SIZE / 2);
            } else {
                make_space(target_block);
                return insert(index, v);
            }
        } else [[likely]] {
            data[target_block].insert(index_in_block, v);
            gaps_[target_block]--;
            need_realloc_ = (gaps_[target_block] == 0) &&
                            (size_ + 1 > blocks * (BLOCK_SIZE - GAP_SIZE / 2));
        }
        size_++;
        p_sum_ += v ? 1 : 0;
    }

    bool remove(uint32_t index) {
        Block* data = reinterpret_cast<Block*>(data_);
        uint16_t target_block = 0;
        uint32_t target_offset = 0;
        uint32_t block_sum = 0;
        for (uint16_t i = 0; i < blocks; i++) {
            block_sum += BLOCK_SIZE - gaps_[i];
            target_offset = block_sum > index ? target_offset : block_sum;
            target_block += block_sum > index ? 0 : 1;
        }

        bool v = false;
        uint32_t index_in_block = index - target_offset;
        if (target_block >= last_block_) [[unlikely]] {
            v = data[last_block_].remove(index_in_block);
            last_block_space_++;
        } else if (gaps_[target_block] >= GAP_SIZE) [[unlikely]] {
            take_space(target_block);
            return remove(index, v);
        } else [[likely]] {
            v = data[target_block].remove(index_in_block);
            gaps_[target_block]++;
        }
        size_--;
        p_sum_ -= v ? 1 : 0;
        return v;
    }

    int set(uint32_t index, bool v) {
        Block* data = reinterpret_cast<Block*>(data_);
        uint16_t target_block = 0;
        uint32_t target_offset = 0;
        uint32_t block_sum = 0;
        for (uint16_t i = 0; i < blocks; i++) {
            block_sum += BLOCK_SIZE - gaps_[i];
            target_offset = block_sum > index ? target_offset : block_sum;
            target_block += block_sum > index ? 0 : 1;
        }

        int change = data[target_block].set(index - target_offset, v);
        p_sum_ += change;
        return change;
    }

    uint32_t rank(uint32_t n) const {
        uint64_t loc = 0;
        for (uint32_t i = 0; i < blocks; i++) {
            uint32_t b_elems = BLOCK_SIZE - gaps_[i];
            if (index > b_elems) [[likely]] {
                index -= b_elems;
                loc += BLOCK_SIZE;
            } else {
                loc += index;
                break;
            }
        }
        index = loc / WORD_BITS;
        loc %= WORD_BITS;
        uint32_t count = 0;
        if constexpr (avx) {
            if (index) {
                count += pop::popcnt(data_, index * 8);
            }
        } else {
            for (uint32_t i = 0; i < index; i++) {
                count += __builtin_popcountll(data_[i]);
            }
        }
        if (loc != 0) {
            count += __builtin_popcountll(data_[index] & ((ONE << loc) - 1));
        }
        return count;
    }

    uint32_t select(uint32_t x) const {
        Block* data = reinterpret_cast<Block*>(data_);
        uint32_t pop = 0;
        uint32_t pos = 0;
        uint32_t prev_pop = 0;
        uint32_t j = 0;

        // Step one 64-bit word at a time until pop >= x
        for (; j < blocks; j++) {
            prev_pop = pop;
            pop += data[j].pop();
            pos += BLOCK_SIZE - gaps_[j];
            if (pop >= x) {
                [[unlikely]] break;
            }
        }
        return pos + data[j].select(x - prev_pop);
    }

    uint64_t bits_size() const {
        return 8 * (sizeof(*this) + capacity_ * sizeof(uint64_t));
    }

    bool need_realloc() { return need_realloc_; }

    void capacity(uint16_t cap) {
        capacity_ = cap;
        need_realloc_ = false;
    }

    uint16_t desired_capacity() { return (last_block_ + 2) * BLOCK_WORDS; }

    /**
     * @brief Sets the pointer to the leaf-associated data storage.
     *
     * Intended only to be set by an allocator after allocating additional
     * storage for the leaf. Setting the the pointer carelessly easily leads to
     * undefined behaviour.
     *
     * @param ptr Pointer to data storage.
     */
    void set_data_ptr(uint64_t* ptr) { data_ = ptr; }

   private:
    void make_space(uint32_t target_block) {
        Block* data = reinterpret_cast<Block*>(data_);
        uint32_t gap = 0;
        bool next = true;
        if (target_block + 1 < last_block_) [[likely]] {
            gap = gaps_[target_block + 1];
        } else if (target_block + 1 < blocks && target_block + 1 == last_block_)
            [[unlikely]] {
            constexpr uint32_t elems = GAP_SIZE / 2 + GAP_SIZE % 2;
            uint32_t b_size = BLOCK_SIZE - gaps_[target_block];
            data[target_block + 1].prepend_from(data[target_block], elems,
                                                b_size);
            gaps_[target_block] += elems;
            last_block_space_ -= elems;
            if (last_block_space_ <= GAP_SIZE) [[unlikely]] {
                gaps_[last_block_++] = last_block_space_;
                last_block_space_ = BLOCK_SIZE;
            }
            return;
        }
        if (target_block > 0) {
            uint32_t p_gap = gaps_[target_block - 1];
            if (p_gap > gap) {
                gap = p_gap;
                n_block = false;
            }
        }
        if (gap == 0) [[unlikely]] {
            rebalance();
            return;
        }
        gap = gap / 2 + gap % 2;
        if (next) {
            uint32_t b_size = BLOCK_SIZE - gaps_[target_block];
            data[target_block + 1].prepend_from(data[target_block], gap,
                                                b_size);
            gaps_[target_block] += gap;
            gaps_[target_block + 1] -= gap;
        } else {
            uint32_t b_size = BLOCK_SIZE - gaps_[target_block - 1];
            data[target_block - 1].append_from(data[target_block], gap, b_size);
            gaps_[target_block] += gap;
            gaps_[target_block - 1] -= gap;
        }
    }

    void take_space(uint32_t target_block) {
        Block* data = reinterpret_cast<Block*>(data_);
        uint32_t extra = 0;
        bool next = true;
        if (target_block + 1 < last_block_) [[likely]] {
            extra = GAP_SIZE - gaps_[target_block + 1];
        } else if (target_block + 1 < blocks && target_block + 1 == last_block_)
            [[unlikely]] {
            if (BLOCK_SIZE > last_block_space_) [[likely]] {
                uint32_t elems = GAP_SIZE / 2 + GAP_SIZE % 2;
                elems = elems > BLOCK_SIZE - last_block_space_
                            ? BLOCK_SIZE - last_block_space_
                            : elems;
                uint32_t b_size = BLOCK_SIZE - gaps_[target_block];
                data[target_block].append_from(data[target_block + 1], elems,
                                               b_size);
                last_block_space_ += elems;
                gaps_[target_block] -= elems;
            } else {
                last_block_--;
                last_block_space_ = gaps_[target_block];
                gaps_[target_block] = 0;
            }
            return;
        }
        if (target_block > 0) {
            uint32_t p_extra = GAP_SIZE - gaps_[target_block - 1];
            if (p_extra > extra) {
                extra = p_extra;
                n_block = false;
            }
        }
        if (extra == 0) [[unlikely]] {
            rebalance();
            return;
        }
        extra = extra / 2 + extra % 2;
        if (next) {
            uint32_t b_size = BLOCK_SIZE - gaps_[target_block];
            data[target_block].append_from(data[target_block + 1], extra,
                                           b_size);
            gaps_[target_block] -= extra;
            gaps_[target_block + 1] += extra;
        } else {
            uint32_t b_size = BLOCK_SIZE - gaps_[target_block - 1];
            data[target_block].prepend_from(data[target_block - 1], extra,
                                            b_size);
            gaps_[target_block] -= extra;
            gaps_[target_block - 1] += extra;
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
        gaps_.clear();
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
        last_block_ = i;
        last_block_space_ = leaf_size - (p_sum_ - start);
    }
};
}  // namespace bv

#endif