#ifndef BV_LEAF_HPP
#define BV_LEAF_HPP

#include <immintrin.h>

#include <bitset>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <utility>

//#include "deb.hpp"
#include "libpopcnt.h"
#include "uncopyable.hpp"

namespace bv {

/**
 * @brief Simple flat dynamic bit vector for use as a leaf for a dynamic b-tree
 * bit vector.
 *
 * The leaf is not usable as fully featured bit vector by itself since the leaf
 * cannot reallocate itself when full. The leaf requires a parent structure
 * (like bv::bit_vector or bv::node) to manage reallocations and rebalancing.
 *
 * ### Practical limitations
 *
 * The maximum leaf size for a buffered leaf without risking undefined behaviour
 * is \f$2^{24} - 1\f$ due to buffer elements storing reference indexes in
 * 24-bits of 32-bit integers. The practical upper limit of leaf size due to
 * performance concerns is no more than \f$2^{20}\f$ and optimal leaf size is
 * likely to be closer to the \f$2^{12}\f$ to \f$2^{15}\f$ range.
 *
 * Maximum leaf size can't in effect be non-divisible by 64, since 64-bit
 * integers are used for storage and a leaf "will use" all available words fully
 * before indicating that a reallocation is necessary, triggering limit checks.
 *
 * The maximum buffer size is 63 due to storing the information on buffer usage
 * with 6 bits of an 8-bit word, with 2 bits reserved for other purposes. This
 * could easily be "fixed", but testing indicates that bigger buffer sizes are
 * unlikely to be of practical use.
 *
 * @tparam Size of insertion/removal buffer.
 * @tparam Use avx population counts for rank operations.
 */
template <uint8_t buffer_size, uint32_t leaf_size, bool avx = true,
          bool compressed = false>
class leaf : uncopyable {
   private:
    uint8_t buffer_count_;  ///< Number of elements in insert/remove buffer.
    uint8_t type_info_;     ///< Internal metadata for compressed leaves.
    uint16_t capacity_;     ///< Number of 64-bit integers available in data.
    uint32_t size_;         ///< Logical number of bits stored.
    uint32_t p_sum_;        ///< Logical number of 1-bits stored.
#pragma GCC diagnostic ignored "-Wpedantic"
    uint32_t run_index_[compressed ? 1 : 0];  ///< next index to write for runs.
    uint32_t buffer_[buffer_size];            ///< Insert/remove buffer.
#pragma GCC diagnostic pop
    uint64_t* data_;  ///< Pointer to data storage.
    inline static uint64_t data_scratch[leaf_size / 64];

    /** @brief 0x1 to be used in  bit operations. */
    static const constexpr uint64_t MASK = 1;
    /** @brief Mask for accessing buffer value. */
    static const constexpr uint32_t VALUE_MASK = 1;
    /** @brief Mask for accessing buffer type. */
    static const constexpr uint32_t TYPE_MASK = 8;
    /** @brief Mask for accessing buffer index value. */
    static const constexpr uint32_t INDEX_MASK = ((uint32_t(1) << 8) - 1);
    /** @brief Number of bits in a computer word. */
    static const constexpr uint64_t WORD_BITS = 64;
    /** @brief Mask for accessing buffer value for compressed leaves */
    static const constexpr uint32_t C_INDEX = (~uint32_t(0)) >> 1;
    /** @brief Mask for initial value of rle compressed leaves */
    static const constexpr uint8_t C_ONE_MASK = 0b00000001;
    /** @brief Mask for accessing type of possibly compressed leaf */
    static const constexpr uint8_t C_TYPE_MASK = 0b00000010;
    static const constexpr uint8_t C_RUN_REMOVAL_MASK = 0b00000100;

    // The buffer fill rate is stored in part of an 8-bit word.
    // This could be resolved with using 16 bits instead (without increasing the
    // sizeof(leaf) value but there does not really appear to be any reason to
    // do so.
    static_assert(buffer_size < WORD_BITS);

    // Number of 32 bit buffer elements should be even to allow using buffer as
    // 64-bit elements if needed.
    static_assert(buffer_size % 2 == 0);

    // Hybrid compressed leaves should be buffered.
    static_assert(!compressed || buffer_size > 0);

   public:
    /**
     * @brief Leaf constructor
     *
     * The intention of this constructor is to enable either allocation of a
     * larger consecutive memory block where the leaf struct can be placed at
     * the start followed by the "data" section, or the data section can be
     * allocated separately.
     *
     * @param capacity Number of 64-bit integers available for use in data.
     * @param data     Pointer to a contiguous memory area available for
     * storage.
     */
    leaf(uint16_t capacity, uint64_t* data, uint32_t elems = 0,
         bool val = false) {
        if constexpr (!compressed) {
            if (elems > 0) {
                std::cerr << "Uncompressed leaf does not support instantiation "
                             "to non-empty"
                          << std::endl;
                exit(1);
            }
        } else {
            if (elems > ~VALUE_MASK) {
                std::cerr << "Maximum size for leaves is " << ~VALUE_MASK
                          << std::endl;
                exit(1);
            }
        }
        if constexpr (buffer_size > 0) {
            buffer_count_ = 0;
            memset(buffer_, 0, sizeof(buffer_));
        }
        capacity_ = capacity;
        type_info_ = 0;
        size_ = elems;
        p_sum_ = 0;
        if constexpr (compressed) {
            p_sum_ = val ? elems : p_sum_;
            run_index_[0] = 0;
        }
        data_ = data;
        if constexpr (compressed) {
            if (elems > 8) {
                write_run(elems);
                type_info_ |= C_TYPE_MASK;
                type_info_ =
                    val ? type_info_ | C_ONE_MASK : type_info_ & ~C_ONE_MASK;
            } else {
                data_[0] = (uint64_t(1) << elems) - 1;
            }
        }
    }

    /**
     * @brief Get the value of the i<sup>th</sup> element in the leaf.
     *
     * @param i Index to check
     *
     * @return Value of bit at index i.
     */
    bool at(uint32_t i) const {
        if constexpr (compressed) {
            if (is_compressed()) {
                return c_at(i);
            }
        }
        if constexpr (buffer_size != 0) {
            uint64_t index = i;
            for (uint8_t idx = 0; idx < buffer_count_; idx++) {
                uint64_t b = buffer_index(buffer_[idx]);
                if (b == i) {
                    if (buffer_is_insertion(buffer_[idx])) {
                        [[unlikely]] return buffer_value(buffer_[idx]);
                    }
                    index++;
                } else if (b < i) {
                    [[likely]] index -=
                        buffer_is_insertion(buffer_[idx]) * 2 - 1;
                }
            }
            return MASK & (data_[index / WORD_BITS] >> (index % WORD_BITS));
        }
        return MASK & (data_[i / WORD_BITS] >> (i % WORD_BITS));
    }

    /** @brief Getter for p_sum_ */
    uint32_t p_sum() const { return p_sum_; }
    /** @brief Getter for size_ */
    uint32_t size() const { return size_; }
    /** @brief Getter for number of buffer elements */
    uint8_t buffer_count() const { return buffer_count_; }
    /** @brief Get pointer to the buffer */
    uint32_t* buffer() { return buffer_; }
    /** @brief Get the values for the first run */
    bool first_value() {
        if constexpr (compressed) {
            if (is_compressed()) {
                return type_info_ & C_ONE_MASK;
            }
        }
        return at(0);
    }
    /** @brief Number of bytes used to encode content */
    uint32_t used_bytes() {
        if constexpr (compressed) {
            if (is_compressed()) {
                return run_index_[0];
            }
        }
        return size_ + (size_ % 8 ? 1 : 0);
    }

    /**
     * @brief Insert x into position i
     *
     * The leaf **cannot reallocate** so inserting into a full leaf is undefined
     * behaviour.
     *
     * If the the buffer is not full or the insertion is appending a new
     * element, the operation can be considered constant time. If the buffer is
     * full and the insertion is not appending, the operations will be linear in
     * the leaf size.
     *
     * @param i Insertion index
     * @param x Value to insert
     */
    void insert(uint32_t i, bool x) {
        if constexpr (compressed) {
            if (is_compressed()) {
                return c_insert(i, x);
            }
        }
#ifdef DEBUG
        if (size_ >= capacity_ * WORD_BITS) {
            std::cerr << "Overflow. Reallocate before adding elements"
                      << "\n";
            std::cerr << "Attempted to insert(" << i << ", " << x << ")"
                      << std::endl;
            std::cerr << "cap = " << capacity_ << ", size = " << size_
                      << std::endl;
            std::cerr << "compressed = " << compressed << std::endl;
            print(false);
            std::cerr << std::endl;
            assert(size_ < capacity_ * WORD_BITS);
        }
#endif
        if (i == size_) {
            // Convert to append if applicable.
            push_back(x);
            [[unlikely]] return;
        }
        p_sum_ += x ? 1 : 0;
        if constexpr (buffer_size != 0) {
            uint8_t idx = buffer_count_;
            while (idx > 0) {
                uint64_t b = buffer_index(buffer_[idx - 1]);
                if (b > i ||
                    (b == i && buffer_is_insertion(buffer_[idx - 1]))) {
                    [[likely]] set_buffer_index(b + 1, idx - 1);
                } else {
                    break;
                }
                idx--;
            }
            size_++;
            if (idx == buffer_count_) {
                buffer_[buffer_count_] = create_buffer(i, 1, x);
                [[likely]] buffer_count_++;
            } else {
                insert_buffer(idx, create_buffer(i, 1, x));
            }
            if (buffer_count_ >= buffer_size) [[unlikely]]
                commit();
        } else {
            // If there is no buffer, a simple linear time insertion is done
            // instead.
            size_++;
            uint32_t target_word = i / WORD_BITS;
            uint32_t target_offset = i % WORD_BITS;
            for (uint32_t j = capacity_ - 1; j > target_word; j--) {
                data_[j] <<= 1;
                data_[j] |= (data_[j - 1] >> 63);
            }
            data_[target_word] =
                (data_[target_word] & ((MASK << target_offset) - 1)) |
                ((data_[target_word] & ~((MASK << target_offset) - 1)) << 1);
            data_[target_word] |= x ? (MASK << target_offset) : uint64_t(0);
        }
    }

    /**
     * @brief Remove the i<sup>th</sup> bit from the leaf.
     *
     * No implicit balancing is possible in leaves without a managing parent
     * (bv::bit_vector or bv::node).
     *
     * If the the buffer is not full the operation can be considered constant
     * time. If the buffer is full, the operations will be linear in the leaf
     * size.
     *
     * @param i Index to remove
     *
     * @return Value of removed element.
     */
    bool remove(uint32_t i) {
        if constexpr (compressed) {
            if (is_compressed()) {
                return c_remove(i);
            }
        }
        if constexpr (buffer_size != 0) {
            bool x = this->at(i);
            p_sum_ -= x;
            --size_;
            uint8_t idx = buffer_count_;
            while (idx > 0) {
                uint32_t b = buffer_index(buffer_[idx - 1]);
                if (b == i) {
                    if (buffer_is_insertion(buffer_[idx - 1])) {
                        delete_buffer_element(idx - 1);
                        return x;
                    } else {
                        [[likely]] break;
                    }
                } else if (b < i) {
                    break;
                } else {
                    [[likely]] set_buffer_index(b - 1, idx - 1);
                }
                idx--;
            }
            if (idx == buffer_count_) {
                buffer_[idx] = create_buffer(i, 0, x);
                buffer_count_++;
            } else {
                [[likely]] insert_buffer(idx, create_buffer(i, 0, x));
            }
            if (buffer_count_ >= buffer_size) [[unlikely]]
                commit();
            return x;
        } else {
            // If buffer does not exits. A simple linear time removal is done
            // instead.
            uint32_t target_word = i / WORD_BITS;
            uint32_t target_offset = i % WORD_BITS;
            bool x = MASK & (data_[target_word] >> target_offset);
            p_sum_ -= x;
            data_[target_word] =
                (data_[target_word] & ((MASK << target_offset) - 1)) |
                ((data_[target_word] >> 1) & (~((MASK << target_offset) - 1)));
            data_[target_word] |= (uint32_t(capacity_ - 1) > target_word)
                                      ? (data_[target_word + 1] << 63)
                                      : 0;
            for (uint32_t j = target_word + 1; j < uint32_t(capacity_ - 1);
                 j++) {
                data_[j] >>= 1;
                data_[j] |= data_[j + 1] << 63;
            }
            data_[capacity_ - 1] >>=
                (uint32_t(capacity_ - 1) > target_word) ? 1 : 0;
            size_--;
            return x;
        }
    }

    /**
     * @brief Set the i<sup>th</sup> bit to x.
     *
     * The value is changed in constant time (if buffer size is considered a
     * constant). The change in data structure sum is returned for updateing
     * cumulative sums in ancestor objects.
     *
     * @param i Index of bit to modify
     * @param x New value for the i<sup>th</sup> element.
     *
     * @return \f$x - \mathrm{bv}[i]\f$
     */
    int set(uint32_t i, bool x) {
        if constexpr (compressed) {
            if (is_compressed()) {
                return c_set(i, x);
            }
        }
        uint32_t idx = i;
        if constexpr (buffer_size != 0) {
            // If buffer exists, the index needs for the underlying structure
            // needs to be modified. And if there exists an insertion to this
            // location in the buffer, the insertion can simply be amended.
            for (uint8_t j = 0; j < buffer_count_; j++) {
                uint32_t b = buffer_index(buffer_[j]);
                if (b < i) {
                    [[likely]] idx += buffer_is_insertion(buffer_[j]) ? -1 : 1;
                } else if (b == i) {
                    if (buffer_is_insertion(buffer_[j])) {
                        if (buffer_value(buffer_[j]) != x) {
                            int change = x ? 1 : -1;
                            p_sum_ += change;
                            buffer_[j] ^= VALUE_MASK;
                            [[likely]] return change;
                        }
                        [[likely]] return 0;
                    }
                    idx++;
                } else {
                    break;
                }
            }
        }
        uint32_t word_nr = idx / WORD_BITS;
        uint32_t pos = idx % WORD_BITS;

        if ((data_[word_nr] & (MASK << pos)) != (uint64_t(x) << pos)) {
            int change = x ? 1 : -1;
            p_sum_ += change;
            data_[word_nr] ^= MASK << pos;
            [[likely]] return change;
        }
        return 0;
    }

