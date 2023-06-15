#ifndef BV_LEAF_HPP
#define BV_LEAF_HPP

#include <immintrin.h>

#include <cstdint>
#include <cstring>
#include <cassert>

#include "buffer.hpp"
#include "libpopcnt.h"
#include "uncopyable.hpp"

namespace bv {
template <uint16_t buffer_size, uint32_t leaf_size, bool avx = true,
          bool compressed = false, bool sorted_buffers = true>
class leaf : uncopyable {
   private:
    inline static uint64_t data_scratch[leaf_size / 64];
    static const constexpr uint32_t LEAF_BYTES =
        sizeof(uint32_t) * 3 +  // size, partial sum and capacity.
        sizeof(uint32_t) *
            (compressed ? 1 : 0) +  // run index for compressed leaves.
        (buffer_size ? sizeof(buffer<buffer_size, compressed, sorted_buffers>)
                     : 0) +  // buffer
        sizeof(uint8_t) *
            (compressed ? 1 : 0);  // meta data for compressed leaves.
    uint8_t members_[LEAF_BYTES];

    // Hybrid compressed leaves should be buffered.
    static_assert(!compressed || buffer_size > 0);

    // Larger leaf size would lead to undefined behaviour. due to buffer
    // overflow.
    static_assert(leaf_size < (uint32_t(1) << 30));

    /** @brief 0x1 to be used in  bit operations. */
    static const constexpr uint64_t ONE = 1;
    /** @brief Number of bits in a computer word. */
    static const constexpr uint64_t WORD_BITS = 64;
    /** @brief Maximum size for compressed leaf. */
    static const constexpr uint32_t MAX_SIZE = (~uint32_t(0)) >> 2;
    /** @brief Mask for initial value of rle compressed leaves */
    static const constexpr uint8_t C_ONE_MASK = 0b00000001;
    /** @brief Mask for accessing type of possibly compressed leaf */
    static const constexpr uint8_t C_TYPE_MASK = 0b00000010;

   public: 
    static constexpr uint32_t init_capacity(uint32_t elems = 0) {
        uint32_t cap = elems / WORD_BITS + 2;
        cap += (elems % WORD_BITS ? 1 : 0);
        return cap;
    }

