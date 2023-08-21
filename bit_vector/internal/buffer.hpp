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

        BufferElement& operator-=(uint32_t i) {
            v_ -= i << OFFSET;
            return *this;
        }

        BufferElement operator+(uint32_t i) {
            return v_ + (i << OFFSET);
        } 

        BufferElement& operator=(const BufferElement& rhs) {
            v_ = rhs.v_;
            return *this;
        }

        bool operator<(const BufferElement& rhs) const {
            return (v_ >> ONE) < (rhs.v_ >> ONE);
        }

        bool operator<=(const BufferElement& rhs) const {
            return (v_ >> ONE) <= (rhs.v_ >> ONE);
        }

        void set_remove_value(bool v) {
            v_ |= uint32_t(v);
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
    static const constexpr uint16_t SCRATCH_ELEMS = buffer_size >= 6 ? buffer_size / 2 : 3; 
    inline static BufferElement scratch[SCRATCH_ELEMS * 2];

    BufferElement buffer_[buffer_size];
    uint16_t buffer_elems_;

   public:
    buffer() : buffer_(), buffer_elems_() {}
    buffer(const buffer&) = delete;
    buffer& operator=(const buffer&) = delete;
/*#pragma GCC diagnostic ignored "-Wclass-memaccess"
    buffer(const buffer& other) { std::memcpy(this, &other, sizeof(buffer)); }
    buffer& operator=(const buffer& other) {
        std::memcpy(this, &other, sizeof(buffer));
    }
#pragma GCC diagnostic pop*/

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

    bool access(uint32_t& idx, bool& v) const {
        if constexpr (sorted && !compressed) {
            uint32_t o_idx = idx;
            for (uint16_t i = 0; i < buffer_elems_; ++i) {
                uint32_t b_idx = buffer_[i].index();
                if (b_idx == o_idx) {
                    if (buffer_[i].is_insertion()) {
                        v = buffer_[i].value();
                        return true;
                    } else {
                        idx++;
                    }
                } else if (b_idx < o_idx) [[likely]] {
                    idx += buffer_[i].is_insertion() ? -1 : 1;
                } else {
                    break;
                }
            }   
            return false;
        } else if constexpr (sorted) {
            uint32_t o_idx = idx;
            for (uint16_t i = 0; i < buffer_elems_; ++i) {
                uint32_t b_idx = buffer_[i].index();
                if (b_idx == o_idx) {
                    v = buffer_[i].value();
                    return true;
                } else if (b_idx < o_idx) {
                    --idx;
                } else {
                    break;
                }
            }
            return false;
        } else {
            for (uint16_t i = buffer_elems_ - 1; i < buffer_elems_; --i) {
                uint32_t b_idx = buffer_[i].index();
                if (b_idx == idx) {
                    v = buffer_[i].value();
                    return true;
                } else if (b_idx < idx) {
                    --idx;
                }
            }
            return false;
        }
    }

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

    uint16_t remove(uint32_t& idx, bool& v) {
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
                        return buffer_size;
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
            uint16_t index = i;
            buffer_[i--] = {idx, false, false};
            buffer_elems_++;
            for (; i < buffer_elems_; i--) {
                idx += buffer_[i].is_insertion() ? -1 : 1;
            }
            return index;
        } else if constexpr (!sorted) {
            uint16_t done = 0;
            for (uint16_t i = buffer_elems_ - 1; i < buffer_elems_; i--) {
                uint32_t b_idx = buffer_[i].index();
                if (b_idx == idx && !done) [[unlikely]] {
                    done = buffer_size;
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
                    return buffer_size;
                } else {
                    idx--;
                }
            }
            return 0;
        }
    }

    void set_remove_value(uint16_t idx, bool v) {
        buffer_[idx].set_remove_value(v);
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

    void set_insert(uint32_t idx, bool v) {
        static_assert(compressed);
        if constexpr (sorted) {
            uint16_t b_e = buffer_elems_ - 1;
            while (b_e < buffer_elems_ && buffer_[b_e].index() > idx) {
                buffer_[b_e + 1] = buffer_[b_e];
                b_e--;
            }
            ++b_e;
            buffer_[b_e] = {idx, v};
            buffer_elems_++;
        } else {
            buffer_[buffer_elems_] = {idx, v};
            for (uint16_t i = buffer_elems_ - 1; i < buffer_elems_; --i) {
                if (buffer_[i].index() > idx) {
                    --buffer_[i];
                } else {
                    --idx;
                }
            }
            buffer_elems_++;
        }
    }
    
    uint32_t rank(uint32_t& idx) const {
        uint32_t ret = 0;
        uint32_t o_idx = idx;
        if constexpr (sorted && !compressed) {
            for (uint16_t i = 0; i < buffer_elems_; ++i) {
                if (buffer_[i].index() < o_idx) [[likely]] {
                    if (buffer_[i].is_insertion()) {
                        --idx;
                        ret += buffer_[i].value();
                    } else {
                        ++idx;
                        ret -= buffer_[i].value();
                    }
                } else {
                    break;
                }
            }
            return ret;
        } else if constexpr (!sorted) {
            for (uint16_t i = buffer_elems_ - 1; i < buffer_elems_; --i) {
                if (buffer_[i].index() < idx) {
                    --idx;
                    ret += buffer_[i].value();
                }
            }
            return ret;
        } else {
            for (uint16_t i = 0; i < buffer_elems_; ++i) {
                if (buffer_[i].index() < o_idx) {
                    --idx;
                    ret += buffer_[i].value();
                } else {
                    break;
                }
            }
            return ret;
        }
    }

    uint32_t clear_first(uint32_t& elems) {
        static_assert(sorted && compressed);
        uint16_t i = 0; 
        uint32_t d_sum = 0;
        uint32_t lim = elems;
        while (i < buffer_elems_ && buffer_[i].index() < lim) {
            --elems;
            d_sum += buffer_[i++].value();
        }
        uint16_t t_idx = 0;
        for (; i < buffer_elems_; i++) {
            buffer_[t_idx] = buffer_[i];
            buffer_[t_idx++] -= lim;
        }
        buffer_elems_ = t_idx;
        return d_sum;
    }

    uint32_t clear_last(uint32_t& keep) {
        static_assert(sorted && compressed);
        uint32_t l_keep = keep;
        uint32_t p_sum = 0;
        uint16_t n_count = 0;
        for (auto be : *this) {
            if (be.index() < l_keep) {
                p_sum += be.value();
                --keep;
                ++n_count;
            } else {
                break;
            }
        }
        buffer_elems_ = n_count;
        return p_sum;
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

    void append(BufferElement elem) {
        buffer_[buffer_elems_++] = elem;
    }

    static uint16_t max_elems() {
        return buffer_size;
    }

    static uint64_t* get_scratch() {
        return reinterpret_cast<uint64_t*>(scratch);
    }

    static constexpr uint16_t scratch_elem_count() {
        return SCRATCH_ELEMS;
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
                                ++a_idx;
                            } else {
                                target[offset + i] = source[b_idx++];
                                ++inversions;
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