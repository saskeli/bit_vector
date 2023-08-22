#pragma once

#include <cstdint>
#include <bitset>

#include "uncopyable.hpp"

namespace bv
{
    
template <uint16_t size>
class Circular_Buffer : uncopyable {
   private:
    static const constexpr uint8_t WORD_BITS = 64;
    static const constexpr uint64_t ONE = 1;
    static const constexpr uint64_t UNITY = ~uint64_t(0);
    uint16_t start_;
    uint16_t end_;
    uint16_t offset_;
    uint64_t* data_;
    uint32_t size_;

   public:
    Circular_Buffer(uint64_t* data) : start_(), end_(), offset_(), data_(data), size_() {}

    void push_back(uint64_t v, uint16_t elems) {
        v &= elems < 64 ? (ONE << elems) - 1: UNITY;
        if (offset_) {
            data_[end_] |= v << offset_;
            uint16_t space_left = WORD_BITS - offset_;
            if (elems > space_left) {
                ++end_;
                end_ %= size;
                data_[end_] = v >> space_left;
            } else if (elems == space_left) {
                ++end_;
                end_ %= size;
            }
        } else {
            data_[end_] = v;
            end_ += elems == WORD_BITS;
            end_ %= size;
        }
        offset_ += elems;
        offset_ %= WORD_BITS;
        size_ += elems;
    }

    uint64_t poll() {
        uint64_t ret = data_[start_++];
        start_ %= size;
        size_ -= WORD_BITS;
        return ret;
    }

    uint32_t space() {
        return size * WORD_BITS - size_;
    }

    std::ostream& print() {
        std::cout << "{\n"
                  << "\"capacity\": " << size << ",\n"
                  << "\"size\": " << size_ << ",\n"
                  << "\"start\": " << start_ << ",\n"
                  << "\"end\": " << end_ << ",\n"
                  << "\"offset\": " << offset_ << ",\n"
                  << "\"data\": [\n";
        for (size_t i = 0; i < size; i++) {
            std::cout << "\"" << std::bitset<64>(data_[i]) << "\"";
            if (i == size - 1) {
                std::cout << "\n]}";
            } else {
                std::cout << ",\n";
            }
        }
        return std::cout;
    }
};

} // namespace bv