    /**
     * @brief Number of 1-bits up to position n.
     *
     * Counts the number of bits set in the first n bits.
     *
     * This is a simple linear operations of population counting.
     *
     * @param n Number of elements to include in the "summation".
     *
     * @return \f$\sum_{i = 0}^{\mathrm{index - 1}} \mathrm{bv}[i]\f$.
     */
    uint32_t rank(uint32_t n) const {
        if constexpr (compressed) {
            if (is_compressed()) {
                return c_rank(n);
            }
        }
        uint32_t count = 0;
        uint32_t idx = n;
        if constexpr (buffer_size != 0) {
            for (uint8_t i = 0; i < buffer_count_; i++) {
                if (buffer_index(buffer_[i]) >= n) {
                    [[unlikely]] break;
                }
                // Location of the n<sup>th</sup> element needs to be amended
                // base on buffer contents.
                if (buffer_is_insertion(buffer_[i])) {
                    idx--;
                    count += buffer_value(buffer_[i]);
                } else {
                    idx++;
                    count -= buffer_value(buffer_[i]);
                }
            }
        }
        uint32_t target_word = idx / WORD_BITS;
        uint32_t target_offset = idx % WORD_BITS;
        if constexpr (avx) {
            if (target_word) {
                count += pop::popcnt(data_, target_word * 8);
            }
        } else {
            for (uint32_t i = 0; i < target_word; i++) {
                count += __builtin_popcountll(data_[i]);
            }
        }
        if (target_offset != 0) {
            count += __builtin_popcountll(data_[target_word] &
                                          ((MASK << target_offset) - 1));
        }
        return count;
    }

    /**
     * @brief Number of 1-bits up to position n from position offset.
     *
     * Counts the number of bits set in the [offset n) range.
     *
     * This is a simple linear operations of population counting.
     *
     * @param n End position of summation.
     * @param offset Start position of summation.
     *
     * @return \f$\sum_{i = \mathrm{offset}}^{\mathrm{n- 1}} \mathrm{bv}[i]\f$.
     */
    uint32_t rank(uint32_t n, uint32_t offset) const {
        if (compressed) {
            if (is_compressed()) {
                return c_rank(n);
            }
        }
        uint32_t count = 0;
        uint32_t idx = n;
        uint32_t o_idx = offset;
        if constexpr (buffer_size != 0) {
            for (uint8_t i = 0; i < buffer_count_; i++) {
                uint32_t b = buffer_index(buffer_[i]);
                if (b >= n) {
                    [[unlikely]] break;
                }
                // Location of the n<sup>th</sup> element needs to be amended
                // base on buffer contents.
                if (buffer_is_insertion(buffer_[i])) {
                    if (b >= offset) {
                        count += buffer_value(buffer_[i]);
                    } else {
                        o_idx--;
                    }
                    idx--;
                } else {
                    if (b >= offset) {
                        count -= buffer_value(buffer_[i]);
                    } else {
                        o_idx++;
                    }
                    idx++;
                }
            }
        }
        uint32_t target_word = idx / WORD_BITS;
        uint32_t offset_word = o_idx / WORD_BITS;
        uint32_t target_offset = idx % WORD_BITS;
        uint32_t offset_offset = o_idx % WORD_BITS;
        if (target_word == offset_word) {
            count += __builtin_popcountll(data_[offset_word] &
                                          ~((MASK << offset_offset) - 1) &
                                          ((MASK << target_offset) - 1));
            return count;
        }
        if (offset_offset != 0) {
            count += __builtin_popcountll(data_[offset_word] &
                                          ~((MASK << offset_offset) - 1));
            offset_word++;
        }
        if constexpr (avx) {
            if (target_word - offset_word > 0) {
                count += pop::popcnt(data_ + offset_word,
                                     (target_word - offset_word) * 8);
            }
        } else {
            for (uint32_t i = offset_word; i < target_word; i++) {
                count += __builtin_popcountll(data_[i]);
            }
        }
        if (target_offset != 0) {
            count += __builtin_popcountll(data_[target_word] &
                                          ((MASK << target_offset) - 1));
        }
        return count;
    }

    /**
     * @brief Index of the x<sup>th</sup> 1-bit in the data structure
     *
     * Select is a simple (if somewhat slow) linear time operation.
     *
     * @param x Selection target.
     *
     * @return \f$\underset{i \in [0..n)}{\mathrm{arg min}}\left(\sum_{j = 0}^i
     * \mathrm{bv}[j]\right) = x\f$.
     */
    uint32_t select(uint32_t x) const {
        if constexpr (compressed) {
            if (is_compressed()) {
                return c_select(x);
            }
        }
        if constexpr (buffer_size == 0) {
            return unb_select(x);
        }
        if (buffer_count_ == 0) {
            return unb_select(x);
        }
        uint32_t pop = 0;
        uint32_t pos = 0;
        uint8_t current_buffer = 0;
        int8_t a_pos_offset = 0;
        int32_t b_index = -100;

        // Step one 64-bit word at a time considering the buffer until pop >= x
        for (uint32_t j = 0; j < capacity_; j++) {
            pop += __builtin_popcountll(data_[j]);
            pos += WORD_BITS;
            for (uint8_t b = current_buffer; b < buffer_count_; b++) {
                b_index = buffer_index(buffer_[b]);
                if (b_index < int32_t(pos)) {
                    if (buffer_is_insertion(buffer_[b])) {
                        pop += buffer_value(buffer_[b]);
                        pos++;
                        a_pos_offset--;
                    } else {
                        pop -=
                            (data_[(b_index + a_pos_offset) / WORD_BITS] >>
                                ((b_index + a_pos_offset) % WORD_BITS)) &
                            MASK;
                        pos--;
                        a_pos_offset++;
                    }
                    [[unlikely]] current_buffer++;
                } else {
                    [[likely]] break;
                }
                [[unlikely]] (void(0));
            }
            if (pop >= x) {
                [[unlikely]] break;
            }
        }

        current_buffer -= 1;
        b_index = current_buffer < buffer_count_
                        ? buffer_index(buffer_[current_buffer])
                        : -100;
        if ((b_index - 1 >= int32_t(pos) &&
                !buffer_is_insertion(buffer_[current_buffer])) ||
            (b_index >= int32_t(pos) &&
                buffer_is_insertion(buffer_[current_buffer]))) {
            current_buffer--;
            b_index = current_buffer < buffer_count_
                            ? buffer_index(buffer_[current_buffer])
                            : -100;
        }

        // Make sure we have not overshot the logical end of the structure.
        pos = size_ < pos ? size_ : pos;

        // Decrement one bit at a time until we can't anymore without going
        // under x.
        pos--;
        while (pop >= x && pos < capacity_ * WORD_BITS) {
            while (b_index - 1 == int32_t(pos) &&
                    !buffer_is_insertion(buffer_[current_buffer])) {
                a_pos_offset--;
                current_buffer--;
                b_index = current_buffer < buffer_count_
                                ? buffer_index(buffer_[current_buffer])
                                : -100;
                [[unlikely]] (void(0));
            }
            if (b_index == int32_t(pos) &&
                buffer_is_insertion(buffer_[current_buffer])) {
                pop -= buffer_value(buffer_[current_buffer]);
                a_pos_offset++;
                pos--;
                current_buffer--;
                b_index = current_buffer < buffer_count_
                                ? buffer_index(buffer_[current_buffer])
                                : -100;
                [[unlikely]] continue;
            }
            pop -= (data_[(pos + a_pos_offset) / WORD_BITS] >>
                    ((pos + a_pos_offset) % WORD_BITS)) &
                   MASK;
            pos--;
        }
        return ++pos;
    }

    /**
     * @brief Index of the x<sup>th</sup> 1-bit in the data structure starting
     * at `pos` with `pop`
     *
     * @param x Selection target.
     * @param pos Start position of Select calculation.
     * @param pop Start population as `pos`
     * @return \f$\underset{i \in [0..n)}{\mathrm{arg min}}\left(\sum_{j = 0}^i
     * \mathrm{bv}[j]\right) = x\f$.
     */
    uint32_t select(uint32_t x, uint32_t pos, uint32_t pop) const {
        if constexpr (compressed) {
            if (is_compressed()) {
                return c_select(x);
            }
        }
        uint8_t current_buffer = 0;
        int8_t a_pos_offset = 0;
        // Scroll the buffer to the start position and calculate offset.
        if constexpr (buffer_size != 0) {
            while (current_buffer < buffer_count_) {
                uint32_t b_index = buffer_index(buffer_[current_buffer]);
                if (b_index < pos) {
                    if (buffer_is_insertion(buffer_[current_buffer])) {
                        a_pos_offset--;
                    } else {
                        a_pos_offset++;
                    }
                    [[unlikely]] current_buffer++;
                } else {
                    [[likely]] break;
                }
            }
        }

        uint32_t pop_idx = 0;
        // Step to the next 64-bit word boundary
        if (pos + a_pos_offset > 0) {
            pop_idx = (pos + a_pos_offset) / WORD_BITS;
            uint32_t offset = (pos + a_pos_offset) % WORD_BITS;
#ifdef DEBUG
            if (pop_idx >= capacity_) {
                std::cerr << "Invalid select query apparently\n"
                          << "x = " << x << ", pos = " << pos
                          << ", pop = " << pop << "\npop_idx = " << pop_idx
                          << ", capacity_ = " << capacity_ << std::endl;
                assert(pop_idx < capacity_);
            }
#endif
            if (offset != 0) {
                pop += __builtin_popcountll(data_[pop_idx++] >> offset);
                pos += WORD_BITS - offset;
                if constexpr (buffer_size != 0) {
                    for (uint8_t b = current_buffer; b < buffer_count_; b++) {
                        uint32_t b_index = buffer_index(buffer_[b]);
                        if (b_index < pos) {
                            if (buffer_is_insertion(buffer_[b])) {
                                pop += buffer_value(buffer_[b]);
                                pos++;
                                a_pos_offset--;
                            } else {
                                pop -=
                                    (data_[(b_index + a_pos_offset) /
                                           WORD_BITS] >>
                                     ((b_index + a_pos_offset) % WORD_BITS)) &
                                    MASK;
                                pos--;
                                a_pos_offset++;
                            }
                            [[unlikely]] current_buffer++;
                        } else {
                            [[likely]] break;
                        }
                        [[unlikely]] (void(0));
                    }
                }
            }
        }

        // Step one 64-bit word at a time considering the buffer until pop >= x
        for (uint32_t j = pop_idx; j < capacity_; j++) {
            pop += __builtin_popcountll(data_[j]);
            pos += WORD_BITS;
            if constexpr (buffer_size != 0) {
                for (uint8_t b = current_buffer; b < buffer_count_; b++) {
                    uint32_t b_index = buffer_index(buffer_[b]);
                    if (b_index < pos) {
                        if (buffer_is_insertion(buffer_[b])) {
                            pop += buffer_value(buffer_[b]);
                            pos++;
                            a_pos_offset--;
                        } else {
                            pop -=
                                (data_[(b_index + a_pos_offset) / WORD_BITS] >>
                                 ((b_index + a_pos_offset) % WORD_BITS)) &
                                MASK;
                            pos--;
                            a_pos_offset++;
                        }
                        [[unlikely]] current_buffer++;
                    } else {
                        [[likely]] break;
                    }
                    [[unlikely]] (void(0));
                }
            }
            if (pop >= x) [[unlikely]]
                break;
        }

        // Make sure we have not overshot the logical end of the structure.
        pos = size_ < pos ? size_ : pos;

        // Decrement one bit at a time until we can't anymore without going
        // under x.
        pos--;
        while (pop >= x && pos < capacity_ * WORD_BITS) {
            pop -= at(pos);
            pos--;
        }
        return ++pos;
    }

    /**
     * @brief Index of the x<sup>th</sup> 0-bit in the data structure 
     * 
     * Select is a simple (if somewhat slow) linear time operation.
     * 
     * @param x Selection target
     * 
     * @return @return \f$\underset{i \in [0..n)}{\mathrm{arg min}}\left(\sum_{j = 0}^i
     * \mathrm{bv}[j]\right) = x\f$.
     */
    uint32_t select0(uint32_t x) const {
        if constexpr (compressed) {
            if (is_compressed()) {
                return c_select0(x);
            }
        }
        if constexpr (buffer_size == 0) {
            return unb_select0(x);
        }
        if (buffer_count_ == 0) {
            return unb_select0(x);
        }

        uint64_t pop = 0;
        uint64_t pos = 0;
        uint8_t current_buffer = 0;
        int8_t a_pos_offset = 0;
        int32_t b_index = -100;
        
        //Step one 64-bit word at a time considering the buffer until pop >= x
        for (uint16_t j = 0; j < capacity_; j++) {
            //Negate the 64-bit integer to get number of 0's
            pop += __builtin_popcountll(~data_[j]);
            pos += WORD_BITS;
            //Iterate buffer until position or end of the buffer
            if constexpr (buffer_size != 0) {
                for (uint8_t b = current_buffer; b < buffer_count_; b++) {
                    //buffer_index indicates the insertion/removal location of the operation
                    b_index = buffer_index(buffer_[b]);

                    if (b_index < int32_t(pos)) {
                        if (buffer_is_insertion(buffer_[b])) {
                            pop += 1-buffer_value(buffer_[b]);
                            pos++;
                            a_pos_offset--;

                        } else {
                            uint32_t desired_index = b_index + a_pos_offset;
                            pop -= 1-((data_[desired_index / WORD_BITS] >> 
                                    (desired_index % WORD_BITS)) &
                                    MASK);
                            pos--;
                            a_pos_offset++;
                        }
                        [[unlikely]] current_buffer++;
                    } else {
                        [[likely]] break;
                    }
                    [[unlikely]] (void(0));
                }
            }
            if (pop >= x) {
                [[unlikely]] break;
            }
        }
        
        // Make sure we have not overshot the logical end of the structure.
        if (pos > size_) {
            //Subtract unwanted zeros caused by going over the logical end
            //of the structure
            pop -= pos-size_;
            pos = size_;
        }

        current_buffer--;
        b_index = current_buffer < buffer_count_
                        ? buffer_index(buffer_[current_buffer])
                        : -100;

        // Decrement one bit at a time until we can't anymore without going
        // under x.
        pos--;
        while (pop >= x && pos < uint64_t(capacity_ * WORD_BITS)) {
            //Check if removals in the buffer and decrease offset
            while (!buffer_is_insertion(buffer_[current_buffer]) &&
                    b_index > int32_t(pos)) {
                a_pos_offset--;
                current_buffer--;
                b_index = current_buffer < buffer_count_
                            ? buffer_index(buffer_[current_buffer])
                            : -100;
                [[unlikely]] (void(0));
            }
            if (buffer_is_insertion(buffer_[current_buffer]) &&
                b_index == int32_t(pos)) {
                    pop -= 1-buffer_value(buffer_[current_buffer]);
                    a_pos_offset++;
                    pos--;
                    current_buffer--;
                    b_index = current_buffer < buffer_count_
                            ? buffer_index(buffer_[current_buffer])
                            : -100;
                    [[unlikely]] continue;
                }
            pop -= 1 - ((data_[(pos + a_pos_offset) / WORD_BITS] >>
                        ((pos + a_pos_offset) % WORD_BITS)) &
                        MASK);
            pos--;
        }

        return ++pos;
    }

