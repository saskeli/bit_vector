#ifndef BV_LEAF_HPP
#define BV_LEAF_HPP

#include <immintrin.h>

#include <bitset>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <utility>

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
template <uint8_t buffer_size, bool avx = true>
class leaf : uncopyable {
   protected:
    uint8_t buffer_count_;  ///< Number of elements in insert/remove buffer.
    uint16_t capacity_;     ///< Number of 64-bit integers available in data.
    uint32_t size_;         ///< Logical number of bits stored.
    uint32_t p_sum_;        ///< Logical number of 1-bits stored.
#pragma GCC diagnostic ignored "-Wpedantic"
    uint32_t buffer_[buffer_size];  ///< Insert/remove buffer.
#pragma GCC diagnostic pop
    uint64_t* data_;  ///< Pointer to data storage.
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

    // The buffer fill rate is stored in part of an 8-bit word.
    // This could be resolved with using 16 bits instead (without increasing the
    // sizeof(leaf) value but there does not really appear to be any reason to
    // do so.
    static_assert(buffer_size < WORD_BITS);

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
    leaf(uint16_t capacity, uint64_t* data) {
        buffer_count_ = 0;
        capacity_ = capacity;
        size_ = 0;
        p_sum_ = 0;
        data_ = data;
    }

    /**
     * @brief Get the value of the i<sup>th</sup> element in the leaf.
     *
     * @param i Index to check
     *
     * @return Value of bit at index i.
     */
    bool at(uint32_t i) const {
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
#ifdef DEBUG
        if (size_ >= capacity_ * WORD_BITS) {
            std::cerr << "Overflow. Reallocate before adding elements"
                      << "\n";
            std::cerr << "Attempted to insert to " << i << std::endl;
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
            for (uint32_t j = target_word + 1; j < uint32_t(capacity_ - 1); j++) {
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
        uint32_t pop = 0;
        uint32_t pos = 0;
        uint8_t current_buffer = 0;
        int8_t a_pos_offset = 0;

        // Step one 64-bit word at a time considering the buffer until pop >= x
        for (uint32_t j = 0; j < capacity_; j++) {
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
            if (pop >= x) {
                [[unlikely]] break;
            }
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
        // std::cout << "simple select: " << pos + 1 << std::endl;
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
        // std::cout << "select(" << x << ", " << pos << ", " << pop << ")" <<
        // std::endl; pos++;
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
        // std::cout << "block select: " << pos + 1 << std::endl;
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
     * additional insertion of new elements.
     *
     * @return True if there is no guarantee that an insertion can be completed
     *         without undefined behaviour.
     */
    bool need_realloc() const { return size_ >= capacity_ * WORD_BITS; }

    /**
     * @brief Size of the leaf-associated data storage in 64-bit words.
     *
     * Primarily intended for debugging and validation.
     *
     * @return Size of internal data storage in 64-bit words.
     */
    uint32_t capacity() const { return capacity_; }

    /**
     * @brief Set the size of the leaf-associated data storage.
     *
     * Intended only to be set by an allocator after allocating additional
     * storage for the leaf. Setting the capacity carelessly easily leads to
     * undefined behaviour.
     *
     * @param cap New size for `data_`
     */
    void capacity(uint32_t cap) { capacity_ = cap; }

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
     * leaf. Assumes that buffer has been committed before calling.
     *
     * Will update size and p_sum.
     *
     * @param elems number of elements to remove.
     */
    void clear_first(uint32_t elems) {
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
#ifdef DEBUG
        if (capacity_ * WORD_BITS < size_ + elems) {
            std::cerr << "Invalid append! Leaf only has room for "
                      << (capacity_ * WORD_BITS - size_)
                      << " bits  and " << elems << " was appended."
                      << std::endl;
            //exit(1);
        }
#endif
        commit();
        other->commit();

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
        commit();
        other->commit();
        const uint64_t* o_data = other->data();
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
                    data_[i] = o_data[source_word] << (WORD_BITS - source_offset);
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
     * Will now clear elements from "other" as it is assumed that "other" will
     * be deallocated .
     *
     * @param other Pointer to next leaf.
     */
    void append_all(leaf* other) {
        commit();
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

    /**
     * @brief Commit and clear the Insert/Remove buffer for the leaf.
     *
     * Intended for clearing a full buffer before insertion or removal, and for
     * ensuring an empty buffer before transfer operations.
     *
     * Slightly complicated but linear time function for committing all buffered
     * operations to the underlying data.
     */
    void commit() {
        // Complicated bit manipulation but whacha gonna do. Hopefully won't
        // need to debug this anymore.
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
                                : (underflow
                                   << (WORD_BITS - (target_offset - start_offset))));
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
                new_word |=
                    start_offset < WORD_BITS ? (word << start_offset) : uint64_t(0);
                new_overflow = overflow_length ? data_[current_word] >>
                                                     (WORD_BITS - overflow_length)
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
        if (capacity_ > words) [[likely]]
            data_[words] = 0;
        buffer_count_ = 0;
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
        assert(size_ <= capacity_ * WORD_BITS);
        assert(p_sum_ <= size_);
        assert(p_sum_ == rank(size_));
        uint32_t last_word = size_ / WORD_BITS;
        uint32_t overflow = size_ % WORD_BITS;
        if (overflow != 0) {
            for (uint32_t i = overflow; i < WORD_BITS; i++) {
                uint64_t val = (MASK << i) & data_[last_word];
                assert(val == 0u);
            }
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
     * @param internal_only Will not output raw data it true to save space.
     */
    void print(bool internal_only) const {
        std::cout << "{\n\"type\": \"leaf\",\n"
                  << "\"size\": " << size_ << ",\n"
                  << "\"capacity\": " << capacity_ << ",\n"
                  << "\"p_sum\": " << p_sum_ << ",\n"
                  << "\"buffer_size\": " << int(buffer_size) << ",\n"
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
                std::cout << "\"" << b << "\"";
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
        return std::pair<uint64_t, uint64_t>(capacity_ * WORD_BITS, size_);
    }

   protected:
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
    inline uint32_t buffer_index(uint32_t e) const { return (e) >> 8; }

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
};
}  // namespace bv
#endif