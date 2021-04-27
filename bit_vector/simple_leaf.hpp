#ifndef BV_SIMPLE_LEAF_HPP
#define BV_SIMPLE_LEAF_HPP

#include <cstdint>
#include <immintrin.h>
#include <iostream>

#define WORD_BITS 64

template <class data_type, class allocator_type, uint8_t buffer_size>
class simple_leaf {
  private:
    uint8_t metadata_;
    uint32_t buffer_[buffer_size + 1];
    data_type capacity_;
    data_type size_;
    data_type p_sum_; //Should this just be recalculated when needed? Currently causes branching.
    uint64_t* data_;
    allocator_type* allocator_;
    static constexpr uint64_t MASK = 1;
    static constexpr uint32_t VALUE_MASK = 1;
    static constexpr uint32_t TYPE_MASK = 8;
    static constexpr uint32_t INDEX_MASK = ~((uint32_t(1) << 8) - 1);
    static_assert(buffer_size < 64);

  public:
    simple_leaf(allocator_type* allocator, data_type capacity, uint64_t* data) {
        allocator_ = allocator;
        metadata_ = 0;
        capacity_ = capacity;
        size_ = 0;
        p_sum_ = 0;
        data_ = data;
    }

    ~simple_leaf() {
        allocator_.deallocate_leaf(this);
    }

    bool at(data_type i) const {
        if constexpr (buffer_size != 0) {
            data_type index = i;
            for (uint8_t idx = 0; idx < buffer_count(); idx++) {
                data_type b = buffer_index(buffer_[idx]);
                if (b == i) {
                    if (buffer_is_insertion(buffer_[idx])) {
                        return buffer_value(buffer_[idx]);
                    }
                    index++;
                } else if (b < i) {
                    index -= buffer_is_insertion(buffer_[idx]) * 2 - 1;
                } else {
                    break;
                }
            }
            return MASK & (data_[index / WORD_BITS] >> (index % WORD_BITS));
        }
        return MASK & (data_[i / WORD_BITS] >> (i % WORD_BITS));
    }

    uint64_t p_sum() const { return p_sum_; }
    uint64_t size() const { return size_; }

    void insert(uint64_t i, bool x) {
#ifdef DEBUG
        if (size_ == capacity_ * WORD_BITS) {
            std::cerr << "Memory overflow. Reallocate befor adding elements" << std::endl;
            exit(1);
        }
#endif
        if (i == size_) {
            push_back(x);
            return;
        }
        p_sum_ += x ? 1 : 0;
        if constexpr (buffer_size != 0) {
            uint8_t idx = buffer_count();
            while (idx > 0) {
                data_type b = buffer_index(buffer_[idx - 1]);
                if (b > i || (b == i && buffer_is_insertion(buffer_[idx - 1]))) {
                    set_buffer_index(b + 1, idx - 1);
                } else {
                    break;
                }
                idx--;
            }
            size_++;
            if (idx == buffer_count()) {
                buffer_[buffer_count()] = create_buffer(i, 1, x);
                metadata_++;
            } else {
                insert_buffer(idx, create_buffer(i, 1, x));
            }
            if (buffer_count() > buffer_size) commit();
        } else {
            size_++;
            auto target_word = i / WORD_BITS;
            auto target_offset = i % WORD_BITS;
            for (size_t j = capacity_ - 1; j > target_word; j--) {
                data_[j] <<= 1;
                data_[j] |= (data_[j - 1] >> 63);
            }
            data_[target_word] =
                (data_[target_word] & ((MASK << target_offset) - 1)) |
                ((data_[target_word] & ~((MASK << target_offset) - 1)) << 1);
            data_[target_word] |= x ? (MASK << target_offset) : uint64_t(0);
        }
    }

    void remove(data_type i) {
        if constexpr (buffer_size != 0) {
            auto x = this->at(i);
            p_sum_ -= x;
            --size_;
            uint8_t idx = buffer_count();
            while (idx > 0) {
                data_type b = buffer_index(buffer_[idx - 1]);
                if (b == i) { 
                    if (buffer_is_insertion(buffer_[idx - 1])) {
                        delete_buffer_element(idx - 1);
                        return;
                    } else {
                        break;
                    }
                } else if (b < i) {
                    break;
                } else {
                    set_buffer_index(b - 1, idx - 1);
                }
                idx--;
            }
            if (idx == buffer_count()) {
                buffer_[idx] = create_buffer(i, 0, x);
                metadata_++;
            } else {
                insert_buffer(idx, create_buffer(i, 0, x));
            }
            if (buffer_count() > buffer_size) commit();
        } else {
            auto target_word = i / WORD_BITS;
            auto target_offset = i % WORD_BITS;
            p_sum_ -= MASK & (data_[target_word] >> target_offset);
            data_[target_word] =
                (data_[target_word] & ((MASK << target_offset) - 1)) |
                ((data_[target_word] >> 1) & (~((MASK << target_offset) - 1)));
            data_[target_word] |= (capacity_ - 1 > target_word)
                                      ? (data_[target_word + 1] << 63)
                                      : 0;
            for (size_t j = target_word + 1; j < capacity_ - 1; j++) {
                data_[j] >>= 1;
                data_[j] |= data_[j + 1] << 63;
            }
            data_[capacity_ - 1] >>=
                (capacity_ - 1 > target_word) ? 1 : 0;
            size_--;
        }
    }

