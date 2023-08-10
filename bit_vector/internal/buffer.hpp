#ifndef BV_BUF_HPP
#define BV_BUF_HPP

#include <cstdint>
#include <cstring>
#include <utility>
#include <algorithm>
#include <iostream>
#include <ranges>

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
            : v_((idx << OFFSET) | uint32_t(v) | (uint32_t(t) << ONE)) {
            static_assert(!compressed && sorted);
        }

        BufferElement& operator++() {
            v_ += uint32_t(ONE) << OFFSET;
            return *this;
        }

        BufferElement& operator--() {
            v_ -= uint32_t(ONE) << OFFSET;
            return *this;
        }

        BufferElement& operator+=(uint32_t i) {
            v_ += i << OFFSET;
            return *this;
        }

        BufferElement operator+(uint32_t i) {
            return v_ + (i << OFFSET);
        } 

        BufferElement& operator=(const BufferElement& rhs) {
            v_ = rhs.v_;
            return *this;
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

        uint32_t index() const { return v_ >> OFFSET; }

        bool value() const { return v_ & ONE; }

        bool is_insertion() const {
            if constexpr (compressed || !sorted) {
                return true;
            }
            return bool(v_ & OFFSET);
        }

        static BufferElement max() {
            return {(~uint32_t(0)) >> 1};
        }
    };

    static_assert(buffer_size <= ((1 << 16) - 1));
    static_assert(__builtin_popcount(buffer_size) == 1);
    inline static BufferElement scratch[buffer_size];

    BufferElement buffer_[buffer_size];
    uint16_t buffer_elems_;

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
        for (uint16_t i = buffer_elems_; i < buffer_size; i++) [[unlikely]] {
            buffer_[i] = BufferElement::max();
        }
        sort<buffer_size>(scratch, buffer_);
    }

    bool is_full() const { return buffer_elems_ == buffer_size; }

    void insert(uint32_t idx, bool v) {
        BufferElement nb = [&] () -> BufferElement {
            if constexpr (!compressed && sorted) {
                return {idx, v, true};
            } else {
                return {idx, v};
            }
        }();
        if constexpr (!sorted) {
            buffer_[buffer_elems_++] = nb;
            return;
        }
        uint16_t i = buffer_elems_;
        while (i > 0) {
            if (nb <= buffer_[i - 1]) [[likely]] {
                buffer_[i] = buffer_[i - 1];
                ++buffer_[i];
            } else {
                break;
            }
            --i;
        }
        buffer_[i] = nb;
        ++buffer_elems_;
    }

    bool remove(uint32_t& idx, bool& v) {
        if constexpr (sorted && !compressed) {
            uint16_t i = buffer_elems_ - 1;
            for (; i < buffer_elems_; --i) {
                uint32_t b_idx = buffer_[i].index();
                if (b_idx > idx) [[likely]] {
                    --buffer_[i];
                } else if (b_idx == idx) {
                    if (buffer_[i].is_insertion()) {
                        v = buffer_[i].value();
#pragma GCC diagnostic ignored "-Wclass-memaccess"
                        std::memmove(buffer_ + i, buffer_ + (i + 1),
                                     sizeof(BufferElement) * (buffer_elems_ - i - 1));
#pragma GCC diagnostic pop
                        buffer_elems_--;
                        return true;
                    } else {
                        break;
                    }
                } else {
                    break;
                }
            }
            ++i;
            if (i < buffer_elems_) {
#pragma GCC diagnostic ignored "-Wclass-memaccess"
                std::memmove(buffer_ + (i + 1), buffer_ + i, sizeof(BufferElement) * (buffer_elems_ - i));
#pragma GCC diagnostic pop
            }
            buffer_[i--] = {idx, false, false};
            buffer_elems_++;
            for (; i < buffer_elems_; i--) {
                idx += buffer_[i].is_insertion() ? -1 : 1;
            }
            return false;
        } else if constexpr (!sorted) {
            bool done = false;
            for (uint16_t i = buffer_elems_ - 1; i < buffer_elems_; i--) {
                uint32_t b_idx = buffer_[i].index();
                if (b_idx == idx && !done) [[unlikely]] {
                    done = true;
                    v = buffer_[i].value();
#pragma GCC diagnostic ignored "-Wclass-memaccess"
                    std::memmove(buffer_ + i, buffer_ + i + 1,
                                 sizeof(BufferElement) * (buffer_elems_ - i - 1));
#pragma GCC diagnostic pop
                    buffer_elems_--;
                    
                } else if (b_idx > idx) {
                    --buffer_[i];
                } else {
                    idx--;
                }
            }
            return done;
        } else {
            for (uint16_t i = buffer_elems_ - 1; i < buffer_elems_; i--) {
                uint32_t b_idx = buffer_[i].index();
                if (b_idx > idx) [[likely]] {
                    --buffer_[i];
                } else if (b_idx == idx) {
                    v = buffer_[i].value();
                    if (i < buffer_elems_ - 1) {
#pragma GCC diagnostic ignored "-Wclass-memaccess"
                        std::memmove(buffer_ + i, buffer_ + i + 1,
                                     sizeof(BufferElement) * (buffer_elems_ - i - 1));
#pragma GCC diagnostic pop
                    }
                    buffer_elems_--;
                    return true;
                } else {
                    idx--;
                }
            }
            return false;
        }
    }

    bool set(uint32_t& idx, bool v, int& diff) {
        uint32_t o_idx = idx;
        if constexpr (sorted && !compressed) {
            for (uint16_t i = 0; i < buffer_elems_; i++) {
                uint32_t b_idx = buffer_[i].index();
                if (b_idx < o_idx) [[likely]] {
                    idx += buffer_[i].is_insertion() ? -1 : 1;
                } else if (b_idx == o_idx) {
                    if (buffer_[i].is_insertion()) {
                        diff = v;
                        diff -= int(buffer_[i].value());
                        buffer_[i] = {b_idx, v, true};
                        return true;
                    } else {
                        idx++;
                    }
                } else {
                    break;
                }
            }
            return false;
        } else if constexpr(!sorted) {
            for (uint16_t i = buffer_elems_ - 1; i < buffer_elems_; i--) {
                if (buffer_[i].index() == idx) [[unlikely]] {
                    diff = v;
                    diff -= int(buffer_[i].value());
                    buffer_[i] = {idx, v};
                    return true;
                }
                idx -= buffer_[i].index() < idx ? 1 : 0;
            }
            return false;
        } else {
            for (uint16_t i = 0; i < buffer_elems_; i++) {
                uint32_t b_idx = buffer_[i].index();
                if (b_idx < o_idx) [[likely]] {
                    idx--;
                } else if (b_idx == o_idx) {
                    diff = v;
                    diff -= int(buffer_[i].value());
                    buffer_[i] = {o_idx, v};
                    return true;
                } else {
                    break;
                }
            }
            return false;
        }
    }

    void clear() { buffer_elems_ = 0; }

    const BufferElement& operator[](uint16_t i) const {
        return buffer_[i];
    }

    uint16_t size() const { return buffer_elems_; }

    const BufferElement* begin() const {
        return buffer_;
    }

    const BufferElement* end() const { 
        return buffer_ + buffer_elems_;
    }

    static uint16_t max_elems() {
        return buffer_size;
    }

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