    /**
     * @brief Index of the x<sup>th</sup> 0-bit in the data structure starting at `pos` with `pop`
     * 
     * @param x Selection target.
     * @param pos Start position of Select calculation.
     * @param pop Start population as `pos`
     * @return \f$\underset{i \in [0..n)}{\mathrm{arg min}}\left(\sum_{j = 0}^i
     * \mathrm{bv}[j]\right) = x\f$.
     */
    uint32_t select0(uint32_t x, uint32_t pos, uint32_t pop) const {
        if constexpr (compressed) {
            if (is_compressed()) {
                return c_select0(x);
            }
        }
        uint8_t current_buffer = 0;
        int8_t a_pos_offset = 0;
        int32_t b_index = -100;

        //Scroll the buffer to the start position and calculate offset.
        if constexpr (buffer_size != 0) {
            while (current_buffer < buffer_count_) {
                b_index = buffer_index(buffer_[current_buffer]);
                if (b_index < int32_t(pos)) {
                    if (buffer_is_insertion(buffer_[current_buffer])) {
                        a_pos_offset--;
                    } else {
                        a_pos_offset++;
                    }
                    [[unlikely]] current_buffer++;
                } else {
                    [[likely]] break;
                }
            }
        }

        //pop_idx = starting index in the data array
        uint32_t pop_idx = 0;

        // Step to the next 64-bit word boundary
        if (pos + a_pos_offset > 0) {
            pop_idx = (pos + a_pos_offset) / WORD_BITS; 
            uint32_t offset = (pos + a_pos_offset) % WORD_BITS;
            if (offset != 0) {
                pop += __builtin_popcountll((~data_[pop_idx++]) >> offset);
                pos += WORD_BITS - offset;
                if constexpr (buffer_size != 0) {
                    for (uint8_t b = current_buffer; b < buffer_count_; b++) {
                        b_index = buffer_index(buffer_[b]);
                        if (b_index < int32_t(pos)) {
                            if (buffer_is_insertion(buffer_[b])) {
                                pop += 1-buffer_value(buffer_[b]);
                                pos++;
                                a_pos_offset--;
                            } else {
                                pop -= 1-((data_[(b_index + a_pos_offset) / WORD_BITS] >> 
                                    ((b_index + a_pos_offset) % WORD_BITS)) &
                                    MASK);
                                pos--;
                                a_pos_offset++;
                            }
                            [[unlikely]] current_buffer++;
                        } else {
                            [[likely]] break;
                        }
                        [[unlikely]] (void(0));
                    }
                }
            }
        }

        //Step one 64-bit word at a time considering the buffer until pop >= x
        for (uint32_t j = pop_idx; j < capacity_; j++) {
            pop += __builtin_popcountll(~data_[j]);
            pos += WORD_BITS;
            if constexpr (buffer_size != 0) {
                for (uint8_t b = current_buffer; b < buffer_count_; b++) {
                    int32_t b_index = buffer_index(buffer_[b]);
                    if (b_index < int32_t(pos)) {
                        if (buffer_is_insertion(buffer_[b])) {
                            pop += 1-buffer_value(buffer_[b]);
                            pos++;
                            a_pos_offset--;
                        } else {
                            pop -= 1-((data_[(b_index + a_pos_offset) / WORD_BITS] >> 
                                    ((b_index + a_pos_offset) % WORD_BITS)) &
                                    MASK);
                            pos--;
                            a_pos_offset++;
                        }
                        [[unlikely]] current_buffer++;
                    } else {
                        [[likely]] break;
                    }
                    [[unlikely]] (void(0));
                }
            }
            if (pop >= x) [[unlikely]] {
                break;
            }
        }

        //Bits over the end of the logical structure are set as 0 which need to be subtracted
        if (pos > size_) {
            pop -= pos-size_;
            pos = size_;
        }

        current_buffer--;
        b_index = current_buffer < buffer_count_
                        ? buffer_index(buffer_[current_buffer])
                        : -100;

        // Decrement one bit at a time until we can't anymore without going
        // under x.
        pos--;
        while (pop >= x && pos < uint64_t(capacity_ * WORD_BITS)) {
            //Check if removals in the buffer and decrease offset
            while (!buffer_is_insertion(buffer_[current_buffer]) &&
                    b_index > int32_t(pos)) {
                a_pos_offset--;
                current_buffer--;
                b_index = current_buffer < buffer_count_
                                ? buffer_index(buffer_[current_buffer])
                                : -100;
                [[unlikely]] (void(0));
            }
            if (buffer_is_insertion(buffer_[current_buffer]) &&
                b_index == int32_t(pos)) {
                    pop -= 1-buffer_value(buffer_[current_buffer]);
                    a_pos_offset++;
                    pos--;
                    current_buffer--;
                    b_index = current_buffer < buffer_count_
                                ? buffer_index(buffer_[current_buffer])
                                : -100;
                    [[unlikely]] continue;
                }
            pop -= 1 - ((data_[(pos + a_pos_offset) / WORD_BITS] >>
                        ((pos + a_pos_offset) % WORD_BITS)) &
                        MASK);
            pos--;
        }

        return ++pos;
    }    

    /**
     * @brief Size of the leaf and associated data in bits.
     */
    uint64_t bits_size() const {
        return 8 * (sizeof(*this) + capacity_ * sizeof(uint64_t));
    }

    /**
     * @brief Determine if reallocation / splitting is required prior to
     * additional insertion of new elements (or modification).
     *
     * For uncompressed leaves, only insertions may require reallocation. For
     * compressed leaves modification may also require modification as
     * modifications are typically converted into a remove operation followed by
     * an insertion, and the type of encoding used may cause additional space to
     * be required.
     *
     * @return True if there is no guarantee that an insertion/modification can
     * be completed without undefined behaviour.
     */
    bool need_realloc() {
        //if (compressed && do_debug) {
        //    std::cerr << "need_realloc called" << std::endl;
        //}
        if constexpr (compressed) {
            if (is_compressed()) {
                if (size_ >= (~uint32_t(0)) >> 1) {
                    //if (compressed && do_debug) {
                    //    std::cerr << " Size limit reached" << std::endl;
                    //}
                    [[unlikely]] return true;
                }
                if (buffer_count_ < buffer_size - 1) {
                    //if (compressed && do_debug) {
                    //    std::cerr << " Buffer has room" << std::endl;
                    //}
                    [[likely]] return false;
                }
                if (type_info_ & C_RUN_REMOVAL_MASK) {
                    //if (compressed && do_debug) {
                    //    std::cerr << " Attempting to clean up runs" << std::endl;
                    //}
                    c_commit<false>();
                }
                //if (compressed && do_debug) {
                //    std::cerr << " capacity_ * 8 = " << (capacity_ * 8)
                //          << " vs. run_index + buffer_size * (1 + (type_info_ >> 5) = "
                //          << run_index_[0] << " + " << buffer_size << " * (1 + "
                //          << (type_info_ >> 5) << ") = "
                //          << run_index_[0] + int(buffer_size) * (1u + (type_info_ >> 5))
                //          << std::endl;
                //}
                bool ret =
                    (capacity_ * 8 <
                     run_index_[0] + buffer_size * (1u + (type_info_ >> 5)));
                //if (compressed && do_debug) {
                //    std::cerr << " returning " << ret << std::endl;
                //}
                return ret;
            }
        }
        //if (compressed && do_debug) {
        //    std::cerr << size_ << " >= " << capacity_ << " * " << WORD_BITS << " = " << (capacity_ * WORD_BITS) << std::endl;
        //}
        return size_ >= capacity_ * WORD_BITS;
    }

    /**
     * @brief Size of the leaf-associated data storage in 64-bit words.
     *
     * Primarily intended for debugging and validation.
     *
     * @return Size of internal data storage in 64-bit words.
     */
    uint16_t capacity() const { return capacity_; }

    /**
     * @brief Set the size of the leaf-associated data storage.
     *
     * Intended only to be set by an allocator after allocating additional
     * storage for the leaf. Setting the capacity carelessly easily leads to
     * undefined behaviour.
     *
     * @param cap New size for `data_`
     */
    void capacity(uint16_t cap) { capacity_ = cap; }

    /**
     * @brief Get the desired capacity for the leaf for potentially reducing
     * capacity
     *
     * Intended for use when removing elements from the leaf. If there is more
     * than 4 words of unused space a new smaller capacity will be returned,
     * else the current capacity will be returned.
     *
     * @returns A suitable capacity for the leaf.
     */
    uint16_t desired_capacity() {
        if constexpr (compressed) {
            if (is_compressed()) {
                uint64_t n_cap = run_index_[0] % 8;
                n_cap += run_index_[0] + (n_cap ? 8 - n_cap : 0);
                n_cap += buffer_size * (1 + (type_info_ >> 5));
                uint64_t m = n_cap % 8;
                n_cap += m ? 8 - m : 0;
                n_cap /= 8;
                n_cap += n_cap % 2 ? 1 : 0;
                return n_cap;
            }
        }
        uint16_t n_cap = 2 + size_ / WORD_BITS;
        n_cap += n_cap % 2;
        return n_cap;
    }

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
     * It is generally a very good idea to make sure the leaf buffer is
     * committed before accessing the raw leaf data.
     *
     * @return Pointer to raw leaf data.
     */
    const uint64_t* data() { return data_; }

    /**
     * @brief Remove the fist "elems" elements from the leaf.
     *
     * Intended for removing elements that have been copied to a neighbouring
     * leaf. Assumes that buffer of uncompressed leaves has been committed
     * before calling.
     *
     * Will update size and p_sum.
     *
     * @param elems number of elements to remove.
     */
    void clear_first(uint32_t elems) {
        if constexpr (compressed) {
            if (is_compressed()) {
                uint32_t q_elems = elems;
                uint8_t n_delete = 0;
                for (uint8_t b_idx = 0; b_idx < buffer_count_; b_idx++) {
                    uint32_t e_index = buffer_[b_idx] & C_INDEX;
                    if (e_index >= elems) {
                        buffer_[b_idx] -= elems;
                    } else {
                        size_--;
                        p_sum_ -= buffer_[b_idx] >> 31;
                        n_delete++;
                        q_elems--;
                    }
                }
                if (n_delete) {
                    for (uint8_t i = n_delete; i < buffer_count_; i++) {
                        buffer_[i - n_delete] = buffer_[i];
                        buffer_[i] = uint32_t(0);
                    }
                }
                buffer_count_ -= n_delete;
                uint32_t d_idx = 0;
                bool val = type_info_ & C_ONE_MASK;
                uint8_t* data = reinterpret_cast<uint8_t*>(data_);
                while (q_elems > 0) {
                    uint32_t c_bytes = 1;
                    uint32_t rl = 0;
                    if ((data[d_idx] & 0b11000000) == 0b11000000) {
                        rl = data[d_idx] & 0b00111111;
                    } else if ((data[d_idx] >> 7) == 0) {
                        rl = data[d_idx] << 24;
                        rl |= data[d_idx + 1] << 16;
                        rl |= data[d_idx + 2] << 8;
                        rl |= data[d_idx + 3];
                        c_bytes = 4;
                    } else if ((data[d_idx] & 0b10100000) == 0b10100000) {
                        rl = (data[d_idx] & 0b00011111) << 16;
                        rl |= data[d_idx + 1] << 8;
                        rl |= data[d_idx + 2];
                        c_bytes = 3;
                    } else {
                        rl = (data[d_idx] & 0b00011111) << 8;
                        rl |= data[d_idx + 1];
                        c_bytes = 2;
                    }
                    if (rl > q_elems) {
                        write_run(rl - q_elems, d_idx, c_bytes);
                        size_ -= q_elems;
                        p_sum_ -= val * q_elems;
                        q_elems = 0;
                    } else {
                        write_run(0, d_idx, c_bytes);
                        q_elems -= rl;
                        size_ -= rl;
                        p_sum_ -= val * rl;
                    }
                    d_idx += c_bytes;
                    val = !val;
                }
                c_commit<false>();
                return;
            }
        }
        // Important to ensure that all trailing data gets zeroed out to not
        // cause undefined behaviour for select or copy operations.
        uint32_t ones = rank(elems);
        uint32_t words = elems / WORD_BITS;

        if (elems % WORD_BITS == 0) {
            // If the data to remove happens to align with 64-bit words the
            // removal can be done by simply shuffling elements. This could be
            // memmove instead but testing indicates that it's not actually
            // faster in this case.
            for (uint32_t i = 0; i < capacity_ - words; i++) {
                data_[i] = data_[i + words];
            }
            for (uint32_t i = capacity_ - words; i < capacity_; i++) {
                data_[i] = 0;
            }
        } else {
            // clear elem bits at start of data
            for (uint32_t i = 0; i < words; i++) {
                data_[i] = 0;
            }
            uint64_t tail = elems % WORD_BITS;
            uint64_t tail_mask = (MASK << tail) - 1;
            data_[words] &= ~tail_mask;

            // Copy data backwars by elem bits
            uint32_t words_to_shuffle = capacity_ - words - 1;
            for (uint32_t i = 0; i < words_to_shuffle; i++) {
                data_[i] = data_[words + i] >> tail;
                data_[i] |= (data_[words + i + 1]) << (WORD_BITS - tail);
            }
            data_[capacity_ - words - 1] = data_[capacity_ - 1] >> tail;
            for (uint32_t i = capacity_ - words; i < capacity_; i++) {
                data_[i] = 0;
            }
            [[unlikely]] (void(0));
        }
        size_ -= elems;
        p_sum_ -= ones;
    }