    void push_back(const bool x) {
#ifdef DEBUG
        if (buffer_count() == buffer_size && size_ == capacity_ * WORD_BITS) {
            std::cerr << "Memory overflow. Reallocate befor adding elements" << std::endl;
            exit(1);
        }
#endif
        auto pb_size = size_;
        if constexpr (buffer_size != 0) {
            for (uint8_t i = 0; i < buffer_count(); i++) {
                pb_size += buffer_is_insertion(buffer_[i]) ? -1 : 1;
            }
        }
        size_++;
        
        data_[pb_size / WORD_BITS] |= uint64_t(x) << (pb_size % WORD_BITS);
        p_sum_ += uint64_t(x);

    }

    void set(const data_type i, const bool x) {
        data_type idx = i;
        if constexpr (buffer_size != 0) {
            for (uint8_t j = 0; j < buffer_count(); j++) {
                data_type b = buffer_index(buffer_[j]);
                if (b < i) {
                    idx += buffer_is_insertion(buffer_[j]) ? -1 : 1;
                } else if (b == i) {
                    if (buffer_is_insertion(buffer_[j])) {
                        if (buffer_value(buffer_[j]) != x) {
                            p_sum_ += x ? 1 : -1;
                            buffer_[j] ^= VALUE_MASK;
                        }
                        return;
                    }
                    idx++;
                } else {
                    break;
                }
            }
        }
        const auto word_nr = idx / WORD_BITS;
        const auto pos = idx % WORD_BITS;

        if ((data_[word_nr] & (MASK << pos)) != (uint64_t(x) << pos)) {
            p_sum_ += x ? 1 : -1;
            data_[word_nr] ^= MASK << pos;
        }
    }

    data_type rank(data_type n) const {
        data_type count = 0;

        data_type idx = n;
        if constexpr (buffer_size != 0) {
            for (uint8_t i = 0; i < buffer_count(); i++) {
                if (buffer_index(buffer_[i]) >= n) break;
                if (buffer_is_insertion(buffer_[i])) {
                    idx--;
                    count += buffer_value(buffer_[i]);
                } else {
                    idx++;
                    count -= buffer_value(buffer_[i]);
                }
            }
        }
        data_type target_word = idx / WORD_BITS;
        data_type target_offset = idx % WORD_BITS;
        for (size_t i = 0; i < target_word; i++) {
            count += __builtin_popcountll(data_[i]);
        }
        count += __builtin_popcountll(data_[target_word] &
                                      ((MASK << target_offset) - 1));
        return count;
    }

    data_type select(const data_type x) const {

        data_type pop = 0;
        data_type pos = 0;
        uint8_t current_buffer = 0;
        int8_t a_pos_offset = 0;

        for (data_type j = 0; j < capacity_; j++) {
            pop += __builtin_popcountll(data_[j]);
            pos += 64;
            if constexpr (buffer_size != 0) {
                for (uint8_t b = current_buffer; b < buffer_count(); b++) {
                    data_type b_index = buffer_index(buffer_[b]);
                    if (b_index < pos) {
                        if (buffer_is_insertion(buffer_[b])) {
                            pop += buffer_value(buffer_[b]);
                            pos++;
                            a_pos_offset--;
                        } else {
                            pop -= (data_[(b_index + a_pos_offset) / WORD_BITS] >> ((b_index + a_pos_offset) % WORD_BITS)) & MASK;
                            pos--;
                            a_pos_offset++;
                        }
                        current_buffer++;
                    } else {
                        break;
                    }
                }
            }
            if (pop >= x) break;
        }

        pos = size_ < pos ? size_ : pos;
        pos--;
        while (pop >= x && pos >= 0) {
            pop -= at(pos);
            pos--;
        }
        return ++pos;
    }

    uint64_t bit_size() const {
        return 8 * (sizeof(*this) + capacity_ * sizeof(uint64_t));
    }

