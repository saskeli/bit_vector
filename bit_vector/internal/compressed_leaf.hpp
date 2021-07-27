#include <cstdint>

#include "leaf.hpp"
#include "libpopcnt.h"

namespace bv {

template <class dtype, uint8_t buffer_size, bool avx = true>
class compressed_leaf : private leaf<buffer_size, avx> {
   protected:
    uint64_t leaf_type_;

    // A 64-bit dtype implies that there are an even number of elements in the
    // buffer. Since the buffer elements are 32 bits wide, storing 64-bit
    // indexes requires 2 elements.
    static_assert(sizeof(dtype) == sizeof(uint32_t) || buffer_size % 2 == 0);
    // sizeof(dtype) = 8 => buffer_size <= 40, since each 64-bit element
    // requires a total of 3 status bits and there are a total of 60 bits
    // available.
    static_assert(sizeof(dtype) == sizeof(uint32_t) || buffer_size <= 40);
    // sizeof(dtype) = 4 => buffer_size <= 20, since each 32-bit element
    // requires a total of 3 status bits and there are a total of 60 bits
    // available.
    static_assert(sizeof(dtype) == sizeof(uint64_t) || buffer_size <= 20);
    // Only 31 and 63 bit universe sizes are supported at the internal node
    // level so only the associated sizes are supported at the leaf level as
    // well.
    static_assert(sizeof(dtype) == sizeof(uint64_t) ||
                  sizeof(dtype) == sizeof(uint32_t));

   public:
    compressed_leaf(uint16_t capacity, uint64_t* data) : leaf(capacity, data) {
        leaf_type_ = 0;
    }

    compressed_leaf(uint16_t capacity, uint64_t* data, dtype size)
        : leaf(capacity, data) {
        leaf_type_ = 0;
        size_ = size;
    }

    bool at(dtype i) {
        switch (leaf_type_) {
            case 0:
                return access_one_ints(i);
            default:
                return ((leaf*)this)->(i);
        }
    }

   protected:
    bool access_one_ints(dtype i) {
        constexpr dtype mask = ((~dtype(0)) >> 1);
        dtype* buffer = static_cast<dtype*>(buffer_);
        for (uint8_t idx = 0; idx < buffer_count_; idx++) {
            if (buffer[idx] & mask == i) {
                return (buffer[idx] & (~mask)) >> (sizeof(dtype) * 8 - 1);
            }
        }
    }
};

}  // namespace bv