    /**
     * @brief Transfer elems worth of elements from other to this.
     *
     * Only for use with compressed leaves when splitting. It is assumed that
     * this leaf is empty with an empty buffer.
     *
     * @param other Pointer to the sibling to copy from.
     */
    void transfer_capacity(leaf* other) {
#ifdef DEBUG
        assert(buffer_count_ == 0);
        assert(size_ == 0);
#endif
        type_info_ = C_TYPE_MASK;
        bool val = other->first_value();
        type_info_ |= val ? C_ONE_MASK : 0;
        uint32_t o_bytes = other->used_bytes();
        const uint8_t* o_data = reinterpret_cast<const uint8_t*>(other->data());
        uint32_t d_idx = 0;
        uint32_t* o_buf = other->buffer();
        uint8_t o_buf_count = other->buffer_count();
        uint8_t ob_idx = 0;
        while (d_idx < o_bytes) {
            uint32_t rl = 0;
            uint8_t r_bytes = 1;
            if ((o_data[d_idx] & 0b11000000) == 0b11000000) {
                rl = o_data[d_idx] & 0b00111111;
            } else if ((o_data[d_idx] >> 7) == 0u) {
                rl = o_data[d_idx] << 24;
                rl |= o_data[d_idx + 1] << 16;
                rl |= o_data[d_idx + 2] << 8;
                rl |= o_data[d_idx + 3];
                r_bytes = 4;
            } else if ((o_data[d_idx] & 0b11100000) == 0b10000000) {
                rl = (o_data[d_idx] & 0b00011111) << 8;
                rl |= o_data[d_idx + 1];
                r_bytes = 2;
            } else {
                rl = (o_data[d_idx] & 0b00011111) << 16;
                rl |= o_data[d_idx + 1] << 8;
                rl |= o_data[d_idx + 2];
                r_bytes = 3;
            }
            if ((other->size() - size_ - rl >= leaf_size / 3) &&
                ((d_idx + r_bytes < o_bytes / 2 ||
                  buffer_count_ < (buffer_size >> 1)))) {
                d_idx += r_bytes;
                assert(rl != 0);
                write_run(rl);
                size_ += rl;
                p_sum_ += val * rl;
                for (; ob_idx < o_buf_count; ob_idx++) {
                    if ((o_buf[ob_idx] & C_INDEX) < size_) {
                        buffer_[buffer_count_++] = o_buf[ob_idx];
                        size_++;
                        p_sum_ += o_buf[ob_idx] >> 31;
                    } else {
                        [[likely]] break;
                    }
                }
            } else if (other->size() - size_ > leaf_size / 3 &&
                       d_idx < o_bytes / 2) {
                uint32_t to_copy = rl - rl / 2;
                if (size_ + to_copy < (leaf_size / 3)) {
                    to_copy = (leaf_size / 3) - size_;
                    [[unlikely]] to_copy = to_copy > rl ? rl : to_copy;
                }
                if (other->size() - size_ - to_copy < (leaf_size / 3)) {
                    to_copy = other->size() - size_ - (leaf_size / 3);
                    [[unlikely]] to_copy = to_copy > rl ? rl : to_copy;
                }
                assert(rl >= to_copy);
                write_run(to_copy);
                size_ += to_copy;
                p_sum_ += val * to_copy;
                for (; ob_idx < o_buf_count; ob_idx++) {
                    if ((o_buf[ob_idx] & C_INDEX) < size_) {
                        buffer_[buffer_count_++] = o_buf[ob_idx];
                        size_++;
                        p_sum_ += o_buf[ob_idx] >> 31;
                    } else {
                        [[likely]] break;
                    }
                }
                break;
            } else {
                break;
            }
            val = !val;
        }
        if (run_index_[0] * 8 > size_) {
            [[unlikely]] flatten();
        }
        other->clear_first(size_);
    }

    /**
     * @brief Move "elems" elements from the start of "other" to the end of
     * "this".
     *
     * **Will not** ensure sufficient capacity for the copy target.
     *
     * The operation will ensure that the buffers of both leaves are cleared
     * before transferring elements. Elements are first copied, after which,
     * elements are cleared from the sibling with `other->clear_first(elems)`.
     *
     * @param other Pointer to the next sibling.
     * @param elems Number of elements to transfer.
     */
    void transfer_append(leaf* other, uint32_t elems) {
        //if (compressed && do_debug) {
        //    std::cout << "Transfer append" << std::endl;
        //    print(false);
        //    std::cout << std::endl;
        //}
        if constexpr (compressed) {
            if (is_compressed()) {
                flatten();
            } else {
                commit<false>();
            }
        } else {
            commit();
        }
        if constexpr (compressed) {
            if (other->is_compressed()) {
                c_append(other, elems);
                other->clear_first(elems);
                return;
            }
            other->commit<false>();
        } else {
            other->commit();
        }
        //if (compressed && do_debug) {
        //    std::cout << "Other is not compressed and I am flat" << std::endl;
        //    print(false);
        //    std::cout << std::endl;
        //}
        const uint64_t* o_data = other->data();
        uint32_t split_point = size_ % WORD_BITS;
        uint32_t target_word = size_ / WORD_BITS;
        uint32_t copy_words = elems / WORD_BITS;
        uint32_t overhang = elems % WORD_BITS;
        // Branching depending on word alignment of source and target.
        if (split_point == 0) {
            // Copy target is word aligned
            for (uint32_t i = 0; i < copy_words; i++) {
                data_[target_word++] = o_data[i];
                p_sum_ += __builtin_popcountll(o_data[i]);
            }
            if (overhang != 0) {
                // Copy source is not word aligned
                data_[target_word] =
                    o_data[copy_words] & ((MASK << overhang) - 1);
                [[unlikely]] p_sum_ += __builtin_popcountll(data_[target_word]);
            }
        } else {
            // Copy target is not word aligned
            for (uint32_t i = 0; i < copy_words; i++) {
                data_[target_word++] |= o_data[i] << split_point;
                data_[target_word] |= o_data[i] >> (WORD_BITS - split_point);
                p_sum_ += __builtin_popcountll(o_data[i]);
            }
            if (elems % WORD_BITS != 0) {
                // Copy source is not word aligned.
                uint64_t to_write =
                    o_data[copy_words] & ((MASK << overhang) - 1);
                p_sum_ += __builtin_popcountll(to_write);
                data_[target_word++] |= to_write << split_point;
                if (target_word < capacity_) {
                    data_[target_word] |= to_write >> (WORD_BITS - split_point);
                }
                [[likely]] ((void)0);
            }
        }
        size_ += elems;
        other->clear_first(elems);
    }

    /**
     * @brief Remove the last "elems" elements from the leaf.
     *
     * Intended for removing elements that have been copied to a neighbouring
     * leaf. Assumes that buffer has been committed before calling.
     *
     * Will update size and p_sum.
     *
     * @param elems number of elements to remove.
     */
    void clear_last(uint32_t elems) {
        if constexpr (compressed) {
            if (is_compressed()) {
                // std::cerr << elems
                //           << " elements are getting deleted:" << std::endl;
                // print(false);
                // std::cout << std::endl;
                uint32_t end = size_ - elems;
                p_sum_ = 0;
                for (uint8_t b_idx = buffer_count_ - 1; b_idx < buffer_count_;
                     b_idx--) {
                    if ((buffer_[b_idx] & C_INDEX) >= end) {
                        // std::cerr << " deleted buffer at "
                        //           << (buffer_[b_idx] & C_INDEX) << std::endl;
                        buffer_[b_idx] = uint32_t(0);
                        buffer_count_--;
                    } else {
                        p_sum_ += buffer_[b_idx] >> 31;
                        // std::cerr << "p_sum_ to " << p_sum_ << std::endl;
                    }
                }
                end -= buffer_count_;
                uint32_t d_idx = 0;
                uint32_t loc = 0;
                uint8_t* data = reinterpret_cast<uint8_t*>(data_);
                bool val = type_info_ & C_ONE_MASK;
                while (d_idx < run_index_[0]) {
                    uint32_t rl = 0;
                    uint32_t r_bytes = 1;
                    if ((data[d_idx] & 0b11000000) == 0b11000000) {
                        rl = data[d_idx] & 0b00111111;
                    } else if ((data[d_idx] >> 7) == 0u) {
                        rl = data[d_idx] << 24;
                        rl |= data[d_idx + 1] << 16;
                        rl |= data[d_idx + 2] << 8;
                        rl |= data[d_idx + 3];
                        r_bytes = 4;
                    } else if ((data[d_idx] & 0b11100000) == 0b10000000) {
                        rl = (data[d_idx] & 0b00011111) << 8;
                        rl |= data[d_idx + 1];
                        r_bytes = 2;
                    } else {
                        rl = (data[d_idx] & 0b00011111) << 16;
                        rl |= data[d_idx + 1] << 8;
                        rl |= data[d_idx + 2];
                        r_bytes = 3;
                    }
                    // std::cerr << "Read run of length " << rl << std::endl;
                    if (rl + loc == end) {
                        p_sum_ += val * rl;
                        val = !val;
                        loc += rl;
                        d_idx += r_bytes;
                        break;
                    } else if (rl + loc > end) {
                        // std::cerr << "Splitting run of length " << rl
                        //           << std::endl;
                        uint32_t n_rl = end - loc;
                        write_run(n_rl, d_idx, r_bytes);
                        type_info_ |= C_RUN_REMOVAL_MASK;
                        p_sum_ += val * n_rl;
                        loc += n_rl;
                        val = !val;
                        d_idx += r_bytes;
                        break;
                    }
                    d_idx += r_bytes;
                    p_sum_ += val * rl;
                    val = !val;
                    loc += rl;
                }
                memset(data + d_idx, 0, run_index_[0] - d_idx);
                run_index_[0] = d_idx;
                // std::cout << size_ << " - " << elems << " = " << (size_ -
                // elems)
                //           << std::endl;
                size_ = size_ - elems;
                // std::cout << "size_ = " << size_ << std::endl;
                // std::cerr << elems << " elements got deleted:" << std::endl;
                // print(false);
                // std::cout << std::endl;
                // validate();
                return;
            }
        }
        size_ -= elems;
        p_sum_ = rank(size_);
        uint32_t offset = size_ % WORD_BITS;
        uint32_t words = size_ / WORD_BITS;
        // Important to ensure that all trailing data gets zeroed out to not
        // cause undefined behaviour for select or copy operations.
        if (offset != 0) {
            [[likely]] data_[words++] &= (MASK << offset) - 1;
        }
        for (uint32_t i = words; i < capacity_; i++) {
            data_[i] = 0;
        }
    }

    /**
     * @brief Move "elems" elements from the end of "other" to the start of
     * "this".
     *
     * **Will not** ensure sufficient capacity for the copy target.
     *
     * The operation will ensure that the buffers of both leaves are cleared
     * before transferring elements. Elements are first copied, after which,
     * elements are cleared from the sibling with `other->clear_last(elems)`.
     *
     * @param other Pointer to the previous sibling.
     * @param elems Number of elements to transfer.
     */
    void transfer_prepend(leaf* other, uint32_t elems) {
        if constexpr (compressed) {
            if (is_compressed()) {
                flatten();
            } else {
                commit<false>();
            }
        } else {
            commit();
        }
        uint32_t words = elems / WORD_BITS;
        // Make space for new data
        for (uint32_t i = capacity_ - 1; i >= words; i--) {
            data_[i] = data_[i - words];
            if (i == 0) break;
        }
        for (uint32_t i = 0; i < words; i++) {
            data_[i] = 0;
        }
        uint32_t overflow = elems % WORD_BITS;
        if (overflow > 0) {
            for (uint32_t i = capacity_ - 1; i > words; i--) {
                data_[i] <<= overflow;
                data_[i] |= data_[i - 1] >> (WORD_BITS - overflow);
            }
            data_[words] <<= overflow;
            [[likely]] (void(0));
        }
        if constexpr (compressed) {
            if (other->is_compressed()) {
                c_transfer_prepend(other, elems);
                return;
            }
            other->commit<false>();
        } else {
            other->commit();
        }
        const uint64_t* o_data = other->data();
        // Copy over data from sibling
        uint32_t source_word = other->size();
        uint32_t source_offset = source_word % WORD_BITS;
        source_word /= WORD_BITS;
        // Somewhat complicated case handling based on target and source word
        // alignment. Additional complications compared to transfer append is
        // due to the both the start point and end point for the copy source not
        // being word aligned.
        if (source_offset == 0) {
            // Source is word aligned.
            if (overflow == 0) {
                // Target is word aligned.
                for (uint32_t i = words - 1; i < words; i--) {
                    data_[i] = o_data[--source_word];
                    p_sum_ += __builtin_popcountll(data_[i]);
                }
            } else {
                // Target is not word aligned.
                source_word--;
                for (uint32_t i = words; i > 0; i--) {
                    p_sum_ += __builtin_popcountll(o_data[source_word]);
                    data_[i] |= o_data[source_word] >> (WORD_BITS - overflow);
                    data_[i - 1] |= o_data[source_word--] << overflow;
                }
                uint64_t w = o_data[source_word] >> (WORD_BITS - overflow);
                p_sum_ += __builtin_popcountll(w);
                [[likely]] data_[0] |= w;
            }
            [[unlikely]] (void(0));
        } else {
            // Source is not word aligned
            if (overflow == 0) {
                // Target is word aligned
                for (uint32_t i = words - 1; i < words; i--) {
                    data_[i] = o_data[source_word]
                               << (WORD_BITS - source_offset);
                    data_[i] |= o_data[--source_word] >> source_offset;
                    p_sum_ += __builtin_popcountll(data_[i]);
                }
            } else {
                // Target is not word aligned
                uint64_t w;
                for (uint32_t i = words; i > 0; i--) {
                    w = o_data[source_word] << (WORD_BITS - source_offset);
                    w |= o_data[--source_word] >> source_offset;
                    p_sum_ += __builtin_popcountll(w);
                    data_[i] |= w >> (WORD_BITS - overflow);
                    data_[i - 1] |= w << overflow;
                }
                w = o_data[source_word] << (WORD_BITS - source_offset);
                w |= o_data[--source_word] >> source_offset;
                w >>= WORD_BITS - overflow;
                p_sum_ += __builtin_popcountll(w);
                [[likely]] data_[0] |= w;
            }
        }
        size_ += elems;
        other->clear_last(elems);
    }

