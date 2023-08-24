#ifndef BV_GAP_LEAF_HPP
#define BV_GAP_LEAF_HPP

#include <immintrin.h>

#include <cassert>
#include <cstdint>
#include <cstring>
#include <bitset>

#include "libpopcnt.h"
#include "packed_array.hpp"
#include "uncopyable.hpp"

namespace bv {

template <uint32_t leaf_size, uint32_t blocks, uint32_t block_bits, bool avx = true>
class gap_leaf : uncopyable {
   public:
    static const constexpr uint64_t WORD_BITS = 64;
    static const constexpr uint64_t ONE = uint64_t(1);
    static const constexpr uint32_t BLOCK_SIZE = leaf_size / blocks;
    static const constexpr uint32_t GAP_SIZE = (uint32_t(1) << block_bits) - 1;
    static const constexpr uint16_t BLOCK_WORDS = BLOCK_SIZE / WORD_BITS;
    static constexpr uint32_t init_capacity(uint32_t elems = 0) {
        return BLOCK_WORDS * (1 + elems / (BLOCK_SIZE - ((1 + GAP_SIZE) / 2)));
    }

   private:
    inline static uint64_t data_scratch_[leaf_size / WORD_BITS];

    class Block : uncopyable {
       private:
        uint64_t data_[BLOCK_WORDS];

       public:
        Block() = delete;