    bool need_realloc() const {
        return size_ >= capacity_ * WORD_BITS;
    }

    data_type capacity() const { return capacity_; }

    void set_data_ptr(uint64_t* ptr) { data_ = ptr; }

    void capacity(data_type cap) {capacity_ = cap; }

  private:
    uint8_t buffer_count() const {
        return metadata_ & uint8_t(127);
    }
    bool buffer_value(uint32_t e) const { return (e & VALUE_MASK) != 0; }

    bool buffer_is_insertion(uint32_t e) const { return (e & TYPE_MASK) != 0; }

    data_type buffer_index(uint32_t e) const { return (e & INDEX_MASK) >> 8; }

    void set_buffer_index(uint32_t v, uint8_t i) {
        buffer_[i] = (v << 8) | (buffer_[i] & ((MASK << 7) - 1));
    }

    uint32_t create_buffer(uint32_t idx, bool t, bool v) {
        return ((idx << 8) | (t ? TYPE_MASK : uint32_t(0))) |
               (v ? VALUE_MASK : uint32_t(0));
    }

    void insert_buffer(uint8_t idx, uint32_t buf) {
        memmove(buffer_ + idx + 1, buffer_ + idx,
                (buffer_count() - idx) * sizeof(uint32_t));
        buffer_[idx] = buf;
        metadata_++;
    }

    void delete_buffer_element(uint8_t idx) {
        metadata_--;
        memmove(buffer_ + idx, buffer_ + idx + 1, (buffer_count() - idx) * sizeof(uint32_t));
        buffer_[buffer_count()] = 0;
    }

    void commit() {
        uint64_t overflow = 0;
        uint8_t overflow_length = 0;
        uint8_t underflow_length = 0;
        uint64_t current_word = 0;
        uint8_t current_index = 0;
        uint32_t buf = buffer_[current_index];
        data_type target_word = buffer_index(buf) / WORD_BITS;
        data_type target_offset = buffer_index(buf) % WORD_BITS;

        while (current_word * WORD_BITS < size_) {
            uint64_t underflow =
                current_word + 1 < capacity_ ? data_[current_word + 1] : 0;
            if (overflow_length) {
                underflow = (underflow << overflow_length) |
                            (data_[current_word] >> (64 - overflow_length));
            }

            uint64_t new_overflow = 0;
            // If buffers need to be commit to this word:
            if (current_word == target_word && current_index < buffer_count()) {
                uint64_t word =
                    underflow_length
                        ? (data_[current_word] >> underflow_length) |
                              (underflow << (64 - underflow_length))
                        : (data_[current_word] << overflow_length) | overflow;
                underflow >>= underflow_length;
                uint64_t new_word = 0;
                uint8_t start_offset = 0;
                // While there are buffers for this word
                while (current_word == target_word) {
                    new_word |=
                        (word << start_offset) & ((MASK << target_offset) - 1);
                    word = (word >> (target_offset - start_offset)) |
                           (target_offset == 0
                                ? 0
                                : target_offset - start_offset == 0
                                      ? 0
                                      : (underflow << (64 - (target_offset -
                                                             start_offset))));
                    underflow >>= target_offset - start_offset;
                    if (buffer_is_insertion(buf)) {
                        if (buffer_value(buf)) {
                            new_word |= MASK << target_offset;
                        }
                        start_offset = target_offset + 1;
                        if (underflow_length)
                            underflow_length--;
                        else
                            overflow_length++;
                    } else {
                        word >>= 1;
                        word |= underflow << 63;
                        underflow >>= 1;
                        if (overflow_length)
                            overflow_length--;
                        else
                            underflow_length++;
                        start_offset = target_offset;
                    }
                    current_index++;
                    if (current_index >= buffer_count()) break;
                    buf = buffer_[current_index];
                    target_word = buffer_index(buf) / WORD_BITS;
                    target_offset = buffer_index(buf) % WORD_BITS;
                }
                new_word |=
                    start_offset < 64 ? (word << start_offset) : uint64_t(0);
                new_overflow = overflow_length ? data_[current_word] >>
                                                     (64 - overflow_length)
                                               : 0;
                data_[current_word] = new_word;
            } else {
                if (underflow_length) {
                    data_[current_word] =
                        (data_[current_word] >> underflow_length) |
                        (underflow << (64 - underflow_length));
                } else if (overflow_length) {
                    new_overflow =
                        data_[current_word] >> (64 - overflow_length);
                    data_[current_word] =
                        (data_[current_word] << overflow_length) | overflow;
                    overflow = new_overflow;
                } else {
                    overflow = 0;
                }
            }
            overflow = new_overflow;
            current_word++;
        }
        metadata_ = 0;
    }
};
#endif