    /**
     * @brief Copy all elements from (the start of) "other" to the end of
     * "this".
     *
     * Intended for merging leaves.
     *
     * **Will not** ensure sufficient capacity for merge.
     *
     * Will ensure that the buffers are empty for both leaves.
     *
     * Will not clear elements from "other" as it is assumed that "other" will
     * be deallocated .
     *
     * @param other Pointer to next leaf.
     */
    void append_all(leaf* other) {
        if constexpr (compressed) {
            if (is_compressed()) {
                flatten();
            } else {
                commit();
            }
        } else {
            commit();
        }
        if constexpr (compressed) {
            if (other->is_compressed()) {
                c_append(other, other->size());
                return;
            }
        }
        other->commit();
        const uint64_t* o_data = other->data();
        uint32_t offset = size_ % WORD_BITS;
        uint32_t word = size_ / WORD_BITS;
        uint32_t o_size = other->size();
        uint32_t o_p_sum = other->p_sum();
        uint32_t o_words = o_size / WORD_BITS;
        o_words += o_size % WORD_BITS == 0 ? 0 : 1;
        // Elements are guaranteed to be fully word aligned in practice so
        // branching is only required for word alignment of the target leaf.
        if (offset == 0) {
            // Target is word aligned.
            for (uint32_t i = 0; i < o_words; i++) {
                data_[word++] = o_data[i];
            }
            [[unlikely]] (void(0));
        } else {
            // Target is not word aligned.
            for (uint32_t i = 0; i < o_words; i++) {
                data_[word++] |= o_data[i] << offset;
                data_[word] |= o_data[i] >> (WORD_BITS - offset);
            }
        }
        size_ += o_size;
        p_sum_ += o_p_sum;
    }

    void flush() {
        if constexpr (compressed) {
            if (is_compressed()) {
                return;
            }
        }
        commit();
    }

    uint64_t dump(uint64_t* target, uint64_t start) {
        if constexpr (compressed) {
            if (is_compressed()) {
                return c_dump(target, start);
            }
        }
        commit();
        uint64_t word = start / WORD_BITS;
        uint16_t offset = start % WORD_BITS;
        uint64_t t_words = size_ / WORD_BITS;
        uint16_t t_offset = size_ % WORD_BITS;
        if (offset == 0) {
            for (uint16_t i = 0; i < t_words; i++) {
                target[word++] = data_[i];
            }
            if (t_offset > 0) {
                target[word] = data_[t_words];
            }
            [[unlikely]] (void(0));
        } else {
            for (uint16_t i = 0; i < t_words; i++) {
                target[word++] |= data_[i] << offset;
                target[word] |= data_[i] >> (WORD_BITS - offset);
            }
            if (t_offset > 0) {
                target[word++] |= data_[t_words] << offset;
                if (t_offset > WORD_BITS - offset) {
                    target[word] |= data_[t_words] >> (WORD_BITS - offset);
                }
            }
        }
        return start + size_;
    }

    bool is_compressed() const {
        if constexpr (compressed) {
            return (type_info_ & C_TYPE_MASK) == C_TYPE_MASK;
        }
        return false;
    }

    /**
     * @brief Ensure that the leaf is in a valid state.
     *
     * Will use assertions to check that the actual stored data agrees with
     * stored metadata. Will not in any way that the leaf is in the correct
     * state given the preceding sequence of operations, just that the leaf is
     * in a state that is valid for a leaf.
     *
     * If the compiler flag `-DNDEBUG` is given, this function will do nothing
     * and may be completely optimized out by an optimizing compiler.
     *
     * @return 1 for calculating the number of nodes in the tree.
     */
    uint32_t validate() const {
        if (is_compressed()) {
            assert(size_ <= (~uint32_t(0) >> 1));
        } else {
            assert(size_ <= capacity_ * WORD_BITS);
        }
        assert(p_sum_ <= size_);
        assert(p_sum_ == rank(size_));
        if (is_compressed()) {
            bool rem = type_info_ & C_RUN_REMOVAL_MASK;
            uint8_t* data = reinterpret_cast<uint8_t*>(data_);
            for (size_t i = run_index_[0]; i < 8 * capacity_; i++) {
                assert(data[i] == 0);
            }
            uint32_t d_idx = 0;
            uint32_t c_size = buffer_count_;
            uint32_t c_sum = 0;
            for (size_t i = 0; i < buffer_count_; i++) {
                c_sum += buffer_[i] >> 31;
            }
            bool val = type_info_ & C_ONE_MASK;
            while (d_idx < run_index_[0]) {
                uint32_t rl = 0;
                if ((data[d_idx] & 0b11000000) == 0b11000000) {
                    rl = data[d_idx++] & 0b00111111;
                } else if ((data[d_idx] >> 7) == 0) {
                    rl = data[d_idx++] << 24;
                    rl |= data[d_idx++] << 16;
                    rl |= data[d_idx++] << 8;
                    rl |= data[d_idx++];
                } else if ((data[d_idx] & 0b10100000) == 0b10100000) {
                    rl = (data[d_idx++] & 0b00011111) << 16;
                    rl |= data[d_idx++] << 8;
                    rl |= data[d_idx++];
                } else {
                    rl = (data[d_idx++] & 0b00011111) << 8;
                    rl |= data[d_idx++];
                }
                if (!rem) {
                    assert(rl > 0);
                }
                c_size += rl;
                c_sum += val * rl;
                val = !val;
            }
            assert(c_size == size_);
            return 1;
        }
        uint32_t u_loc = size_;
        for (uint8_t i = 0; i < buffer_count_; i++) {
            if (buffer_is_insertion(buffer_[i])) {
                u_loc--;
            } else {
                u_loc++;
            }
        }
        uint32_t last_word = u_loc / WORD_BITS;
        uint32_t overflow = u_loc % WORD_BITS;
        if (overflow != 0) {
            uint64_t val = (MASK << overflow) - 1;
            val = (~val) & data_[last_word];
            assert(val == 0u);
            last_word++;
        }
        for (uint32_t i = last_word; i < capacity_; i++) {
            assert(data_[i] == 0);
        }
        return 1;
    }

    /**
     * @brief Output data stored in the leaf as json.
     *
     * Will output valid json, but will not output a trailing newline since the
     * call is expected to be part of a recursive tree traversal.
     *
     * @param internal_only Will not output raw data if true to save space.
     */
    void print(bool internal_only = true) const {
        if (is_compressed()) {
            c_print(internal_only);
            return;
        }
        std::cout << "{\n\"type\": \"leaf\",\n"
                  << "\"size\": " << size_ << ",\n"
                  << "\"capacity\": " << capacity_ << ",\n"
                  << "\"p_sum\": " << p_sum_ << ",\n"
                  << "\"buffer_size\": " << int(buffer_size) << ",\n"
                  << "\"buffer_count\": " << int(buffer_count_) << ",\n"
                  << "\"buffer\": [\n";
        for (uint8_t i = 0; i < buffer_count_; i++) {
#pragma GCC diagnostic ignored "-Warray-bounds"
            std::cout << "{\"is_insertion\": "
                      << buffer_is_insertion(buffer_[i]) << ", "
                      << "\"buffer_value\": " << buffer_value(buffer_[i])
                      << ", "
                      << "\"buffer_index\": " << buffer_index(buffer_[i])
                      << "}";
#pragma GCC diagnostic pop
            if (i != buffer_count_ - 1) {
                std::cout << ",\n";
            }
        }
        if (!internal_only) {
            std::cout << "],\n\"data\": [\n";
            for (uint64_t i = 0; i < capacity_; i++) {
                std::bitset<WORD_BITS> b(data_[i]);
                std::cout << "\"";
                for (size_t i = 0; i < 64; i++) {
                    if (i % 8 == 0 && i > 0) {
                        std::cout << " ";
                    }
                    std::cout << (b[i] ? "1" : "0");
                }
                std::cout << "\"";
                if (i != uint64_t(capacity_ - 1)) {
                    std::cout << ",\n";
                }
            }
            std::cout << "]}";
        } else {
            std::cout << "]}";
        }
    }

    std::pair<uint64_t, uint64_t> leaf_usage() const {
        // TODO: What is even leaf usage for rle leaves?
        return std::pair<uint64_t, uint64_t>(capacity_ * WORD_BITS, size_);
    }

   private:
    /**
     * @brief Extract the value of a buffer element
     *
     * The First bit (lsb) of a 23-bit buffer element contains the value of the
     * element.
     *
     * I.e. if `bv[i]` is `1`, then the buffer created for `remove(i)` will have
     * a lsb with value 1.
     *
     * @param e Buffer element to extract value from.
     *
     * @return Boolean value indicating the value of the element referred to by
     * the buffer.
     */
    inline bool buffer_value(uint32_t e) const { return (e & VALUE_MASK) != 0; }

    /**
     * @brief Extract type information of a buffer element.
     *
     * The fourth least significant bit (`0b1000`), contains a 1 if the buffered
     * operation is an insertion.
     *
     * @param e Buffer element to extract type from.
     *
     * @return Boolean value True if the buffer is related to an insert
     * operations, and false if the buffer is related to a removal.
     */
    inline bool buffer_is_insertion(uint32_t e) const {
        return (e & TYPE_MASK) != 0;
    }

    /**
     * @brief Extract index information from a butter element.
     *
     * The 24 most significant bits of the 32-bit buffer element contain index
     * information on the insert/removal operation.
     *
     * @param e Buffer element to extract index from.
     *
     * @return Index information related to the buffer element.
     */
    inline uint32_t buffer_index(uint32_t e) const { return e >> 8; }

    /**
     * @brief Updates index information for a specified buffer element.
     *
     * Clears index information for the i<sup>th</sup> buffer element and
     * replases it with v.
     *
     * @param v Value to set the buffer index to.
     * @param i Index of buffer element in buffer.
     */
    void set_buffer_index(uint32_t v, uint8_t i) {
        buffer_[i] = (v << 8) | (buffer_[i] & INDEX_MASK);
    }

    /**
     * @brief Creates a new 32-bit buffer element with the given parameters.
     *
     * A new buffer element is typically created for insertion into the buffer.
     *
     * @param idx Index value for the new buffer element.
     * @param t   Type (Insert/Remove) of new buffer element.
     * @param v   Value associated with the new buffer element.
     */
    uint32_t create_buffer(uint32_t idx, bool t, bool v) {
        return ((idx << 8) | (t ? TYPE_MASK : uint32_t(0))) |
               (v ? VALUE_MASK : uint32_t(0));
    }

    /**
     * @brief Insert a new element into the buffer.
     *
     * Existing elements with index idx or greater gets shuffled forward and the
     * new element will overwrite the buffer at index idx.
     *
     * @param idx Position of the new element in the buffer.
     * @param buf Element to insert.
     */
    void insert_buffer(uint8_t idx, uint32_t buf) {
        memmove(buffer_ + idx + 1, buffer_ + idx,
                (buffer_count_ - idx) * sizeof(uint32_t));
        buffer_[idx] = buf;
        buffer_count_++;
    }

    /**
     * @brief Remove an element from the buffer.
     *
     * Existing elements with index greater than idx get shuffled backwards and
     * the old element at idx will be overwritten.
     *
     * @param idx Location of the element in the buffer
     */
    void delete_buffer_element(uint8_t idx) {
        buffer_count_--;
        memmove(buffer_ + idx, buffer_ + idx + 1,
                (buffer_count_ - idx) * sizeof(uint32_t));
        buffer_[buffer_count_] = 0;
    }

    /**
     * @brief Add an element to the end of the leaf data.
     *
     * If naively writing to the next available position would cause an
     * overflow, the buffer will be committed and this will guarantee that the
     * next available position will become valid assuming proper handling of the
     * leaf by the parent element.
     *
     * @param x Value to be appended to the data.
     */
    void push_back(const bool x) {
        if constexpr (compressed) {
            assert(!is_compressed());
        }
        assert(size_ < capacity_ * WORD_BITS);
        uint32_t pb_size = size_;
        if constexpr (buffer_size != 0) {
            for (uint8_t i = 0; i < buffer_count_; i++) {
                pb_size += buffer_is_insertion(buffer_[i]) ? -1 : 1;
            }
        }

        // If the leaf has at some point been full and has subsequently shrunk
        // due to removals, the next available position to write to without
        // committing the buffer may be beyond the end of the data_ array. In
        // this case the buffer will need to be committed before appending the
        // new element.
        if (pb_size >= capacity_ * WORD_BITS) {
            commit();
            [[unlikely]] data_[size_ / WORD_BITS] |= uint64_t(x)
                                                     << (size_ % WORD_BITS);
        } else {
            data_[pb_size / WORD_BITS] |= uint64_t(x) << (pb_size % WORD_BITS);
        }

        size_++;
        p_sum_ += uint64_t(x);
    }

    uint32_t unb_select(uint32_t x) const {
        uint32_t pop = 0;
        uint32_t pos = 0;
        uint32_t prev_pop = 0;
        uint32_t j = 0;

        // Step one 64-bit word at a time considering the buffer until pop >= x
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
        add_loc = uint64_t(1) << add_loc;
        return pos + 63 - __builtin_clzll(_pdep_u64(add_loc, data_[j]));
    }

    uint32_t unb_select0(uint32_t x) const{

        uint32_t pop = 0;
        uint32_t pos = 0;
        uint32_t prev_pop = 0;
        uint32_t j = 0;

        for(; j < capacity_; j++) {
            prev_pop = pop;
            pop += __builtin_popcountll(~data_[j]);
            pos += WORD_BITS;
            if (pop >= x) {
                [[unlikely]] break;
            }
        }

        pos -= WORD_BITS;
        uint64_t add_loc = x - prev_pop - 1;
        add_loc = uint64_t(1) << add_loc;
        return pos + 63 - __builtin_clzll(_pdep_u64(add_loc, ~data_[j]));
    }

