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

template <uint16_t buffer_size, bool compressed, bool sorted, uint16_t bf_limit = 16>
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
        sort<buffer_size, true>(buffer_, scratch);
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

    std::ostream& print(bool use_scratch = false, std::ostream& out = std::cout) {
        BufferElement* buf = use_scratch ? scratch : buffer_;
        out << "{\n"
            << "\"buffer size\": " << buffer_size << ",\n"
            << "\"buffer elems\": " << buffer_elems_ << ",\n"
            << "\"compressed\": " << compressed << ",\n"
            << "\"sorted\": " << sorted << ",\n"
            << "\"elements\": [";
        for (uint16_t i = 0; i < buffer_elems_; i++) {
            out << (i == 0 ? "\n" : ",\n")
                << "{\"index\": " << buf[i].index() << ", "
                << "\"value\": " << buf[i].value() << ", "
                << "\"is_insertion\": " << buf[i].is_insertion() << "}";
        }
        out << "]}";
        return out;
     }

   private:
    template<uint16_t init, uint16_t step, uint16_t size>
    constexpr std::array<uint16_t, size> init_arr() {
        std::array<uint16_t, size> ret;
        for (uint16_t i = 0; i < size; ++i) {
            ret[i] = i * step + init;
        }
        return ret;
    }

    template <uint16_t block_size, bool in_target>
    void sort(BufferElement target[], BufferElement source[]) {
        if constexpr (block_size <= bf_limit && in_target) {
            for (uint64_t i = 0; i < buffer_size; i += block_size) {
                bf_sort<block_size>(buffer_ + i);
            }
        } else {
            const constexpr uint16_t sub_block_size = block_size / 2;
            const constexpr uint16_t blocks = buffer_size / block_size;
            sort<sub_block_size, !in_target>(source, target);
            auto a_s(init_arr<0, block_size, blocks>());
            auto a_e(init_arr<sub_block_size - 1, block_size, blocks>());
            auto b_s(init_arr<sub_block_size, block_size, blocks>());
            auto b_e(init_arr<block_size - 1, block_size, blocks>());
            auto s_inv(init_arr<0, 0, blocks>());
            auto e_inv(init_arr<sub_block_size, 0, blocks>());

            for (uint16_t i = 0; i < block_size / 2; ++i) {
                for (uint16_t j = 0; j < blocks; ++j) {
                    if (a_s[j] <= a_e[j]) [[likely]] {
                        auto pot = source[a_s[j]] + s_inv[j];
                        if (b_s[j] <= b_e[j]) [[likely]] {
                            if (pot < source[b_s[j]]) {
                                target[j * block_size + i] = pot;
                                ++a_s[j];
                            } else {
                                target[j * block_size + i] = source[b_s[j]++];
                                ++s_inv[j];
                            }
                        } else {
                            target[j * block_size + i] = pot;
                            ++a_s[j];
                        }
                    } else {
                        target[j * block_size + i] = source[b_s[j]++];
                    }

                    if (a_e[j] >= a_s[j]) [[likely]] {
                        auto pot = source[a_e[j]] + e_inv[j];
                        if (b_e[j] >= b_s[j]) [[likely]] {
                            if (source[b_e[j]] <= pot) {
                                target[j * block_size + block_size - i - 1] = pot;
                                --a_e[j];
                            } else {
                                target[j * block_size + block_size - i - 1] = source[b_e[j]--];
                                --e_inv[j];
                            }
                        } else {
                            target[j * block_size + block_size - i - 1] = pot;
                            --a_e[j];
                        }
                    } else {
                        target[j * block_size + block_size - i - 1] = source[b_e[j]--];
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