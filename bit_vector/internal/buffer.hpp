#ifndef BV_BUF_HPP
#define BV_BUF_HPP

#include <cstdint>
#include <cstring>
#include <utility>

#include "uncopyable.hpp"

namespace bv {

template <uint16_t buffer_size, bool compressed, bool sorted>
class buffer {
   private:
    class BufferElement {
        static const constexpr uint32_t ONE = 1;
        static const constexpr uint8_t OFFSET = 0b10;
        uint32_t v_;

        BufferElement(uint32_t val) : v_(val) {}

       public:
        BufferElement() : v_() {}

        BufferElement(const BufferElement& other) : v_(other.v_) {}

        BufferElement(uint32_t idx, bool v)
            : v_((idx << OFFSET) | uint32_t(v)) {
            static_assert(compressed || !sorted);
        }

        BufferElement(uint32_t idx, bool v, bool t)
            : v_((idx << offset) | uint32_t(v) | (uint32_t(t) << ONE)) {
            static_assert(!compressed && sorted);
        }

        BufferElement& operator++() {
            v_ += uint32_t(ONE) << OFFSET;
            return *this;
        }

        BufferElement& operator+=(uint32_t i) {
            v_ += i << OFFSET;
            return *this;
        }

        BufferElement operator+(uint32_t i) {
            return v_ + (i << OFFSET);
        } 

        bool operator==(const BufferElement& rhs) const { return v_ == rhs.v_; }

        bool operator!=(const BufferElement& rhs) const { return v_ != rhs.v_; }

        bool operator<(const BufferElement& rhs) const {
            return (v_ >> ONE) < (rhs.v_ >> ONE);
        }

        bool operator<=(const BufferElement& rhs) const {
            return (v_ >> ONE) <= (rhs.v_ >> ONE);
        }

        bool operator>(const BufferElement& rhs) const {
            return (v_ >> ONE) > (rhs.v_ >> ONE);
        }

        bool operator>=(const BufferElement& rhs) const {
            return (v_ >> ONE) >= (rhs.v_ >> ONE);
        }

        uint32_t index() { return v_ >> OFFSET; }

        bool value() { return v_ & ONE; }

        bool is_insertion() {
            if constexpr (compressed || !sorted) {
                return true;
            }
            return bool(v_ & TYPE_MASK);
        }
    };

    static_assert(buffer_size > 0);
    static_assert(buffer_size <= ((1 << 16) - 1));
    static_assert(__builtin_popcount(buffer_size) == 1);
    inline static BufferElement scratch[buffer_size];

    BufferElement buffer_[buffer_size];
    uint16_t buffer_elems_;

    class buffer_iter {
       private:
        const buffer& buf_;
        uint16_t offset_;

       public:
        buffer_iter(const buffer& buf, uint16_t pos) : buf_(buf), offset_(pos) {}

        bool operator==(const buffer_iter& rhs) const {
            return (buf_ == rhs.buf_) && (offset_ == rhs.offset_);
        }

        bool operator!=(const buffer_iter& rhs) const {
            return !operator==(rhs);
        }

        const BufferElement& operator*() const { return buf[offset]; }

        buffer_iter& operator++() {
            ++offset_;
            return *this;
        }
    };

   public:
    buffer() : buffer_(), buffer_elems_() {}
    buffer(const buffer& other) { std::memcpy(this, &other, sizeof(buffer)); }
    buffer& operator=(const buffer& other) {
        std::memcpy(this, &other, sizeof(buffer));
    }

    void sort() {
        if constexpr (sorted) {
            return;
        }
        if (buffer_elems_ <= 1) [[unlikely]] {
            return;
        }
        const constexpr uint32_t max_val = (uint32_t(1) << 24) - 1;
        for (uint16_t i = 0; i < buffer_size - buffer_elems_; i++) [[unlikely]] {
            buffer_[buffer_size - 1 - i] = max_val - i;
        }
        sort<buffer_size>(scratch, buffer_);
    }

    bool is_full() const { return buffer_elems_ == buffer_size; }

    void insert(uint32_t idx, bool v) {
        BufferElement nb = {idx, v};
        if constexpr (!sorted) {
            buffer_[buffer_elems_++] = nb;
            return;
        }
        uint16_t i = buffer_elems_;
        while (i > 0) {
            if (nb <= buffer_[i - 1]) [[likely]] {
                buffer_[i] = ++buffer[i - 1];
            } else {
                break;
            }
            --i;
        }
        buffer_[i] = nb;
    }