        void insert(uint32_t index, bool v) {
            uint64_t target_word = index / WORD_BITS;
            uint64_t target_offset = index % WORD_BITS;
            for (uint32_t i = BLOCK_WORDS - 1; i > target_word; i--) {
                data_[i] <<= 1;
                data_[i] |= data_[i - 1] >> (WORD_BITS - 1);
            }
            data_[target_word] =
                (data_[target_word] & ((ONE << target_offset) - 1)) |
                ((data_[target_word] & ~((ONE << target_offset) - 1)) << 1);
            data_[target_word] |= v ? (ONE << target_offset) : uint64_t(0);
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

        int set(uint32_t index, bool v) {
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

        void delete_first(uint32_t elems) {
            uint32_t target_word = 0;
            uint32_t source_word = elems / WORD_BITS;
            uint32_t source_offset = elems % WORD_BITS;
            if (source_word) {
                std::memmove(data_, data_ + source_word, (BLOCK_WORDS - source_word) * sizeof(uint64_t));
                std::memset(data_ + BLOCK_WORDS - target_word, 0, (BLOCK_WORDS - target_word) * sizeof(uint64_t));
            }
            if (source_offset) {
                for (uint32_t i = 0; i < BLOCK_WORDS - 1; i++) {
                    data_[i] = (data_[i] >> source_offset) | (data_[i + 1] << (WORD_BITS - source_offset));
                }
                data_[BLOCK_WORDS - 1] >>= source_offset;
            }
        }

        void append_from(Block* other, uint32_t elems, uint32_t size) {
            uint32_t to_del = elems;
            uint32_t source_word = 0;
            uint32_t target_word = size / WORD_BITS;
            uint64_t target_offset = size % WORD_BITS;
            uint64_t word = other->data_[source_word];
            while (elems >= WORD_BITS) {
                if (target_offset == 0) [[unlikely]] {
                    data_[target_word++] = word;
                } else {
                    data_[target_word++] |= word << target_offset;
                    data_[target_word] |= word >> (WORD_BITS - target_offset);
                }
                word = other->data_[++source_word];
                elems -= WORD_BITS;
            }
            if (elems > 0) [[likely]] {
                word &= (ONE << elems) - 1;
                data_[target_word++] |= word << target_offset;
                if (target_word < BLOCK_WORDS) {
                    data_[target_word] |= word << (WORD_BITS - target_offset);
                }
            }
            other->delete_first(to_del);
        }

        void delete_from(uint32_t size) {
            uint32_t target_word = size / WORD_BITS;
            uint32_t target_offset = size % WORD_BITS;
            data_[target_word++] &= (ONE << target_offset) - 1;
            while (target_word < BLOCK_WORDS) {
                data_[target_word++] = 0;
            }
        }

        void prepend_from(Block* other, uint32_t elems, uint32_t size) {
            uint32_t target_word = elems / WORD_BITS;
            if (target_word) {
                memmove(data_ + target_word, data_, (BLOCK_WORDS - target_word) * sizeof(uint64_t));
            }
            uint64_t target_offset = elems % WORD_BITS;
            if (target_offset) [[likely]] {
                for (uint32_t i = BLOCK_WORDS - 1; i > target_word; i--) {
                    data_[i] = (data_[i] << target_offset) | (data_[i - 1] >> (WORD_BITS - target_offset));
                }
                data_[target_word] <<= target_offset;
            }
            size -= elems;
            uint32_t source_word = size / WORD_BITS;
            uint64_t source_offset = size % WORD_BITS;
            target_word = 0;
            uint64_t word;
            while (elems) {
                if (source_offset) [[likely]] {
                    word = other->data_[source_word++] >> source_offset;
                    word |= (source_word < BLOCK_WORDS ? other->data_[source_word] : uint64_t(0)) << (WORD_BITS - source_offset);
                } else {
                    word = other->data_[source_word++];
                }
                if (elems < WORD_BITS) break;
                data_[target_word++] = word;
                elems -= 64;
            }
            if (elems) {
                data_[target_word] |= word;
            }
            other->delete_from(size);
        }

        void dump(uint64_t* target, uint64_t offset) {
            uint32_t target_word = offset / WORD_BITS;
            offset %= WORD_BITS;
            if (offset == 0) [[unlikely]] {
                memcpy(target + target_word, data_, BLOCK_WORDS * sizeof(uint64_t));
            } else {
                for (size_t i = 0; i < BLOCK_WORDS; i++) {
                    target[target_word++] |= data_[i] << offset;
                    target[target_word] = data_[i] >> (WORD_BITS - offset);
                }
            }
        }

        void read(uint64_t* source, uint64_t start, uint32_t elems) {
            memset(data_, 0, BLOCK_WORDS * sizeof(uint64_t));
            uint32_t source_word = start / WORD_BITS;
            uint64_t source_offset = start % WORD_BITS;
            uint64_t target_offset = elems % WORD_BITS;
            uint32_t n_words = elems / WORD_BITS + (target_offset ? 1 : 0);
            if (source_offset == 0)  [[unlikely]] {
                memcpy(data_, source + source_word, n_words * sizeof(uint64_t));
            } else {
                for (uint32_t i = 0; i < n_words; i++) {
                    data_[i] = source[source_word++] >> source_offset;
                    data_[i] |= source[source_word] << (WORD_BITS - source_offset);
                }
            }
            if (target_offset) {
                data_[n_words - 1] &= (ONE << target_offset) - 1;
            }
        }

        bool validate(uint32_t gap) {
#ifdef DEBUG
            uint32_t full_words = gap / WORD_BITS;
            for (uint32_t i = 0; i < full_words; i++) {
                if (data_[BLOCK_WORDS - 1 - i] != 0) {
                    std::cerr << "Word " << BLOCK_WORDS - 1 - i << " is not zero. (gap = " << gap << ")." << std::endl;
                    return false;
                }
            }
            gap %= WORD_BITS;
            if (gap) {
                if ((data_[BLOCK_WORDS - full_words - 1] >> (WORD_BITS - gap)) != 0) {
                    std::cerr << "Word: " << BLOCK_WORDS - full_words - 1 << ", with gap " << gap << std::endl;
                    return false;
                }
            }
#endif
            return true;
        } 
    };

    uint64_t* data_;
    packed_array<blocks, block_bits> gaps_;
    uint32_t size_;
    uint32_t p_sum_;
    uint16_t capacity_;
    uint16_t last_block_;
    uint16_t last_block_space_;

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
#pragma GCC diagnostic ignored "-Wunused-parameter"
    gap_leaf(uint16_t capacity, uint64_t* data, uint32_t elems = 0,
             bool val = false)
        : data_(data),
          gaps_(),
          size_(0),
          p_sum_(0),
          capacity_(capacity),
          last_block_(0),
          last_block_space_(leaf_size / blocks) {
        assert(elems == 0);
        assert(val == false);
    }
#pragma GCC diagnostic pop

    /** @brief Getter for p_sum_ */
    uint32_t p_sum() const { return p_sum_; }
    /** @brief Getter for size_ */
    uint32_t size() const { return size_; }

    bool at(uint32_t index) const {
        uint64_t loc = 0;
        for (uint32_t i = 0; i < blocks; i++) {
            uint32_t b_elems = BLOCK_SIZE - gaps_[i];
            if (index >= b_elems) [[likely]] {
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
        for (uint16_t i = 0; i < last_block_; i++) {
            block_sum += BLOCK_SIZE - gaps_[i];
            target_offset = block_sum < index ? block_sum : target_offset;
            target_block += block_sum < index ? 1 : 0;
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
                gaps_[target_block + 1]--;
            } else {
                make_space(target_block);
                return insert(index, v);
            }
        } else [[likely]] {
            data[target_block].insert(index_in_block, v);
            gaps_[target_block]--;

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
            return remove(index);
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
        uint32_t index = n;
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
        uint32_t pop = 0;
        uint32_t pos = 0;
        uint32_t prev_pop = 0;
        uint32_t j = 0;

        for (; j < capacity_; j++) {
            prev_pop = pop;
            pop += __builtin_popcountll(data_[j]);
            pos += WORD_BITS;
            if (pop >= x) {
                [[unlikely]] break;
            }
        }
        pos -= WORD_BITS;
        uint64_t add_loc = x - prev_pop - 1;
        add_loc = ONE << add_loc;
        pos += 63 - __builtin_clzll(_pdep_u64(add_loc, data_[j]));
        j = 0;
        add_loc = BLOCK_SIZE - gaps_[j];
        while (add_loc <= pos) {
            pos -= gaps_[j];
            add_loc += BLOCK_SIZE - gaps_[++j];
        }
        return pos;
    }

    uint64_t bits_size() const {
        return 8 * (sizeof(*this) + capacity_ * sizeof(uint64_t));
    }

    bool need_realloc() const { 
        if (size_ >= (capacity_ / BLOCK_WORDS) * (BLOCK_SIZE - (1 + GAP_SIZE) / 2)) {
            for (uint32_t i = 0; i < blocks; i++) {
                if (gaps_[i] == 0) {
                    return true;
                }
            }
        }
        return false;
    }

    uint16_t capacity() const {
        return capacity_;
    }

    void capacity(uint16_t cap) {
        capacity_ = cap;
    }

    uint16_t desired_capacity() const { return init_capacity(size_); }

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

    /**
     * @brief Provides access to raw leaf-associated data
     *
     * Intended for use when rebalancing and splitting leaves.
     *
     * @return Pointer to raw leaf data.
     */
    const uint64_t* data() { return data_; }

    std::ostream& print(bool internal_only = true, std::ostream& out = std::cout) const {
        out << "{\n\"type\": \"gap leaf\",\n"
                  << "\"size\": " << size_ << ",\n"
                  << "\"capacity\": " << capacity_ << ",\n"
                  << "\"p_sum\": " << p_sum_ << ",\n"
                  << "\"last block\": " << last_block_ << ",\n"
                  << "\"last block space\": " << last_block_space_ << ",\n"
                  << "\"max gap\": " << GAP_SIZE << ",\n";
        if (!internal_only) {
            out << "\"blocks\": [\n";
            for (uint32_t i = 0; i < capacity_ / BLOCK_WORDS; i++) {
                out << "\"block\": {\n"
                          << "\"gap\": " << gaps_.at(i) << "\n"
                          << "\"data\": [\n";
                for (uint16_t wi = 0; wi < BLOCK_WORDS; wi++) {
                    std::bitset<64> b(data_[i * BLOCK_WORDS + wi]);
                    out << "\"" << b << "\"" << (wi + 1 < BLOCK_WORDS ? "," : "]}") << "\n";
                }
                out << (i + 1 < capacity_ / BLOCK_WORDS ? "," : "]") << "\n";
            }
        }
        out << "}";
        return out;
    }

    uint32_t validate() {
        assert(size_ <= capacity_ * WORD_BITS);
        assert(p_sum_ <= size_);
        Block* data = reinterpret_cast<Block*>(data_);
        for (uint32_t i = 0; i < last_block_; i++) {
            assert(data[i].validate(gaps_[i]));
        }
        for (uint32_t i = last_block_; i < blocks; i++) {
            assert(gaps_[i] == 0);
        }
        if (capacity_ / BLOCK_WORDS > last_block_) {
            assert(data[last_block_].validate(last_block_space_));
        }
        assert(p_sum_ == rank(size_));
        return 1;
    }

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
            data[target_block + 1].prepend_from(data + target_block, elems,
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
                next = false;
            }
        }
        if (gap == 0) [[unlikely]] {
            rebalance();
            return;
        }
        gap = gap / 2 + gap % 2;
        if (next) {
            data[target_block + 1].prepend_from(data + target_block, gap,
                                                BLOCK_SIZE);
            gaps_[target_block] += gap;
            gaps_[target_block + 1] -= gap;
        } else {
            uint32_t b_size = BLOCK_SIZE - gaps_[target_block - 1];
            data[target_block - 1].append_from(data + target_block, gap, b_size);
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
                data[target_block].append_from(data + target_block + 1, elems,
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
                next = false;
            }
        }
        if (extra == 0) [[unlikely]] {
            rebalance();
            return;
        }
        extra = extra / 2 + extra % 2;
        if (next) {
            uint32_t b_size = BLOCK_SIZE - gaps_[target_block];
            data[target_block].append_from(data + target_block + 1, extra,
                                           b_size);
            gaps_[target_block] -= extra;
            gaps_[target_block + 1] += extra;
        } else {
            uint32_t b_size = BLOCK_SIZE - gaps_[target_block - 1];
            data[target_block].prepend_from(data + target_block - 1, extra,
                                            b_size);
            gaps_[target_block] -= extra;
            gaps_[target_block - 1] += extra;
        }
    }

    void rebalance() {
        uint32_t start = 0;
        Block* data = reinterpret_cast<Block*>(data_);
        for (uint64_t i = 0; i < last_block_; i++) {
            data[i].dump(data_scratch_, start);
            start += BLOCK_SIZE - gaps_[i];
        }
        data[last_block_].dump(data_scratch_, start);
        gaps_.clear();
        constexpr uint32_t n_gap = (1 + GAP_SIZE) / 2;
        constexpr uint32_t n_size = BLOCK_SIZE - n_gap;
        uint32_t i = 0;
        start = 0;
        while (start + n_size <= size_) {
            data[i].read(data_scratch_, start, n_size);
            gaps_[i] = n_gap;
            start += n_size;
            i++;
        }
        data[i].read(data_scratch_, start, size_ - start);
        last_block_ = i;
        last_block_space_ = BLOCK_SIZE - (size_ - start);
    }
};
}  // namespace bv

#endif