    /**
     * @brief Leaf constructor
     *
     * The intention of this constructor is to enable either allocation of a
     * larger consecutive memory block where the leaf struct can be placed at
     * the start followed by the "data" section, or the data section can be
     * allocated separately.
     *
     * @param capacity Number of 64-bit integers available for use in data.
     */
    leaf(uint32_t capacity, uint32_t elems = 0, bool val = false) : members_() {
        capacity_() = capacity;
        if constexpr (!compressed) {
            // Uncompressed leaf does not support instantiation to non-empty
            assert(elems == 0);
        } else {
            assert(elems <= MAX_SIZE);
        }
        if constexpr (compressed) {
            size_() = elems;
            p_sum_() = val ? elems : 0;
            run_index_() = 0;
            if (elems > 8) {
                append_run(elems);
                type_info_() |= C_TYPE_MASK;
                type_info_() |= val ? C_ONE_MASK : 0;
            } else {
                data_()[0] = (uint64_t(1) << elems) - 1;
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
    bool at(uint32_t i) {
        if constexpr (compressed) {
            if (is_compressed()) {
                return c_at(i);
            }
        }
        if constexpr (buffer_size != 0) {
            uint64_t index = i;
            typedef buffer<buffer_size, false, sorted_buffers> buf_t;
            for (auto be : *reinterpret_cast<buf_t*>(buf_ptr_())) {
                uint32_t be_idx = be.index();
                if (be_idx == i) {
                    if constexpr (!sorted_buffers) {
                        return be.value();
                    }
                    if (be.is_insertion()) [[unlikely]] {
                        return be.value();
                    }
                    index++;
                } else if (be_idx < i) [[likely]] {
                    if constexpr (!sorted_buffers) {
                        index--;
                    } else {
                        index += be.is_insertion() ? -1 : 1;
                    } 
                } else {
                    break;
                }
            }
            return ONE & (data_()[index / WORD_BITS] >> (index % WORD_BITS));
        }
        return ONE & (data_()[i / WORD_BITS] >> (i % WORD_BITS));
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
        if (i == size_) [[unlikely]] {
            // Convert to append if applicable.
            push_back(x);
            return;
        }
        p_sum_() += uint32_t(x);
        ++size_();
        if constexpr (buffer_size) {
            typedef buffer<buffer_size, false, sorted_buffers> buf_t;
            buf_t* buf = reinterpret_cast<buf_t*>(buf_ptr_());
            buf->insert(x, i);
            if (buf->is_full()) [[unlikely]] {
                commit();
            }
        } else {
            // If there is no buffer, a simple linear time insertion is done
            // instead.
            uint32_t target_word = i / WORD_BITS;
            uint32_t target_offset = i % WORD_BITS;
            for (uint32_t j = capacity_() - 1; j > target_word; j--) {
                data_()[j] <<= 1;
                data_()[j] |= (data_()[j - 1] >> 63);
            }
            data_()[target_word] =
                (data_()[target_word] & ((ONE << target_offset) - 1)) |
                ((data_()[target_word] & ~((ONE << target_offset) - 1)) << 1);
            data_()[target_word] |= x ? (ONE << target_offset) : uint64_t(0);
        }
    }

    /** @brief Getter for p_sum_ */
    uint32_t p_sum() const { return p_sum_(); }
    /** @brief Getter for size_ */
    uint32_t size() const { return size_(); }
    /** @brief Getter for number of buffer elements */
    uint8_t buffer_count() const {
        if (is_compressed()) {
            typedef buffer<buffer_size, true, sorted_buffers> buf_t;
            return reinterpret_cast<buf_t*>(buf_ptr_())->size(); 
        } else {
            typedef buffer<buffer_size, false, sorted_buffers> buf_t;
            return reinterpret_cast<buf_t*>(buf_ptr_())->size(); 
        }
        
    }
    ///** @brief Get pointer to the buffer */
    //uint32_t* buffer() { return buffer_; }
    /** @brief Get the values for the first run */
    bool first_value() const {
        // Don't know if this is a good idea, but this function only
        // makes sense for compressed leaves.
        static_assert(compressed); 
        if (is_compressed()) {
            return type_info_() & C_ONE_MASK;
        }
        return at(0);
    }
    /** @brief Number of bytes used to encode content */
    uint32_t used_bytes() const {
        if constexpr (compressed) {
            if (is_compressed()) {
                return run_index_();
            }
        }
        return size_() + (size_() % 8 ? 1 : 0);
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
        assert(size_() < capacity_ * WORD_BITS);
        uint32_t pb_size = size_();
        if constexpr (buffer_size) {
            typedef buffer<buffer_size, false, sorted_buffers> buf_t;
            buf_t* buf = reinterpret_cast<buf_t*>(buf_ptr_());
            for (auto be : *buf) {
                pb_size += be.is_insertion() ? -1 : 1;
            }
        }

        // If the leaf has at some point been full and has subsequently shrunk
        // due to removals, the next available position to write to without
        // committing the buffer may be beyond the end of the data_ array. In
        // this case the buffer will need to be committed before appending the
        // new element.
        if (pb_size >= capacity_() * WORD_BITS) [[unlikely]] {
            commit();
            data_[size_() / WORD_BITS] |= uint64_t(x) << (size_() % WORD_BITS);
        } else {
            data_[pb_size / WORD_BITS] |= uint64_t(x) << (pb_size % WORD_BITS);
        }
        ++size_();
        p_sum_() += uint64_t(x);
    }

   private:
    bool is_compressed() const {
        if constexpr (compressed) {
            return type_info_() & C_TYPE_MASK;
        }
        return false;
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
    template <bool allow_convert = true>
    void commit() {
        if constexpr (buffer_size == 0) return;
        if constexpr (compressed) {
            if (is_compressed()) {
                c_commit<allow_convert>();
                return;
            }
        }
        //  Complicated bit manipulation but wacha gonna do. Hopefully won't
        //  need to debug this anymore.
        typedef buffer<buffer_size, false, sorted_buffers> buf_t;
        buf_t* buf = reinterpret_cast<buf_t*>(buf_ptr_());
        if (buf->size() == 0) [[unlikely]] {
            return;
        }
        // TODO: Replace all of this with a circular buffer...  
        uint32_t overflow = 0;
        uint16_t overflow_length = 0;
        uint16_t underflow_length = 0;
        auto be = buf->begin();
        auto buffer_end = buf->end();
        uint32_t target_word = be->index / WORD_BITS;
        uint32_t target_offset = be->index % WORD_BITS;

        uint32_t words = size_() / WORD_BITS;
        words += size_() % WORD_BITS > 0 ? 1 : 0;
        for (uint32_t current_word = 0; current_word < words; current_word++) {
            uint64_t underflow =
                current_word + 1 < capacity_() ? data_()[current_word + 1] : 0;
            if (overflow_length) [[likely]] {
                underflow =
                    (underflow << overflow_length) |
                    (data_()[current_word] >> (WORD_BITS - overflow_length));
            }

            uint64_t new_overflow = 0;
            // If buffers need to be commit to this word:
            if (current_word == target_word && be != buffer_end) {
                uint64_t word =
                    underflow_length
                        ? (data_()[current_word] >> underflow_length) |
                              (underflow << (WORD_BITS - underflow_length))
                        : (data_()[current_word] << overflow_length) | overflow;
                underflow >>= underflow_length;
                uint64_t new_word = 0;
                uint8_t start_offset = 0;
                // While there are buffers for this word
                while (current_word == target_word) [[unlikely]] {
                    new_word |=
                        (word << start_offset) & ((ONE << target_offset) - 1);
                    word = (word >> (target_offset - start_offset)) |
                           (target_offset == 0 ? 0
                            : target_offset - start_offset == 0
                                ? 0
                                : (underflow << (WORD_BITS - (target_offset -
                                                              start_offset))));
                    underflow >>= target_offset - start_offset;
                    if (be->is_insertion) {
                        if (be->value) {
                            new_word |= ONE << target_offset;
                        }
                        start_offset = target_offset + 1;
                        if (underflow_length) [[unlikely]] {
                            underflow_length--;
                        } else {
                            overflow_length++;
                        }
                    } else {
                        word >>= 1;
                        word |= underflow << 63;
                        underflow >>= 1;
                        if (overflow_length) {
                            overflow_length--;
                        } else [[likely]] {
                            underflow_length++;
                        }
                        start_offset = target_offset;
                    }
                    ++be;
                    if (be == buffer_end) [[unlikely]] {
                        break;
                    }
                    target_word = be->index / WORD_BITS;
                    target_offset = be->index % WORD_BITS;
                }
                new_word |= start_offset < WORD_BITS ? (word << start_offset)
                                                     : uint64_t(0);
                new_overflow =
                    overflow_length
                        ? data_()[current_word] >> (WORD_BITS - overflow_length)
                        : 0;
                [[unlikely]] data_()[current_word] = new_word;
            } else {
                if (underflow_length) {
                    data_()[current_word] =
                        (data_()[current_word] >> underflow_length) |
                        (underflow << (WORD_BITS - underflow_length));
                } else if (overflow_length) [[likely]] {
                    new_overflow =
                        data_()[current_word] >> (WORD_BITS - overflow_length);
                    data_()[current_word] =
                        (data_()[current_word] << overflow_length) | overflow;
                } else {
                    overflow = 0;
                }
            }
            overflow = new_overflow;
        }
        if (capacity_() > words) [[likely]] {
            data_()[words] = 0;
        }
        buf->clear();
        if constexpr (compressed && allow_convert) {
            c_rle_check_convert();
        }
    }

    bool c_at(uint32_t i) {
        uint32_t i_q = i;
        typedef buffer<buffer_size, true, sorted_buffers> buf_t;
        for (auto be : *reinterpret_cast<buf_t*>(buf_ptr_())) {
            if (be.index < i) [[likely]] {
                i_q--;
            } else if (be.index == i) {
                return be.value;
            } else {
                break;
            }
        }

        bool ret = type_info_() & C_ONE_MASK;
        ret = !ret;
        uint32_t c_i = 0;
        
        uint32_t r_idx = 0;
        while (c_i <= i_q) {
            c_i += read_run(r_idx);
            ret = !ret;
        }
        return ret;
    }

    void c_insert(uint32_t i, bool v) {
        typedef buffer<buffer_size, true, sorted_buffers> buf_t;
        buf_t* buf = reinterpret_cast<buf_t*>(buf_ptr_());
        buf->insert(v, i);
        size_()++;
        p_sum_() += v;
        if (buf->is_full()) {
            c_commit();
        }
    }

    template <bool commit_buffer = true>
    void c_commit() {
        typedef buffer<buffer_size, true, sorted_buffers> buf_t;
        buf_t* buf = reinterpret_cast<buf_t*>(buf_ptr_());

        uint32_t d_idx = 0;
        auto be = buf->begin();
        auto buffer_end = buf->end();
        uint32_t elem_count = 0;
        uint32_t copied = 0;
        bool val = type_info_() & C_ONE_MASK;
        bool first = val;
        if constexpr (commit_buffer) {
            first = be->index == 0 ? be->value : val;  
        }
        type_info_() &= 0b00011111;
        uint8_t* data = reinterpret_cast<uint8_t*>(data_());
        while (d_idx < run_index_()) {
            uint32_t rl = read_run(d_idx);
            
            // Extend current run while following run for other symbol is empty.
            while (d_idx < run_index_()) {
                uint32_t ed_idx = d_idx;
                uint32_t e_rl = read_run(ed_idx);
                if (e_rl || ed_idx >= run_index_()) [[likely]] {
                    break;
                }
                e_rl = read_run(ed_idx);
                rl += e_rl;
                d_idx = ed_idx;
            }
            if constexpr (commit_buffer) {
                while (be != buffer_end && be->index <= copied + rl) {
                    if (be->value == val) {
                        rl++;
                    } else if (be->index == copied + rl) {
                        break;
                    } else {
                        uint32_t pre_count = be->index - copied;
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

    uint32_t read_run(uint32_t& r_idx) {
        uint32_t rl;
        uint8_t* byte_data = reinterpret_cast<uint8_t*>(data_());
        if ((byte_data[r_idx] & 0b11000000) == 0b11000000) {
            rl = byte_data[r_idx++] & 0b00111111;
            // std::cout << "\t1 byte" << std::endl;
        } else if ((byte_data[r_idx] & 0b10000000) == 0) {
            rl = byte_data[r_idx++] << 24;
            rl |= byte_data[r_idx++] << 16;
            rl |= byte_data[r_idx++] << 8;
            rl |= byte_data[r_idx++];
            // std::cout << "\t4 bytes" << std::endl;
        } else if ((byte_data[r_idx] & 0b10100000) == 0b10100000) {
            rl = (byte_data[r_idx++] & 0b00011111) << 16;
            rl |= byte_data[r_idx++] << 8;
            rl |= byte_data[r_idx++];
            // std::cout << "\t3 bytes" << std::endl;
        } else {
            rl = (byte_data[r_idx++] & 0b00011111) << 8;
            rl |= byte_data[r_idx++];
            // std::cout << "\t2 bytes" << std::endl;
        }
        return rl;
    }

    void append_run(uint32_t length) {
        uint32_t r_b = 32 - __builtin_clz(length);
        uint8_t* byte_data = reinterpret_cast<uint8_t*>(data_());
        uint8_t rl = 1;
        if (r_b < 7) {
            byte_data[run_index_()++] = 0b11000000 | uint8_t(length);
        } else if (r_b < 14) {
            byte_data[run_index_()++] = 0b10000000 | uint8_t(length >> 8);
            byte_data[run_index_()++] = uint8_t(length);
            rl = 2;
        } else if (r_b < 22) {
            byte_data[run_index_()++] = 0b10100000 | uint8_t(length >> 16);
            byte_data[run_index_()++] = uint8_t(length >> 8);
            byte_data[run_index_()++] = uint8_t(length);
            rl = 3;
        } else {
            byte_data[run_index_()++] = uint8_t(length >> 24);
            byte_data[run_index_()++] = uint8_t(length >> 16);
            byte_data[run_index_()++] = uint8_t(length >> 8);
            byte_data[run_index_()++] = uint8_t(length);
            rl = 4;
        }
        if (rl > (type_info_() >> 5)) {
            type_info_() = (type_info_() & 0b00011111) | (rl << 5);
        }
    }

    uint32_t& size_() { return reinterpret_cast<uint32_t*>(members_)[0]; }
    uint32_t& p_sum_() { return reinterpret_cast<uint32_t*>(members_)[1]; }
    uint32_t& capacity_() { return reinterpret_cast<uint32_t*>(members_)[2]; }
    uint32_t& run_index_() {
        const constexpr uint32_t offset = compressed ? 3 : 0;
        return reinterpret_cast<uint32_t*>(members_)[offset];
    }
    uint8_t* buf_ptr_() {
        const constexpr uint32_t offset =
            sizeof(uint32_t) * (compressed ? 4 : 3);
        return members_ + offset;
    }
    uint8_t& type_info_() {
        const constexpr uint32_t offset =
            compressed
                ? 4 + (buffer_size ? sizeof(buffer<buffer_size, compressed,
                                                   sorted_buffers>)
                                   : 0)
                : 0;
        return members_[offset];
    }
    uint64_t* data_() {
        const constexpr uint32_t offset = LEAF_BYTES / 8 + (LEAF_BYTES % 8 ? 1 : 0);
        return reinterpret_cast<uint64_t*>(members_) + offset;
    }
};
}  // namespace bv
#endif