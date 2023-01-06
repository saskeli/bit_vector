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
template <uint8_t buffer_size, uint32_t leaf_size, bool avx = true,
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

    // Committing uses a 64 bit word as buffer, limiting the number of changes 
    // that can be committed at once.
    static_assert(buffer_size < 63);

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
                if (be.index == i) {
                    if constexpr (!sorted_buffers) {
                        return be.value;
                    }
                    if (be.is_insertion) [[unlikely]] {
                        return be.value;
                    }
                    index++;
                } else if (be.index < i) [[likely]] {
                    if constexpr (!sorted_buffers) {
                        index--;
                    } else {
                        index += be.is_insertion ? -1 : 1;
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
        p_sum_() += x ? 1 : 0;
        size_()++;
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
            size_()++;
            uint32_t target_word = i / WORD_BITS;
            uint32_t target_offset = i % WORD_BITS;
            for (uint32_t j = capacity_() - 1; j > target_word; j--) {
                data_()[j] <<= 1;
                data_()[j] |= (data_()[j - 1] >> 63);
            }
            data_()[target_word] =
                (data_()[target_word] & ((MASK << target_offset) - 1)) |
                ((data_()[target_word] & ~((MASK << target_offset) - 1)) << 1);
            data_()[target_word] |= x ? (MASK << target_offset) : uint64_t(0);
        }
    }

    /** @brief Getter for p_sum_ */
    uint32_t p_sum() { return p_sum_(); }
    /** @brief Getter for size_ */
    uint32_t size() { return size_(); }
    /** @brief Getter for number of buffer elements */
    uint8_t buffer_count() { 
        typedef buffer<buffer_size, compressed, sorted_buffers> buf_t;
        return reinterpret_cast<buf_t*>(buf_ptr_())->size(); 
    }
    ///** @brief Get pointer to the buffer */
    //uint32_t* buffer() { return buffer_; }
    /** @brief Get the values for the first run */
    bool first_value() {
        if constexpr (compressed) {
            if (is_compressed()) {
                return type_info_() & C_ONE_MASK;
            }
        }
        return at(0);
    }
    /** @brief Number of bytes used to encode content */
    uint32_t used_bytes() {
        if constexpr (compressed) {
            if (is_compressed()) {
                return run_index_();
            }
        }
        return size_() + (size_() % 8 ? 1 : 0);
    }

   private:
    bool is_compressed() {
        if constexpr (compressed) {
            return (type_info_() & C_TYPE_MASK) == C_TYPE_MASK;
        }
        return false;
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