    /**
     * @brief Commit and clear the Insert/Remove buffer for the leaf.
     *
     * Intended for clearing a full buffer before insertion or removal, and for
     * ensuring an empty buffer before transfer operations.
     *
     * Slightly complicated but linear time function for committing all buffered
     * operations to the underlying data.
     */
    template<bool allow_convert = true>
    void commit() {
        if constexpr (compressed) {
            if (is_compressed()) {
                c_commit();
                return;
            }
        }
        // std::cout << "Commit is here" << std::endl;
        //  Complicated bit manipulation but whacha gonna do. Hopefully won't
        //  need to debug this anymore.
        if constexpr (buffer_size == 0) return;
        if (buffer_count_ == 0) [[unlikely]]
            return;

        uint32_t overflow = 0;
        uint8_t overflow_length = 0;
        uint8_t underflow_length = 0;
        uint8_t current_index = 0;
        uint32_t buf = buffer_[current_index];
        uint32_t target_word = buffer_index(buf) / WORD_BITS;
        uint32_t target_offset = buffer_index(buf) % WORD_BITS;

        uint32_t words = size_ / WORD_BITS;
        words += size_ % WORD_BITS > 0 ? 1 : 0;
        for (uint32_t current_word = 0; current_word < words; current_word++) {
            uint64_t underflow =
                current_word + 1 < capacity_ ? data_[current_word + 1] : 0;
            if (overflow_length) {
                [[likely]] underflow =
                    (underflow << overflow_length) |
                    (data_[current_word] >> (WORD_BITS - overflow_length));
            }

            uint64_t new_overflow = 0;
            // If buffers need to be commit to this word:
            if (current_word == target_word && current_index < buffer_count_) {
                uint64_t word =
                    underflow_length
                        ? (data_[current_word] >> underflow_length) |
                              (underflow << (WORD_BITS - underflow_length))
                        : (data_[current_word] << overflow_length) | overflow;
                underflow >>= underflow_length;
                uint64_t new_word = 0;
                uint8_t start_offset = 0;
                // While there are buffers for this word
                while (current_word == target_word) {
                    new_word |=
                        (word << start_offset) & ((MASK << target_offset) - 1);
                    word = (word >> (target_offset - start_offset)) |
                           (target_offset == 0 ? 0
                            : target_offset - start_offset == 0
                                ? 0
                                : (underflow << (WORD_BITS - (target_offset -
                                                              start_offset))));
                    underflow >>= target_offset - start_offset;
                    if (buffer_is_insertion(buf)) {
                        if (buffer_value(buf)) {
                            new_word |= MASK << target_offset;
                        }
                        start_offset = target_offset + 1;
                        if (underflow_length) [[unlikely]]
                            underflow_length--;
                        else
                            overflow_length++;
                    } else {
                        word >>= 1;
                        word |= underflow << 63;
                        underflow >>= 1;
                        if (overflow_length)
                            overflow_length--;
                        else [[likely]]
                            underflow_length++;
                        start_offset = target_offset;
                    }
                    current_index++;
                    if (current_index >= buffer_count_) [[unlikely]]
                        break;
                    buf = buffer_[current_index];
                    target_word = buffer_index(buf) / WORD_BITS;
                    [[unlikely]] target_offset = buffer_index(buf) % WORD_BITS;
                }
                new_word |= start_offset < WORD_BITS ? (word << start_offset)
                                                     : uint64_t(0);
                new_overflow =
                    overflow_length
                        ? data_[current_word] >> (WORD_BITS - overflow_length)
                        : 0;
                [[unlikely]] data_[current_word] = new_word;
            } else {
                if (underflow_length) {
                    data_[current_word] =
                        (data_[current_word] >> underflow_length) |
                        (underflow << (WORD_BITS - underflow_length));
                } else if (overflow_length) {
                    new_overflow =
                        data_[current_word] >> (WORD_BITS - overflow_length);
                    [[likely]] data_[current_word] =
                        (data_[current_word] << overflow_length) | overflow;
                } else {
                    overflow = 0;
                }
            }
            overflow = new_overflow;
        }
        if (capacity_ > words) {
            [[likely]] data_[words] = 0;
        }
        buffer_count_ = 0;
        if constexpr (compressed && allow_convert) {
            c_rle_check_convert();
        }
    }

    bool c_at(uint32_t i) const {
        uint32_t i_q = i;
        for (uint8_t b_idx = 0; b_idx < buffer_count_; b_idx++) {
            uint32_t e_index = buffer_[b_idx] & C_INDEX;
            if (e_index < i) [[likely]]
                i_q--;
            else if (e_index == i)
                return buffer_[b_idx] >> 31;
            else
                break;
        }
        // std::cout << "Looking for " << i_q << std::endl;
        bool ret = type_info_ & C_ONE_MASK;
        uint32_t c_i = 0;
        uint8_t* data = reinterpret_cast<uint8_t*>(data_);
#pragma GCC diagnostic ignored "-Warray-bounds"
        for (uint32_t d_idx = 0; d_idx < run_index_[0]; d_idx++) {
#pragma GCC diagnostic pop
            uint32_t rl = 0;
            if ((data[d_idx] & 0b11000000) == 0b11000000) {
                rl = data[d_idx] & 0b00111111;
                // std::cout << "\t1 byte" << std::endl;
            } else if ((data[d_idx] & 0b10000000) == 0) {
                rl = data[d_idx++] << 24;
                rl |= data[d_idx++] << 16;
                rl |= data[d_idx++] << 8;
                rl |= data[d_idx];
                // std::cout << "\t4 bytes" << std::endl;
            } else if ((data[d_idx] & 0b10100000) == 0b10100000) {
                rl = (data[d_idx++] & 0b00011111) << 16;
                rl |= data[d_idx++] << 8;
                rl |= data[d_idx];
                // std::cout << "\t3 bytes" << std::endl;
            } else {
                rl = (data[d_idx++] & 0b00011111) << 8;
                rl |= data[d_idx];
                // std::cout << "\t2 bytes" << std::endl;
            }
            // std::cout << "rl = " << rl << std::endl;
            c_i += rl;
            // std::cout << "c_i + rl = " << c_i << std::endl;
            if (c_i > i_q) break;
            ret = !ret;
        }
        return ret;
    }

    void c_insert(uint32_t i, bool v) {
        //if (compressed && do_debug) {
        //    std::cout << "c_insert(" << i << ", " << v << ")" << std::endl;
        //}
        uint8_t i_idx = 0;
        for (uint8_t b_idx = 0; b_idx < buffer_count_; b_idx++) {
            uint32_t e_index = buffer_[b_idx] & C_INDEX;
            if (e_index < i)
                i_idx++;
            else
                buffer_[b_idx]++;
        }
        uint32_t n_b = i | (v ? ~C_INDEX : uint32_t(0));
        if (i_idx == buffer_count_) {
            buffer_[buffer_count_++] = n_b;
        } else {
            insert_buffer(i_idx, n_b);
        }
        size_++;
        p_sum_ += v;
        if (buffer_count_ >= buffer_size) {
            c_commit();
        }
    }

    bool c_remove(uint32_t i) {
        uint32_t q_i = i;
        bool done = false;
        bool ret = false;
        for (uint8_t b_idx = 0; b_idx < buffer_count_; b_idx++) {
            uint32_t e_index = buffer_[b_idx] & C_INDEX;
            if (e_index < i) {
                q_i--;
            } else if (e_index == i) {
                ret = buffer_[b_idx] >> 31;
                delete_buffer_element(b_idx--);
                done = true;
                size_--;
                p_sum_ -= ret;
            } else {
                buffer_[b_idx]--;
            }
        }
        if (done) {
            [[unlikely]] return ret;
        }
        type_info_ |= C_RUN_REMOVAL_MASK;
        uint32_t c_i = 0;
        ret = type_info_ & C_ONE_MASK;
        uint8_t* data = reinterpret_cast<uint8_t*>(data_);
        uint32_t d_idx = 0;
        while (d_idx < run_index_[0]) {
            uint32_t rl = 0;
            uint8_t r_b = 1;
            if ((data[d_idx] & 0b11000000) == 0b11000000) {
                rl = data[d_idx] & 0b00111111;
            } else if ((data[d_idx] >> 7) == 0) {
                rl = data[d_idx] << 24;
                rl |= data[d_idx + 1] << 16;
                rl |= data[d_idx + 2] << 8;
                rl |= data[d_idx + 3];
                r_b = 4;
            } else if ((data[d_idx] & 0b10100000) == 0b10100000) {
                rl = (data[d_idx] & 0b00011111) << 16;
                rl |= data[d_idx + 1] << 8;
                rl |= data[d_idx + 2];
                r_b = 3;
            } else {
                rl = (data[d_idx] & 0b00011111) << 8;
                rl |= data[d_idx + 1];
                r_b = 2;
            }
            c_i += rl;
            if (c_i > q_i) {
                write_run(rl - 1, d_idx, r_b);
                size_--;
                p_sum_ -= ret;
                break;
            }
            d_idx += r_b;
            ret = !ret;
        }
        return ret;
    }

    int c_set(uint32_t i, bool x) {
        uint32_t q_i = i;
        uint8_t b_idx = 0;
        for (; b_idx < buffer_count_; b_idx++) {
            uint32_t e_index = buffer_[b_idx] & C_INDEX;
            if (e_index < i) {
                [[likely]] q_i--;
            } else if (e_index == i) {
                bool val = buffer_[b_idx] >> 31;
                if (val == x) {
                    return 0;
                }
                int ret = val ? -1 : 1;
                p_sum_ += ret;
                buffer_[b_idx] =
                    (buffer_[b_idx] & C_INDEX) | (x ? ~C_INDEX : 0);
                return ret;
            } else {
                break;
            }
        }
        uint32_t c_i = 0;
        bool val = type_info_ & C_ONE_MASK;
        int ret = 0;
        uint8_t* data = reinterpret_cast<uint8_t*>(data_);
        uint32_t d_idx = 0;
        while (d_idx < run_index_[0]) {
            uint32_t rl = 0;
            uint8_t r_b = 1;
            if ((data[d_idx] & 0b11000000) == 0b11000000) {
                rl = data[d_idx] & 0b00111111;
            } else if ((data[d_idx] >> 7) == 0) {
                rl = data[d_idx] << 24;
                rl |= data[d_idx + 1] << 16;
                rl |= data[d_idx + 2] << 8;
                rl |= data[d_idx + 3];
                r_b = 4;
            } else if ((data[d_idx] & 0b10100000) == 0b10100000) {
                rl = (data[d_idx] & 0b00011111) << 16;
                rl |= data[d_idx + 1] << 8;
                rl |= data[d_idx + 2];
                r_b = 3;
            } else {
                rl = (data[d_idx] & 0b00011111) << 8;
                rl |= data[d_idx + 1];
                r_b = 2;
            }
            c_i += rl;
            // std::cout << c_i << " vs " << q_i << std::endl;
            if (c_i > q_i) {
                if (val == x) return ret;
                type_info_ |= C_RUN_REMOVAL_MASK;
                write_run(rl - 1, d_idx, r_b);
                insert_buffer(b_idx, i | (x ? ~C_INDEX : uint32_t(0)));
                if (buffer_count_ == buffer_size) {
                    c_commit();
                }
                ret = x ? 1 : -1;
                p_sum_ += ret;
                break;
            }
            d_idx += r_b;
            val = !val;
        }
        return ret;
    }

    uint32_t c_rank(uint32_t n) const {
        uint32_t q_n = n;
        uint32_t count = 0;
        for (uint8_t b_idx = 0; b_idx < buffer_count_; b_idx++) {
            uint32_t e_index = buffer_[b_idx] & C_INDEX;
            if (e_index < n) {
                q_n--;
                count += buffer_[b_idx] >> 31;
            } else {
                break;
            }
        }
        uint32_t c_i = 0;
        bool val = type_info_ & C_ONE_MASK;
        uint8_t* data = reinterpret_cast<uint8_t*>(data_);
        for (uint32_t d_idx = 0; d_idx < run_index_[0]; d_idx++) {
            uint32_t rl = 0;
            if ((data[d_idx] & 0b11000000) == 0b11000000) {
                rl = data[d_idx] & 0b00111111;
            } else if ((data[d_idx] >> 7) == 0) {
                rl = data[d_idx++];
                rl = (rl << 8) | data[d_idx++];
                rl = (rl << 8) | data[d_idx++];
                rl = (rl << 8) | data[d_idx];
            } else {
                rl = data[d_idx++] & 0b00011111;
                rl = (rl << 8) | data[d_idx];
                if ((data[d_idx - 1] & 0b10100000) == 0b10100000) {
                    rl = (rl << 8) | data[++d_idx];
                }
            }
            if (c_i + rl >= q_n) {
                count += val * (q_n - c_i);
                break;
            }
            c_i += rl;
            count += rl * val;
            val = !val;
        }
        return count;
    }

    uint32_t c_select(uint32_t x) const {
        // std::cout << "c_select(" << x << ") called" << std::endl;
        bool val = type_info_ & C_ONE_MASK;
        uint8_t* data = reinterpret_cast<uint8_t*>(data_);
        uint8_t b_idx = 0;
        uint32_t e_index = buffer_[b_idx] & C_INDEX;
        uint32_t c_i = 0;
        uint32_t count = 0;
        uint32_t d_idx = 0;
        while (d_idx < run_index_[0]) {
            uint32_t rl = 0;
            if ((data[d_idx] & 0b11000000) == 0b11000000) {
                rl = data[d_idx++] & 0b00111111;
            } else if ((data[d_idx] >> 7) == 0) {
                rl = data[d_idx++] << 24;
                rl |= data[d_idx++] << 16;
                rl |= data[d_idx++] << 8;
                rl |= data[d_idx++];
            } else if ((data[d_idx] & 0b10100000) == 0b10100000) {
                rl = (data[d_idx++] & 0b00011111) << 16;
                rl |= data[d_idx++] << 8;
                rl |= data[d_idx++];
            } else {
                rl = (data[d_idx++] & 0b00011111) << 8;
                rl |= data[d_idx++];
            }
            // std::cout << "rl = " << rl << ", e_index = " << e_index
            //           << ", c_i = " << c_i << std::endl;
            while (b_idx < buffer_count_ && e_index < c_i + rl) {
                if (count + val * (e_index - c_i) >= x) {
                    // std::cout << "Split before buffer" << std::endl;
                    return c_i + x - count - 1;
                }
                bool b_val = buffer_[b_idx] >> 31;
                // std::cout << "count = " << count << ", b_val = " << b_val
                //           << std::endl;
                if (b_val) {
                    if (count + val * (e_index - c_i) + 1 == x) {
                        // std::cout << "Split in buffer" << std::endl;
                        return e_index;
                    }
                    count++;
                }
                c_i++;
                b_idx++;
                e_index = b_idx < buffer_count_ ? buffer_[b_idx] & C_INDEX : 0;
                [[unlikely]] (void(0));
            }
            if (count + (val * rl) >= x) {
                c_i += x - count;
                // std::cout << "Full run" << std::endl;
                return --c_i;
            }
            count += val * rl;
            c_i += rl;
            val = !val;
        }
        while (b_idx < buffer_count_) {
            count += buffer_[b_idx] >> 31;
            c_i++;
            if (count == x) break;
            b_idx++;
            e_index = b_idx < buffer_count_ ? buffer_[b_idx] & C_INDEX : 0;
        }
        if (c_i >= size_) {
            return size_;
        }
        return --c_i;
    }

