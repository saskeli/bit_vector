#ifndef BV_PACKED_ARRAY_HPP
#define BV_PACKED_ARRAY_HPP

#include <cstdint>

namespace bv {

template <uint16_t elems, uint8_t width>
class packed_array {
   private:
    class array_ref {
       private:
        packed_array* arr_;
        uint32_t idx_;
        uint32_t val_;

       public:
        array_ref(packed_array* arr, uint32_t idx) : arr_(arr), idx_(idx) {
            val_ = arr_->at(idx_);
        }

        array_ref(const array_ref& other)
            : arr_(other.arr_), idx_(other.idx_), val_(other.val_) {
        }

        array_ref& operator=(const array_ref& other) {
            val_ = other.val_;
            arr_->set(idx_, val_);
            return *this;
        }

        operator uint16_t() const { return val_; }

        array_ref const& operator=(uint16_t val) {
            val_ = val;
            arr_->set(idx_, val_);
            return *this;
        }

        array_ref const& operator++(int) {
            val_++;
            arr_->set(idx_, val_);
            return *this;
        }

        array_ref const& operator--(int) {
            val_--;
            arr_->set(idx_, val_);
            return *this;
        }

        array_ref const& operator+=(uint16_t val) {
            val_ += val;
            arr_->set(idx_, val_);
            return *this;
        }

        array_ref const& operator-=(uint16_t val) {
            val_ -= val;
            arr_->set(idx_, val_);
            return *this;
        }
    };

    static const constexpr uint32_t WORD_BITS = 32;
    static const constexpr uint32_t MASK = (uint32_t(1) << width) - 1;

    uint32_t data_[elems * width / WORD_BITS + ((elems * width) % WORD_BITS != 0 ? 1 : 0)];

    static_assert(elems > 0 && width > 0,
                  "Setting blocks or block bits to 0 is effectively the same "
                  "as using an unbuffered leaf.");

   public:
    packed_array() : data_(0) {}

    uint16_t at(uint16_t index) const {
        uint32_t offset = width * index;
        uint32_t word = offset / WORD_BITS;
        offset %= WORD_BITS;
        uint32_t res = (data_[word] >> offset) & MASK;

        if (offset + width > WORD_BITS) [[unlikely]] {
            res |= (data_[word + 1] & (MASK >> (WORD_BITS - offset)))
                   << (WORD_BITS - offset);
        }
        return res & MASK;
    }

    void set(uint16_t index, uint32_t value) {
        uint32_t offset = width * index;
        uint32_t word = offset / WORD_BITS;
        offset %= WORD_BITS;
        data_[word] &= ~(MASK << offset);
        data_[word++] |= value << offset;

        if (width + offset > WORD_BITS) [[unlikely]] {
            data_[word] &= ~(MASK >> (WORD_BITS - offset));
            data_[word] |= value >> (WORD_BITS - offset);
        }
    }

    void clear() {
        memset(data_, 0, elems * width / 8);
    }

    array_ref operator[](uint32_t index) { return {this, index}; }

    uint16_t operator[](uint32_t index) const {return at(index); }
};
}  // namespace bv

#endif