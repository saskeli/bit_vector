#ifndef BV_BUF_HPP
#define BV_BUF_HPP

#include <cstdint>
#include <cstring>
#include <utility>

#include "uncopyable.hpp"

namespace bv {

template <uint8_t buffer_size, bool compressed, bool sorted>
class buffer {
   private:
    static_assert(buffer_size > 0);
    static_assert(buffer_size < 1024);
    static_assert(__builtin_popcount(buffer_size) == 1);
    inline static uint32_t scratch[buffer_size];

    uint32_t buffer_[buffer_size];
    uint16_t buffer_elems_;

    struct buffer_ref {
        const uint32_t index;
        const bool is_insertion;
        const bool value;
    };
    class buffer_iter {
       private:
        buffer* buf;
        uint8_t offset;

       public:
        buffer_iter(buffer* buf_ref, uint8_t pos) {
            buf = buf_ref;
            offset = pos;
        }

        bool operator==(const buffer_iter& rhs) const {
            return (buf == rhs.buf) && (offset == rhs.offset);
        }

        bool operator!=(const buffer_iter& rhs) const {
            return !operator==(rhs);
        }

        const buffer_ref operator*() const { return buf->operator[](offset); }

        buffer_iter& operator++() {
            offset++;
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
        const constexpr uint32_t max_val = ~uint32_t(0);
        for (uint8_t i = 0; i < buffer_size - buffer_elems_; i++) {
            buffer_[buffer_size - 1 - i] = max_val - i;
        }
        if constexpr (buffer_size == 2) {
            return twosort(buffer_);
        }
        if constexpr (buffer_size == 4) {
            return foursort(buffer_);
        }
        sort<buffer_size>(scratch, buffer_);
    }

    bool is_full() const { return buffer_elems_ == buffer_size; }

    void insert(bool v, uint32_t idx) {
        if constexpr (!sorted) {
            buffer_[buffer_elems_++] = idx | (uint32_t(v) << 31);
            return;
        }
        uint8_t i = buffer_elems_;
        while (i > 0) {
            uint32_t b = val(buffer_[i - 1]);
            if (b > idx || (b == idx && is_insertion(buffer_[i - 1]))) {
                [[likely]] buffer_[i] = buffer_[i - 1] + 1;
            } else {
                break;
            }
            i--;
        }
        buffer_[i] = idx | (uint32_t(v) << 31);
    }

    bool remove(uint32_t& idx, bool& v) {
        if constexpr (sorted && !compressed) {
            uint8_t i = buffer_elems_;
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
            for (uint8_t i = buffer_elems_ - 1; i < buffer_elems_; i--) {
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
            for (uint8_t i = 0; i < buffer_elems_; i++) {
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
            for (uint8_t i = 0; i < buffer_elems_; i++) {
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

    buffer_ref operator[](uint8_t i) const {
        return {val(buffer_[i]), is_insertion(buffer_[i]),
                assoc_val(buffer_[i])};
    }

    uint8_t size() { return buffer_elems_; }

    buffer_iter begin() {
        if constexpr (!sorted) {
            sort();
        }
        return buffer_iter(this, 0);
    }

    buffer_iter end() { return buffer_iter(this, buffer_elems_); }

   private:
    inline constexpr uint32_t val(uint32_t be) const {
        if constexpr (!sorted || compressed) {
            const constexpr uint32_t mask = (~uint32_t(0)) >> 1;
            return be & mask;
        } else {
            const constexpr uint32_t mask = (~uint32_t(0)) >> 2;
            return be & mask;
        }
    }

    inline constexpr bool is_insertion(uint32_t be) const {
        if constexpr (!sorted || compressed) {
            return true;
        }
        const constexpr uint32_t r_mask = uint32_t(1) << 30;
        return (be & r_mask) == 0;
    }

    inline constexpr bool assoc_val(uint32_t be) const {
        const constexpr uint32_t v_mask = uint32_t(1) << 31;
        return (be & v_mask) == 0;
    }

    void twosort(uint32_t arr[]) {
        for (uint8_t i = 0; i < buffer_size; i += 2) {
            if (val(arr[i + 1]) <= val(arr[i])) {
                arr[i]++;
                std::swap(arr[i], arr[i + 1]);
            }
        }
    }

    void foursort(uint32_t arr[]) {
        for (uint8_t i = 0; i < buffer_size; i += 4) {
            arr[i] += val(arr[i + 1]) <= val(arr[i]);

            arr[i + 1] += val(arr[i + 2]) <= val(arr[i + 1]);
            arr[i] += val(arr[i + 2]) <= val(arr[i]);

            arr[i + 2] += val(arr[i + 3]) <= val(arr[i + 2]);
            arr[i + 1] += val(arr[i + 3]) <= val(arr[i + 1]);
            arr[i] += val(arr[i + 3]) <= val(arr[i]);

            if (val(arr[i + 2]) > val(arr[i + 3])) {
                std::swap(arr[i + 2], arr[i + 3]);
            }
            if (val(arr[i]) > val(arr[i + 1])) {
                std::swap(arr[i], arr[i + 1]);
            }
            if (val(arr[i]) > val(arr[i + 2])) {
                std::swap(arr[i], arr[i + 2]);
            }
            if (val(arr[i + 1]) > val(arr[i + 3])) {
                std::swap(arr[i + 1], arr[i + 3]);
            }
            if (val(arr[i + 1]) > val(arr[i + 2])) {
                std::swap(arr[i + 1], arr[i + 2]);
            }
        }
    }

    template <uint8_t block_size>
    void sort(uint32_t source[], uint32_t target[]) {
        if constexpr (block_size == 2) {
            twosort(target);
            return;
        } else if constexpr (block_size == 4 &&
                             __builtin_clz(buffer_size) & uint16_t(1)) {
            foursort(target);
            return;
        } else {
            const constexpr uint8_t sub_block_size = block_size / 2;
            sort<sub_block_size>(target, source);
            for (uint8_t offset = 0; offset < buffer_size;
                 offset += block_size) {
                uint8_t inversions = 0;
                uint8_t a_idx = offset;
                uint8_t b_idx = offset + sub_block_size;
                uint8_t a_limit = b_idx;
                uint8_t b_limit = offset + block_size;
                for (uint8_t i = 0; i < block_size; i++) {
                    if (a_idx < a_limit) [[likely]] {
                        if (b_idx < b_limit) [[likely]] {
                            if (val(source[a_idx]) + inversions <
                                val(source[b_idx])) {
                                target[offset + i] = source[a_idx++];
                                target[offset + i] += inversions;
                            } else {
                                target[offset + i] = source[b_idx++];
                                inversions++;
                            }
                        } else {
                            target[offset + i] = source[a_idx++];
                            target[offset + i] += inversions;
                        }
                    } else {
                        target[offset + i] = source[b_idx++];
                    }
                }
            }
        }
    }
};
}  // namespace bv
#endif