    uint32_t c_select0(uint32_t x) const {
        //Flip the value to count 0's as 1's
        bool val = !(type_info_ & C_ONE_MASK);
        uint8_t* data = reinterpret_cast<uint8_t*>(data_);
        uint8_t b_idx = 0;
        uint32_t e_index = buffer_[b_idx] & C_INDEX;
        uint32_t c_i = 0;
        uint32_t count = 0;
        uint32_t d_idx = 0;
        while (d_idx < run_index_[0]) {
            uint32_t rl = 0;
            if ((data[d_idx] & 0b11000000) == 0b11000000) {
                rl = data[d_idx++] & 0b00111111;
            } else if ((data[d_idx] >> 7) == 0) {
                rl = data[d_idx++] << 24;
                rl |= data[d_idx++] << 16;
                rl |= data[d_idx++] << 8;
                rl |= data[d_idx++];
            } else if ((data[d_idx] & 0b10100000) == 0b10100000) {
                rl = (data[d_idx++] & 0b00011111) << 16;
                rl |= data[d_idx++] << 8;
                rl |= data[d_idx++];
            } else {
                rl = (data[d_idx++] & 0b00011111) << 8;
                rl |= data[d_idx++];
            }
            while (b_idx < buffer_count_ && e_index < c_i + rl) {
                if (count + val * (e_index - c_i) >= x) {
                    return c_i + x - count - 1;
                }
                //Flip the value to count 0's
                bool b_val = !(buffer_[b_idx] >> 31);

                if (b_val) {
                    if (count + val * (e_index - c_i) + 1 == x) {
                        return e_index;
                    }
                    count++;
                }
                c_i++;
                b_idx++;
                e_index = b_idx < buffer_count_ ? buffer_[b_idx] & C_INDEX : 0;
                [[unlikely]] (void(0));
            }  
            if (count + (val * rl) >= x) {
                c_i += x - count;
                return --c_i;
            }
            count += val * rl;
            c_i += rl;
            val = !val;
        }
    
        while (b_idx < buffer_count_) {
            count += !(buffer_[b_idx] >> 31);
            c_i++;
            if (count == x) break;
            b_idx++;
            e_index = b_idx < buffer_count_ ? buffer_[b_idx] & C_INDEX : 0;
        }
        if (c_i >= size_) {
            return size_;
        }
        return --c_i;
    }

    void c_append(leaf* other, uint32_t elems) {
        uint32_t copied = 0;
        bool val = other->first_value();
        const uint8_t* o_data = reinterpret_cast<const uint8_t*>(other->data());
        uint8_t o_buf_count = other->buffer_count();
        uint32_t* o_buf = other->buffer();
        uint32_t old_size = size_;
        uint8_t b_idx = 0;
        uint32_t d_idx = 0;
        while (copied < elems) {
            uint32_t rl = 0;
            if ((o_data[d_idx] & 0b11000000) == 0b11000000) {
                rl = o_data[d_idx++] & 0b00111111;
                // std::cout << "1 byte run: " << rl << std::endl;
            } else if ((o_data[d_idx] >> 7) == 0) {
                rl = o_data[d_idx++] << 24;
                rl |= o_data[d_idx++] << 16;
                rl |= o_data[d_idx++] << 8;
                rl |= o_data[d_idx++];
                // std::cout << "4 byte run: " << rl << std::endl;
            } else if ((o_data[d_idx] & 0b11100000) == 0b10000000) {
                rl = (o_data[d_idx++] & 0b00011111) << 8;
                rl |= o_data[d_idx++];
                // std::cout << "2 byte run: " << rl << std::endl;
            } else {
                rl = (o_data[d_idx++] & 0b00011111) << 16;
                rl |= o_data[d_idx++] << 8;
                rl |= o_data[d_idx++];
                // std::cout << "3 byte run: " << rl << std::endl;
            }
            while (b_idx < o_buf_count &&
                   (o_buf[b_idx] & C_INDEX) <= rl + copied) {
                rl++;
                b_idx++;
                // std::cout << "Added buffer" << std::endl;
            }
            // std::cout << "Run of " << rl << "elements" << std::endl;
            rl = rl < elems - copied ? rl : elems - copied;
            if (val) {
                uint64_t offset = size_ % WORD_BITS;
                uint64_t w_idx = size_ / WORD_BITS;
                uint32_t write = rl;
                p_sum_ += rl;
                if (offset != 0) {
                    if (write < WORD_BITS - offset) {
                        data_[w_idx++] |= ((uint64_t(1) << write) - 1)
                                          << offset;
                        write = 0;
                    } else {
                        data_[w_idx++] |= (~uint64_t(0)) << offset;
                        write -= WORD_BITS - offset;
                    }
                }
                while (write >= WORD_BITS) {
                    data_[w_idx++] = ~uint64_t(0);
                    write -= WORD_BITS;
                }
                if (write > 0) {
                    data_[w_idx] = (uint64_t(1) << write) - 1;
                }
            }
            size_ += rl;
            copied += rl;
            val = !val;
        }
        for (b_idx = 0; b_idx < o_buf_count; b_idx++) {
            uint32_t e_idx = o_buf[b_idx] & C_INDEX;
            if (e_idx >= copied) break;
            uint32_t w_idx = old_size + e_idx;
            uint32_t w_offset = w_idx % WORD_BITS;
            w_idx /= WORD_BITS;
            if (((data_[w_idx] >> w_offset) & MASK) ==
                uint64_t(o_buf[b_idx] >> 31)) {
                continue;
            }
            data_[w_idx] &= ~(uint64_t(1) << w_offset);
            data_[w_idx] |= uint64_t(o_buf[b_idx] >> 31) << w_offset;
            p_sum_ += (o_buf[b_idx] >> 31) ? 1 : -1;
        }
    }

    void c_transfer_prepend(leaf* other, uint32_t elems) {
        assert(elems < other->size());
        uint32_t* o_buf = other->buffer();
        uint8_t o_buf_count = other->buffer_count();
        uint8_t b_idx = 0;
        const uint8_t* o_data = reinterpret_cast<const uint8_t*>(other->data());
        uint32_t d_idx = 0;
        bool val = other->first_value();
        uint32_t copied = 0;
        uint32_t loc = 0;
        while (loc < other->size() - elems) {
            uint32_t rl = 0;
            if ((o_data[d_idx] & 0b11000000) == 0b11000000) {
                rl = o_data[d_idx++] & 0b00111111;
            } else if ((o_data[d_idx] >> 7) == 0) {
                rl = o_data[d_idx++] << 24;
                rl |= o_data[d_idx++] << 16;
                rl |= o_data[d_idx++] << 8;
                rl |= o_data[d_idx++];
            } else if ((o_data[d_idx] & 0b11100000) == 0b10000000) {
                rl = (o_data[d_idx++] & 0b00011111) << 8;
                rl |= o_data[d_idx++];
            } else {
                rl = (o_data[d_idx++] & 0b00011111) << 16;
                rl |= o_data[d_idx++] << 8;
                rl |= o_data[d_idx++];
            }
            for (; b_idx < o_buf_count; b_idx++) {
                if ((o_buf[b_idx] & C_INDEX) <= loc + rl) {
                    rl++;
                } else {
                    break;
                }
            }
            if (loc + rl > other->size() - elems) {
                uint32_t to_copy = loc + rl - (other->size() - elems);
                if (val) {
                    size_t i = 0;
                    for (; (i + 1) * 64 <= to_copy; i++) {
                        data_[i] |= ~uint64_t(0);
                    }
                    data_[i] |= (uint64_t(1) << (to_copy % 64)) - 1;
                    p_sum_ += to_copy;
                }
                copied += to_copy;
            }
            loc += rl;
            val = !val;
        }
        while (copied < elems) {
            uint32_t rl = 0;
            if ((o_data[d_idx] & 0b11000000) == 0b11000000) {
                rl = o_data[d_idx++] & 0b00111111;
            } else if ((o_data[d_idx] >> 7) == 0) {
                rl = o_data[d_idx++] << 24;
                rl |= o_data[d_idx++] << 16;
                rl |= o_data[d_idx++] << 8;
                rl |= o_data[d_idx++];
            } else if ((o_data[d_idx] & 0b11100000) == 0b10000000) {
                rl = (o_data[d_idx++] & 0b00011111) << 8;
                rl |= o_data[d_idx++];
            } else {
                rl = (o_data[d_idx++] & 0b00011111) << 16;
                rl |= o_data[d_idx++] << 8;
                rl |= o_data[d_idx++];
            }
            for (; b_idx < o_buf_count; b_idx++) {
                if ((o_buf[b_idx] & C_INDEX) <= loc + rl) {
                    rl++;
                } else {
                    break;
                }
            }
            if (val) {
                uint64_t offset = copied % WORD_BITS;
                uint64_t w_idx = copied / WORD_BITS;
                uint32_t write = rl;
                if (offset != 0) {
                    if (write < WORD_BITS - offset) {
                        data_[w_idx++] |= ((uint64_t(1) << write) - 1)
                                          << offset;
                        write = 0;
                    } else {
                        data_[w_idx++] |= (~uint64_t(0)) << offset;
                        write -= WORD_BITS - offset;
                    }
                }
                while (write >= WORD_BITS) {
                    data_[w_idx++] |= ~uint64_t(0);
                    write -= WORD_BITS;
                }
                if (write > 0) {
                    data_[w_idx] |= (uint64_t(1) << write) - 1;
                }
                p_sum_ += rl;
            }
            val = !val;
            copied += rl;
            loc += rl;
        }
        for (b_idx = 0; b_idx < o_buf_count; b_idx++) {
            uint32_t e_idx = o_buf[b_idx] & C_INDEX;
            if (e_idx >= other->size() - elems) {
                uint32_t w_idx = e_idx - (other->size() - elems);
                uint64_t w_offset = w_idx % WORD_BITS;
                w_idx /= WORD_BITS;
                val = (data_[w_idx] >> w_offset) & MASK;
                if (val == (o_buf[b_idx] >> 31)) {
                    continue;
                }
                if (val) {
                    data_[w_idx] ^= MASK << w_offset;
                    p_sum_--;
                } else {
                    data_[w_idx] |= MASK << w_offset;
                    p_sum_++;
                }
            }
        }
        size_ += elems;
        other->clear_last(elems);
    }

    void flatten() {
        uint8_t b_idx = 0;
        uint32_t e_idx = buffer_[b_idx] & C_INDEX;
        uint32_t d_idx = 0;
        uint8_t* data = reinterpret_cast<uint8_t*>(data_);
        uint32_t elems = 0;
        bool val = type_info_ & C_ONE_MASK;
        memset(data_scratch, 0, capacity_ * 8);
        while (d_idx < run_index_[0]) {
            uint32_t rl = 0;
            if ((data[d_idx] & 0b11000000) == 0b11000000) {
                rl = data[d_idx++] & 0b00111111;
            } else if ((data[d_idx] >> 7) == 0) {
                rl = data[d_idx++] << 24;
                rl |= data[d_idx++] << 16;
                rl |= data[d_idx++] << 8;
                rl |= data[d_idx++];
            } else if ((data[d_idx] & 0b10100000) == 0b10100000) {
                rl = (data[d_idx++] & 0b00011111) << 16;
                rl |= data[d_idx++] << 8;
                rl |= data[d_idx++];
            } else {
                rl = (data[d_idx++] & 0b00011111) << 8;
                rl |= data[d_idx++];
            }
            while (b_idx < buffer_count_ && e_idx <= elems + rl) {
                rl++;
                b_idx++;
                e_idx = b_idx < buffer_count_ ? buffer_[b_idx] & C_INDEX : 0;
            }
            if (val) {
                uint64_t offset = elems % WORD_BITS;
                uint64_t w_idx = elems / WORD_BITS;
                uint32_t write = rl;
                if (offset != 0) {
                    if (write < WORD_BITS - offset) {
                        data_scratch[w_idx++] |= ((uint64_t(1) << write) - 1)
                                                 << offset;
                        write = 0;
                    } else {
                        data_scratch[w_idx++] |= (~uint64_t(0)) << offset;
                        write -= WORD_BITS - offset;
                    }
                }
                while (write >= WORD_BITS) {
                    data_scratch[w_idx++] = ~uint64_t(0);
                    write -= WORD_BITS;
                }
                if (write > 0) {
                    data_scratch[w_idx] = (uint64_t(1) << write) - 1;
                }
            }
            elems += rl;
            val = !val;
        }
        memcpy(data_, data_scratch, capacity_ * 8);
        for (b_idx = 0; b_idx < buffer_count_; b_idx++) {
            uint64_t word = buffer_[b_idx] & C_INDEX;
            uint64_t offset = word % WORD_BITS;
            word /= WORD_BITS;
            data_[word] &= ~(MASK << offset);
            data_[word] |= uint64_t(buffer_[b_idx] >> 31) << offset;
            buffer_[b_idx] = uint32_t(0);
        }
        buffer_count_ = 0;
        type_info_ &= 0;
    }