    bool remove(uint32_t& idx, bool& v) {
        if constexpr (sorted && !compressed) {
            uint16_t i = buffer_elems_;
            while (i > 0) {
                uint32_t b_idx = val(buffer_[i - 1]);
                if (b_idx > idx) [[likely]] {
                    buffer_[i - 1]--;
                } else if (b_idx == idx) {
                    if (is_insertion(buffer_[i - 1])) {
                        std::memmove(buffer_ + (i - 1), buffer_ + i,
                                     buffer_elems_ - i - 1);
                        v = assoc_val(buffer_[i - 1]);
                        buffer_elems_--;
                        return true;
                    } else {
                        break;
                    }
                } else {
                    break;
                }
                i--;
            }
            if (i < buffer_elems_) {
                std::memmove(buffer_ + i, buffer_ + (i + 1), buffer_elems_ - i);
            }
            buffer_[i] = idx | (uint32_t(1) << 30);
            buffer_elems_++;
            while (i > 0) {
                idx += is_insertion(buffer_[i - 1]) ? -1 : 1;
                i--;
            }
            return false;
        } else {
            if constexpr (!sorted) {
                sort();
            }
            for (uint16_t i = buffer_elems_ - 1; i < buffer_elems_; i--) {
                uint32_t b_idx = val(buffer_[i]);
                if (b_idx > idx) [[likely]] {
                    buffer_[i]--;
                } else if (b_idx == idx) {
                    v = assoc_val(buffer_[i]);
                    if (i < buffer_elems_ - 1) {
                        std::memmove(buffer_ + i, buffer_ + i + 1,
                                     buffer_elems_ - i - 1);
                    }
                    buffer_elems_--;
                    return true;
                } else {
                    idx--;
                }
                i--;
            }
            return false;
        }
    }

    bool set(uint32_t& idx, bool v, int& diff) {
        uint32_t o_idx = idx;
        if constexpr (sorted && !compressed) {
            for (uint16_t i = 0; i < buffer_elems_; i++) {
                uint32_t b_idx = val(buffer_[i]);
                if (b_idx < o_idx) [[likely]] {
                    idx += is_insertion(buffer_[i]) ? -1 : 1;
                } else if (b_idx == o_idx) {
                    if (is_insertion(buffer_[i])) {
                        if (assoc_val(buffer_[i])) {
                            diff = v ? 0 : -1;
                        } else {
                            diff = v ? 1 : 0;
                        }
                        return true;
                    } else {
                        idx++;
                    }
                } else {
                    break;
                }
            }
            return false;
        } else {
            if constexpr (!sorted) {
                sort();
            }
            for (uint16_t i = 0; i < buffer_elems_; i++) {
                uint32_t b_idx = val(buffer_[i]);
                if (b_idx < o_idx) [[likely]] {
                    idx--;
                } else if (b_idx == o_idx) {
                    if (assoc_val(buffer_[i])) {
                        diff = v ? 0 : -1;
                    } else {
                        diff = v ? 1 : 0;
                    }
                    return true;
                } else {
                    break;
                }
            }
            return false;
        }
    }

    void clear() { buffer_elems_ = 0; }

    const BufferElement& operator[](uint_t i) const {
        return buffer_[i];
    }

    uint16_t size() const { return buffer_elems_; }

    buffer_iter begin() {
        if constexpr (!sorted) {
            sort();
        }
        return buffer_iter(this, 0);
    }

    buffer_iter end() const { return buffer_iter(this, buffer_elems_); }

   private:
    template <uint16_t block_size>
    void sort(BufferElement source[], BufferElement target[]) {
        if constexpr (block_size <= 64) {
            for (uint64_t i = 0; i < buffer_size; i += block_size) {
                bf_sort<block_size>(target + i);
            }
            return;
        } else if constexpr (block_size == 128 &&
                             ((__builtin_clz(buffer_size / 16) & uint16_t(1)) == 0)) {
            for (uint64_t i = 0; i < buffer_size; i += block_size) {
                bf_sort<block_size>(target + i);
            }
            return;
        } else {
            const constexpr uint16_t sub_block_size = block_size / 2;
            sort<sub_block_size>(target, source);
            for (uint16_t offset = 0; offset < buffer_size;
                 offset += block_size) {
                uint16_t inversions = 0;
                uint16_t a_idx = offset;
                uint16_t b_idx = offset + sub_block_size;
                uint16_t a_limit = b_idx;
                uint16_t b_limit = offset + block_size;
                for (uint16_t i = 0; i < block_size; i++) {
                    if (a_idx < a_limit) [[likely]] {
                        if (b_idx < b_limit) [[likely]] {
                            BufferElement be = source[a_idx] + inversions;
                            if (be < source[b_idx]) {
                                target[offset + i] = be;
                            } else {
                                target[offset + i] = source[b_idx++];
                                inversions++;
                            }
                        } else {
                            target[offset + i] = source[a_idx++] + inversions;
                        }
                    } else {
                        target[offset + i] = source[b_idx++];
                    }
                }
            }
        }
    }

    template <uint16_t size>
    void bf_sort(BufferElement source[]) {
#pragma GCC unroll 1024
        for (uint16_t i = 1; i < size; i++) {
            for (uint16_t j = 0; j < i; j++) {
                source[j] += source[i] <= source[j];
            }
        }
        std::sort(source, source + size);
    }
};
}  // namespace bv
#endif