#ifndef BV_PACKED_ARRAY_HPP
#define BV_PACKED_ARRAY_HPP

#include <cstdint>

namespace bv {

template<uint16_t elems, uint8_t width>
class packed_array {
  private:
    class array_ref {
      private:    
        packed_array* _arr;
        uint32_t _idx;

      public:
        array_ref(packed_array* arr, uint32_t idx) : _arr(arr), _idx(idx) {}

        array_ref(const array_ref& other) : _arr(other._arr), _idx(other._idx) {}

        array_ref& operator=(const array_ref& other) {
            _arr = other._arr;
            _idx = other._idx;
            return *this;
        }

        operator uint16_t() const {
            return _arr->at(_idx);
        }

        array_ref const& operator=(uint16_t val) const {
            _arr->set(_idx, val);
            return *this;
        }
    };

    static const constexpr uint32_t WORD_BITS = 32;
    static const constexpr uint32_t MASK = (uint32_t(1) << width) - 1;

    uint32_t _data[elems * width / WORD_BITS];

    // Setting blocks or block bits to 0 is effectively the same as using an
    // unbuffered leaf.
    static_assert(elems > 0);
    static_assert(width > 0);

    // Meta-data is packed into 32-bit integers. Space is wasted if blocks *
    // block_bits % 32 != 0.
    static_assert(elems * width % WORD_BITS == 0);

  public:
    packed_array() : _data(0) {}

    uint16_t at(uint16_t index) const {
        uint32_t offset = width * index;
        uint32_t word = offset / WORD_BITS;
        offset %= WORD_BITS;
        uint32_t res = (_data[word] >> offset) & MASK;

        if (offset + width > WORD_BITS) {
            res |= (_data[word + 1] & (MASK >> (WORD_BITS - offset)))
                   << (WORD_BITS - offset);
            [[unlikely]] (void(0));
        }
        return res & MASK;
    }

    void set(uint16_t index, uint16_t value) {
        uint32_t offset = width * index;
        uint32_t word = offset / WORD_BITS;
        offset %= WORD_BITS;
        _data[word] |= value << offset;

        if (width + offset > WORD_BITS) {
            _data[word + 1] |= value >> (WORD_BITS - offset);
            [[unlikely]] (void(0));
        }
    }

    array_ref operator[](uint32_t index) {
        return {this, index};
    }

};
} // namespace BV


#endif