    template <bool commit_buffer = true>
    void c_commit() {
        uint8_t b_idx = 0;
        uint32_t d_idx = 0;
        uint32_t e_idx = buffer_[b_idx] & C_INDEX;
        uint32_t elem_count = 0;
        uint32_t copied = 0;
        bool val = type_info_ & C_ONE_MASK;
        bool first = commit_buffer ? c_at(0) : val;
        type_info_ &= 0b00011111;
        uint8_t* data = reinterpret_cast<uint8_t*>(data_);
#pragma GCC diagnostic ignored "-Warray-bounds"
        while (d_idx < run_index_[0]) {
#pragma GCC diagnostic pop
            uint32_t rl = 0;
            if ((data[d_idx] & 0b10000000) == 0) {
                rl = data[d_idx++] << 24;
                rl |= data[d_idx++] << 16;
                rl |= data[d_idx++] << 8;
                rl |= data[d_idx++];
            } else if ((data[d_idx] & 0b11000000) == 0b11000000) {
                rl = data[d_idx++] & 0b00111111;
            } else if ((data[d_idx] & 0b10100000) == 0b10100000) {
                rl = (data[d_idx++] & 0b00011111) << 16;
                rl |= data[d_idx++] << 8;
                rl |= data[d_idx++];
            } else {
                rl = (data[d_idx++] & 0b00011111) << 8;
                rl |= data[d_idx++];
            }

            // Extend current run while following run for other symbol is empty.
            while (d_idx < run_index_[0]) {
                uint32_t ed_idx = d_idx;
                uint32_t e_rl = 0;
                if ((data[ed_idx] & 0b10000000) == 0) {
                    e_rl = data[ed_idx++] << 24;
                    e_rl |= data[ed_idx++] << 16;
                    e_rl |= data[ed_idx++] << 8;
                    e_rl |= data[ed_idx++];
                } else if ((data[ed_idx] & 0b11000000) == 0b11000000) {
                    e_rl = data[ed_idx++] & 0b00111111;
                } else if ((data[ed_idx] & 0b10100000) == 0b10100000) {
                    e_rl = (data[ed_idx++] & 0b00011111) << 16;
                    e_rl |= data[ed_idx++] << 8;
                    e_rl |= data[ed_idx++];
                } else {
                    e_rl = (data[ed_idx++] & 0b00011111) << 8;
                    e_rl |= data[ed_idx++];
                }
                if (e_rl || ed_idx >= run_index_[0]) {
                    [[likely]] break;
                }
                e_rl = 0;
                if ((data[ed_idx] & 0b10000000) == 0) {
                    e_rl = data[ed_idx++] << 24;
                    e_rl |= data[ed_idx++] << 16;
                    e_rl |= data[ed_idx++] << 8;
                    e_rl |= data[ed_idx++];
                } else if ((data[ed_idx] & 0b11000000) == 0b11000000) {
                    e_rl = data[ed_idx++] & 0b00111111;
                } else if ((data[ed_idx] & 0b10100000) == 0b10100000) {
                    e_rl = (data[ed_idx++] & 0b00011111) << 16;
                    e_rl |= data[ed_idx++] << 8;
                    e_rl |= data[ed_idx++];
                } else {
                    e_rl = (data[ed_idx++] & 0b00011111) << 8;
                    e_rl |= data[ed_idx++];
                }
                rl += e_rl;
                d_idx = ed_idx;
            }
            if constexpr (commit_buffer) {
                while (b_idx < buffer_count_ && e_idx <= copied + rl) {
                    if ((buffer_[b_idx] >> 31) == val) {
                        rl++;
                    } else if (e_idx == copied + rl) {
                        break;
                    } else {
                        uint32_t pre_count = e_idx - copied;
                        if (pre_count) {
                            rl -= pre_count;
                            elem_count = write_scratch(pre_count, elem_count);
                            copied += pre_count;
                        }
                        pre_count = 1;
                        while ((b_idx < buffer_count_ - 1) &&
                               ((buffer_[b_idx + 1] & C_INDEX) == e_idx + 1) &&
                               ((buffer_[b_idx + 1] >> 31) == !val)) {
                            pre_count++;
                            e_idx++;
                            [[unlikely]] b_idx++;
                        }
                        elem_count = write_scratch(pre_count, elem_count);
                        copied += pre_count;
                    }
                    b_idx++;
                    e_idx =
                        b_idx < buffer_count_ ? buffer_[b_idx] & C_INDEX : 0;
                }
            }
            if (rl) {
                elem_count = write_scratch(rl, elem_count);
                copied += rl;
            } else if (!commit_buffer && copied == 0) {
                [[unlikely]] first = !val;
            }
            if (elem_count * 8 >= size_) {
                flatten();
                return;
            }
            val = !val;
        }
        if constexpr (commit_buffer) {
            while (b_idx < buffer_count_) {
                uint32_t rl = 1;
                while ((b_idx < buffer_count_ - 1) &&
                       ((buffer_[b_idx + 1] >> 31) == (buffer_[b_idx] >> 31))) {
                    rl++;
                    b_idx++;
                }
                elem_count = write_scratch(rl, elem_count);
                b_idx++;
            }
            if (elem_count * 8 > size_) {
                flatten();
                return;
            }
        }
        type_info_ &= 0b11100000;
        type_info_ |= 0b00000010;
        type_info_ |= first ? 0b00000001 : 0b00000000;
        assert(capacity_ * 8 >= elem_count);
        memcpy(data_, data_scratch, elem_count);
        memset(data + elem_count, 0, 8 * capacity_ - elem_count);
        if constexpr (commit_buffer) {
            memset(buffer_, 0, sizeof(buffer_));
            buffer_count_ = 0;
        }
#pragma GCC diagnostic ignored "-Warray-bounds"
        run_index_[0] = elem_count;
#pragma GCC diagnostic pop
    }

    void c_rle_check_convert() {
        run_index_[0] = 0;
        type_info_ &= 0b00011110;
        type_info_ |= data_[0] & MASK;
        uint32_t i = 0;
        while (i < size_) {
            uint32_t rl = 1;
            uint64_t val = (data_[i / 64] >> (i % 64)) & MASK;
            while (i < size_ - 1 &&
                   (data_[(i + 1) / 64] >> ((i + 1) % 64) & MASK) == val) {
                i++;
                rl++;
            }
            run_index_[0] = write_scratch(rl, run_index_[0]);
            if (run_index_[0] * 8 >= size_) {
                [[unlikely]] return;
            }
            i++;
        }
        memcpy(data_, data_scratch, run_index_[0]);
        uint8_t* data = reinterpret_cast<uint8_t*>(data_);
        memset(data + run_index_[0], 0, 8 * capacity_ - run_index_[0]);
        type_info_ |= C_TYPE_MASK;
    }

    uint32_t write_scratch(uint32_t rl, uint32_t elem_count) {
        uint32_t r_b = 32 - __builtin_clz(rl);
        uint8_t* data = reinterpret_cast<uint8_t*>(data_scratch);
        uint8_t r_bytes = 1;
        // std::cout << "Writing " << rl << " to " << elem_count << std::endl;
        if (r_b < 7) {
            data[elem_count++] = 0b11000000 | uint8_t(rl);
        } else if (r_b < 14) {
            data[elem_count++] = 0b10000000 | uint8_t(rl >> 8);
            data[elem_count++] = uint8_t(rl);
            r_bytes = 2;
        } else if (r_b < 22) {
            data[elem_count++] = 0b10100000 | uint8_t(rl >> 16);
            data[elem_count++] = uint8_t(rl >> 8);
            data[elem_count++] = uint8_t(rl);
            r_bytes = 3;
        } else {
            data[elem_count++] = uint8_t(rl >> 24);
            data[elem_count++] = uint8_t(rl >> 16);
            data[elem_count++] = uint8_t(rl >> 8);
            data[elem_count++] = uint8_t(rl);
            r_bytes = 4;
        }
        // std::cout << "\t" << int(r_bytes) << " byte(s)" << std::endl;
        if (r_bytes > (type_info_ >> 5)) {
            type_info_ = (type_info_ & 0b00011111) | (r_bytes << 5);
        }
        return elem_count;
    }

    uint64_t c_dump(uint64_t* target, uint64_t start) {
        uint8_t b_idx = 0;
        uint32_t e_idx = buffer_[b_idx] & C_INDEX;
        bool val = type_info_ & C_ONE_MASK;
        uint32_t d_idx = 0;
        uint8_t* data = reinterpret_cast<uint8_t*>(data_);
        uint32_t loc = 0;
        uint64_t w_idx = start / WORD_BITS;
        uint64_t offset = start % 64;
        while (d_idx < run_index_[0]) {
            uint32_t rl = 0;
            if ((data[d_idx] & 0b11000000) == 0b11000000) {
                rl = data[d_idx++] & 0b00111111;
            } else if ((data[d_idx] & 0b10000000) == 0) {
                rl = data[d_idx++] << 24;
                rl |= data[d_idx++] << 16;
                rl |= data[d_idx++] << 8;
                rl |= data[d_idx++];
            } else if ((data[d_idx] & 0b10100000) == 0b10100000) {
                rl = data[d_idx++] << 16;
                rl |= data[d_idx++] << 8;
                rl |= data[d_idx++];
            } else {
                rl = data[d_idx++] << 8;
                rl |= data[d_idx++];
            }
            while (b_idx < buffer_count_ && loc + rl > e_idx) {
                uint32_t write = e_idx - loc;
                rl -= write;
                loc += write;
                start += write;
                if (val) {
                    if (offset) {
                        if (write < WORD_BITS - offset) {
                            target[w_idx] |= ((MASK << write) - 1) << offset;
                            write = 0;
                        } else {
                            target[w_idx++] |= (~uint64_t(0)) << offset;
                            write -= WORD_BITS - offset;
                        }
                    }
                    while (write >= WORD_BITS) {
                        target[w_idx++] = ~uint64_t(0);
                        write -= WORD_BITS;
                    }
                    if (write) {
                        target[w_idx] = (MASK << write) - 1;
                    }
                }
                target[loc / WORD_BITS] |= uint64_t(buffer_[b_idx++] >> 31)
                                           << (loc++ % WORD_BITS);
                e_idx = b_idx < buffer_count_ ? buffer_[b_idx] & C_INDEX : 0;
                w_idx = start / WORD_BITS;
                offset = start % WORD_BITS;
            }
            loc += rl;
            start += rl;
            if (val) {
                if (offset) {
                    if (rl < WORD_BITS - offset) {
                        target[w_idx] |= ((MASK << rl) - 1) << offset;
                        rl = 0;
                    } else {
                        target[w_idx++] |= (~uint64_t(0)) << offset;
                        rl -= WORD_BITS - offset;
                    }
                }
                while (rl >= WORD_BITS) {
                    target[w_idx++] = ~uint64_t(0);
                    rl -= WORD_BITS;
                }
                if (rl) {
                    target[w_idx] = (MASK << rl) - 1;
                }
            }
            w_idx = start / WORD_BITS;
            offset = start % WORD_BITS;
            val = !val;
        }
        return start;
    }

    void write_run(uint32_t length, uint32_t index, uint8_t c_bytes) {
        uint8_t* data = reinterpret_cast<uint8_t*>(data_);
        switch (c_bytes) {
            case 1:
                data[index] = 0b11000000 | uint8_t(length);
                return;
            case 2:
                data[index++] = 0b10000000 | uint8_t(length >> 8);
                data[index] = uint8_t(length);
                return;
            case 3:
                data[index++] = 0b10100000 | uint8_t(length >> 16);
                data[index++] = uint8_t(length >> 8);
                data[index] = uint8_t(length);
                return;
            default:
                data[index++] = uint8_t(length >> 24);
                data[index++] = uint8_t(length >> 16);
                data[index++] = uint8_t(length >> 8);
                data[index] = uint8_t(length);
        }
    }

    void write_run(uint32_t length) {
        uint32_t r_b = 32 - __builtin_clz(length);
        uint8_t* data = reinterpret_cast<uint8_t*>(data_);
        uint8_t rl = 1;
        if (r_b < 7) {
            data[run_index_[0]++] = 0b11000000 | uint8_t(length);
        } else if (r_b < 14) {
            data[run_index_[0]++] = 0b10000000 | uint8_t(length >> 8);
            data[run_index_[0]++] = uint8_t(length);
            rl = 2;
        } else if (r_b < 22) {
            data[run_index_[0]++] = 0b10100000 | uint8_t(length >> 16);
            data[run_index_[0]++] = uint8_t(length >> 8);
            data[run_index_[0]++] = uint8_t(length);
            rl = 3;
        } else {
            data[run_index_[0]++] = uint8_t(length >> 24);
            data[run_index_[0]++] = uint8_t(length >> 16);
            data[run_index_[0]++] = uint8_t(length >> 8);
            data[run_index_[0]++] = uint8_t(length);
            rl = 4;
        }
        if (rl > (type_info_ >> 5)) {
            type_info_ = (type_info_ & 0b00011111) | (rl << 5);
        }
    }

    void c_print(bool internal_only) const {
        std::cout << "{\n\"type\": \"c_leaf\",\n"
                  << "\"size\": " << size_ << ",\n"
                  << "\"capacity\": " << capacity_ << ",\n"
                  << "\"p_sum\": " << p_sum_ << ",\n"
                  << "\"first_value\": " << bool(type_info_ & C_ONE_MASK)
                  << ",\n"
                  << "\"Uncommitted removal\": "
                  << bool(type_info_ & C_RUN_REMOVAL_MASK) << ",\n"
                  << "\"Run bytes\": " << run_index_[0] << ",\n"
                  << "\"buffer_size\": " << int(buffer_size) << ",\n"
                  << "\"buffer_count\": " << int(buffer_count_) << ",\n"
                  << "\"buffer\": [\n";
        for (uint8_t i = 0; i < buffer_count_; i++) {
#pragma GCC diagnostic ignored "-Warray-bounds"
            std::cout << "{\"is_insertion\": " << true << ", "
                      << "\"buffer_value\": " << bool(buffer_[i] >> 31) << ", "
                      << "\"buffer_index\": " << (buffer_[i] & C_INDEX) << "}";
#pragma GCC diagnostic pop
            if (i != buffer_count_ - 1) {
                std::cout << ",\n";
            }
        }
        if (internal_only) {
            std::cout << "]}";
            return;
        }
        std::cout << "],\n\"runs\": [\n";
        uint32_t d_idx = 0;
        uint8_t* data = reinterpret_cast<uint8_t*>(data_);
        while (d_idx < run_index_[0]) {
            uint32_t rl = 0;
            uint32_t r_bytes = 1;
            if ((data[d_idx] & 0b11000000) == 0b11000000) {
                rl = data[d_idx] & 0b00111111;
            } else if ((data[d_idx] >> 7) == 0) {
                rl = data[d_idx] << 24;
                rl |= data[d_idx + 1] << 16;
                rl |= data[d_idx + 2] << 8;
                rl |= data[d_idx + 3];
                r_bytes = 4;
            } else if ((data[d_idx] & 0b10100000) == 0b10100000) {
                rl = (data[d_idx] & 0b00011111) << 16;
                rl |= data[d_idx + 1] << 8;
                rl |= data[d_idx + 2];
                r_bytes = 3;
            } else {
                rl = (data[d_idx] & 0b00011111) << 8;
                rl |= data[d_idx + 1];
                r_bytes = 2;
            }

            std::cout << "{\"run_index\": " << d_idx
                      << ", \"run_value\": " << rl << ", \"run_data\": \"";
            for (uint32_t i = d_idx; i < d_idx + r_bytes; i++) {
                std::bitset<8> b(data[i]);
                std::cout << b;
                if (i < d_idx + r_bytes - 1) {
                    std::cout << " ";
                }
            }
            d_idx += r_bytes;
            if (d_idx == run_index_[0]) {
                std::cout << "\"}";
            } else {
                std::cout << "\"},\n";
            }
        }
        std::cout << "],\n\"data\": [\n";
        for (uint64_t k = 0; k < capacity_ * 8; k += 8) {
            uint64_t lim = std::min(k + 8, uint64_t(capacity_ * 8));
            std::cout << "\"";
            for (uint64_t i = k; i < lim; i++) {
                std::bitset<8> b(data[i]);
                std::cout << b;
                if (i < lim - 1) {
                    std::cout << " ";
                }
            }
            std::cout << "\"";
            if (k + 8 < capacity_ * 8) {
                std::cout << ",\n";
            }
        }
        std::cout << "]}";
    }
};
}  // namespace bv
#endif