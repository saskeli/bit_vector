#ifndef BV_SIMPLE_LEAF_HPP
#define BV_SIMPLE_LEAF_HPP

#include <immintrin.h>

#include <bitset>
#include <cstdint>
#include <cstring>
#include <iostream>

namespace bv {

#define WORD_BITS 64

template <uint8_t buffer_size>
class simple_leaf {
   protected:
    uint8_t buffer_count_;
    uint16_t capacity_;
    uint32_t size_;
    uint32_t buffer_[buffer_size];
    uint32_t p_sum_;  // Should this just be recalculated when needed? Currently
                      // causes branching.
    uint64_t* data_;
    static constexpr uint64_t MASK = 1;
    static constexpr uint32_t VALUE_MASK = 1;
    static constexpr uint32_t TYPE_MASK = 8;
    static constexpr uint32_t INDEX_MASK = ~((uint32_t(1) << 8) - 1);
    static_assert(buffer_size < 64);

   public:
    simple_leaf(uint64_t capacity, uint64_t* data) {
        buffer_count_ = 0;
        capacity_ = capacity;
        size_ = 0;
        p_sum_ = 0;
        data_ = data;
    }

    bool at(const uint64_t i) const {
        if constexpr (buffer_size != 0) {
            uint64_t index = i;
            for (uint8_t idx = 0; idx < buffer_count_; idx++) {
                uint64_t b = buffer_index(buffer_[idx]);
                if (b == i) {
                    if (buffer_is_insertion(buffer_[idx])) {
                        return buffer_value(buffer_[idx]);
                    }
                    index++;
                } else if (b < i) {
                    [[likely]] index -= buffer_is_insertion(buffer_[idx]) * 2 - 1;
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

    void insert(const uint64_t i, const bool x) {
#ifdef DEBUG
        if (size_ >= capacity_ * WORD_BITS) {
            std::cerr << "Overflow. Reallocate before adding elements"
                      << "\n";
            std::cerr << "Attempted to insert to " << i << std::endl;
            exit(1);
        }
#endif
        if (i == size_) {
            push_back(x);
            [[unlikely]] return;
        }
        p_sum_ += x ? 1 : 0;
        if constexpr (buffer_size != 0) {
            uint8_t idx = buffer_count_;
            while (idx > 0) {
                uint64_t b = buffer_index(buffer_[idx - 1]);
                if (b > i ||
                    (b == i && buffer_is_insertion(buffer_[idx - 1]))) {
                    [[likely]] set_buffer_index(b + 1, idx - 1);
                } else {
                    break;
                }
                idx--;
            }
            size_++;
            if (idx == buffer_count_) {
                buffer_[buffer_count_] = create_buffer(i, 1, x);
                [[likely]] buffer_count_++;
            } else {
                insert_buffer(idx, create_buffer(i, 1, x));
            }
            if (buffer_count_ >= buffer_size) [[unlikely]] commit();
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

    bool remove(const uint64_t i) {
        if constexpr (buffer_size != 0) {
            bool x = this->at(i);
            p_sum_ -= x;
            --size_;
            uint8_t idx = buffer_count_;
            while (idx > 0) {
                uint64_t b = buffer_index(buffer_[idx - 1]);
                if (b == i) {
                    if (buffer_is_insertion(buffer_[idx - 1])) {
                        delete_buffer_element(idx - 1);
                        return x;
                    } else {
                        [[likely]] break;
                    }
                } else if (b < i) {
                    break;
                } else {
                    [[likely]] set_buffer_index(b - 1, idx - 1);
                }
                idx--;
            }
            if (idx == buffer_count_) {
                buffer_[idx] = create_buffer(i, 0, x);
                buffer_count_++;
            } else {
                [[likely]] insert_buffer(idx, create_buffer(i, 0, x));
            }
            if (buffer_count_ >= buffer_size) [[unlikely]] commit();
            return x;
        } else {
            uint64_t target_word = i / WORD_BITS;
            uint64_t target_offset = i % WORD_BITS;
            bool x = MASK & (data_[target_word] >> target_offset);
            p_sum_ -= x;
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
            data_[capacity_ - 1] >>= (capacity_ - 1 > target_word) ? 1 : 0;
            size_--;
            return x;
        }
    }

    uint64_t set(const uint64_t i, const bool x) {
        uint64_t idx = i;
        if constexpr (buffer_size != 0) {
            for (uint8_t j = 0; j < buffer_count_; j++) {
                uint64_t b = buffer_index(buffer_[j]);
                if (b < i) {
                    [[likely]] idx += buffer_is_insertion(buffer_[j]) ? -1 : 1;
                } else if (b == i) {
                    if (buffer_is_insertion(buffer_[j])) {
                        if (buffer_value(buffer_[j]) != x) {
                            uint64_t change = x ? 1 : -1;
                            p_sum_ += change;
                            buffer_[j] ^= VALUE_MASK;
                            [[likely]] return change;
                        }
                        [[likely]] return 0;
                    }
                    idx++;
                } else {
                    break;
                }
            }
        }
        uint64_t word_nr = idx / WORD_BITS;
        uint64_t pos = idx % WORD_BITS;

        if ((data_[word_nr] & (MASK << pos)) != (uint64_t(x) << pos)) {
            uint64_t change = x ? 1 : -1;
            p_sum_ += change;
            data_[word_nr] ^= MASK << pos;
            [[likely]] return change;
        }
        return 0;
    }

    uint64_t rank(const uint64_t n) const {
        uint64_t count = 0;

        uint64_t idx = n;
        if constexpr (buffer_size != 0) {
            for (uint8_t i = 0; i < buffer_count_; i++) {
                if (buffer_index(buffer_[i]) >= n) [[unlikely]] break;
                if (buffer_is_insertion(buffer_[i])) {
                    idx--;
                    count += buffer_value(buffer_[i]);
                } else {
                    idx++;
                    count -= buffer_value(buffer_[i]);
                }
            }
        }
        uint64_t target_word = idx / WORD_BITS;
        uint64_t target_offset = idx % WORD_BITS;
        for (size_t i = 0; i < target_word; i++) {
            count += __builtin_popcountll(data_[i]);
        }
        if (target_offset != 0) {
            count += __builtin_popcountll(data_[target_word] &
                                          ((MASK << target_offset) - 1));
        }
        return count;
    }

    uint32_t select(const uint32_t x) const {
        uint32_t pop = 0;
        uint32_t pos = 0;
        uint8_t current_buffer = 0;
        int8_t a_pos_offset = 0;

        for (uint64_t j = 0; j < capacity_; j++) {
            pop += __builtin_popcountll(data_[j]);
            pos += 64;
            if constexpr (buffer_size != 0) {
                for (uint8_t b = current_buffer; b < buffer_count_; b++) {
                    uint64_t b_index = buffer_index(buffer_[b]);
                    if (b_index < pos) {
                        if (buffer_is_insertion(buffer_[b])) {
                            pop += buffer_value(buffer_[b]);
                            pos++;
                            a_pos_offset--;
                        } else {
                            pop -=
                                (data_[(b_index + a_pos_offset) / WORD_BITS] >>
                                 ((b_index + a_pos_offset) % WORD_BITS)) &
                                MASK;
                            pos--;
                            a_pos_offset++;
                        }
                        [[unlikely]] current_buffer++;
                    } else {
                        break;
                    }
                }
            }
            if (pop >= x) [[unlikely]] break;
        }

        pos = size_ < pos ? size_ : pos;
        pos--;
        while (pop >= x && pos < capacity_ * 64) {
            pop -= at(pos);
            pos--;
        }
        return ++pos;
    }

    uint64_t bits_size() const {
        return 8 * (sizeof(*this) + capacity_ * sizeof(uint64_t));
    }

    bool need_realloc() const { return size_ >= capacity_ * WORD_BITS; }

    uint64_t capacity() const { return capacity_; }

    void set_data_ptr(uint64_t* ptr) { data_ = ptr; }

    void capacity(uint64_t cap) { capacity_ = cap; }

    uint64_t* data() { return data_; }

    void clear_first(uint64_t elems) {
        uint64_t ones = rank(elems);
        uint64_t words = elems / WORD_BITS;

        if (elems % WORD_BITS == 0) {
            for (uint64_t i = 0; i < capacity_ - words; i++) {
                data_[i] = data_[i + words];
            }
            for (uint64_t i = capacity_ - words; i < capacity_; i++) {
                data_[i] = 0;
            }
        } else {
            // clear elem bits at start of data
            for (uint64_t i = 0; i < words; i++) {
                data_[i] = 0;
            }
            uint64_t tail = elems % WORD_BITS;
            uint64_t tail_mask = (MASK << tail) - 1;
            data_[words] &= ~tail_mask;

            // Copy data backwars by elem bits
            uint64_t words_to_shuffle = capacity_ - words - 1;
            for (uint64_t i = 0; i < words_to_shuffle; i++) {
                data_[i] = data_[words + i] >> tail;
                data_[i] |= (data_[words + i + 1]) << (WORD_BITS - tail);
            }
            data_[capacity_ - words - 1] = data_[capacity_ - 1] >> tail;
            for (uint64_t i = capacity_ - words; i < capacity_; i++) {
                data_[i] = 0;
            }
            [[unlikely]] (void(0));
        }

        size_ -= elems;
        p_sum_ -= ones;
    }

    template <class sibling_type>
    void transfer_append(sibling_type* other, uint64_t elems) {
        commit();
        other->commit();
        uint64_t* o_data = other->data();
        uint64_t split_point = size_ % WORD_BITS;
        uint64_t target_word = size_ / WORD_BITS;
        uint64_t copy_words = elems / WORD_BITS;
        uint64_t overhang = elems % WORD_BITS;
        if (split_point == 0) {
            for (size_t i = 0; i < copy_words; i++) {
                data_[target_word++] = o_data[i];
                p_sum_ += __builtin_popcountll(o_data[i]);
            }
            if (overhang != 0) {
                data_[target_word] =
                    o_data[copy_words] & ((MASK << overhang) - 1);
                [[unlikely]] p_sum_ += __builtin_popcountll(data_[target_word]);
            }
        } else {
            for (size_t i = 0; i < copy_words; i++) {
                data_[target_word++] |= o_data[i] << split_point;
                data_[target_word] |= o_data[i] >> (WORD_BITS - split_point);
                p_sum_ += __builtin_popcountll(o_data[i]);
            }
            if (elems % 64 != 0) {
                uint64_t to_write =
                    o_data[copy_words] & ((MASK << overhang) - 1);
                p_sum_ += __builtin_popcountll(to_write);
                data_[target_word++] |= to_write << split_point;
                if (target_word < capacity_) {
                    data_[target_word] |= to_write >> (WORD_BITS - split_point);
                }
                [[likely]] ((void)0);
            }
        }
        size_ += elems;
        other->clear_first(elems);
    }

    void clear_last(uint64_t elems) {
        size_ -= elems;
        p_sum_ = rank(size_);
        uint64_t offset = size_ % 64;
        uint64_t words = size_ / 64;
        if (offset != 0) {
            [[likely]] data_[words++] &= (MASK << offset) - 1;
        }
        for (uint64_t i = words; i < capacity_; i++) {
            data_[i] = 0;
        }
    }

    void transfer_prepend(simple_leaf* other, uint64_t elems) {
        commit();
        other->commit();
        uint64_t* o_data = other->data();
        uint64_t words = elems / 64;
        // Make space for new data
        for (uint64_t i = capacity_ - 1; i >= words; i--) {
            data_[i] = data_[i - words];
            if (i == 0) break;
        }
        for (uint64_t i = 0; i < words; i++) {
            data_[i] = 0;
        }
        uint64_t overflow = elems % 64;
        if (overflow > 0) {
            for (uint64_t i = capacity_ - 1; i >= words; i--) {
                data_[i] <<= overflow;
                data_[i] |= data_[i - 1] >> (64 - overflow);
                if (i == 0) break;
            }
            [[likely]] (void(0));
        }

        // Copy over data from sibling
        uint64_t source_word = other->size();
        uint64_t source_offset = source_word % 64;
        source_word /= 64;
        if (source_offset == 0) {
            if (overflow == 0) {
                for (uint64_t i = words - 1; i < words; i--) {
                    data_[i] = o_data[--source_word];
                    p_sum_ += __builtin_popcountll(data_[i]);
                }
            } else {
                source_word--;
                for (uint64_t i = words; i > 0; i--) {
                    p_sum_ += __builtin_popcountll(o_data[source_word]);
                    data_[i] |= o_data[source_word] >> (64 - overflow);
                    data_[i - 1] |= o_data[source_word--] << overflow;
                }
                uint64_t w = o_data[source_word] >> (64 - overflow);
                p_sum_ += __builtin_popcountll(w);
                [[likely]] data_[0] |= w;
            }
            [[unlikely]] (void(0));
        } else {
            if (overflow == 0) {
                for (uint64_t i = words - 1; i < words; i--) {
                    data_[i] = o_data[source_word] << (64 - source_offset);
                    data_[i] |= o_data[--source_word] >> source_offset;
                    p_sum_ += __builtin_popcountll(data_[i]);
                }
            } else {
                uint64_t w;
                for (uint64_t i = words; i > 0; i--) {
                    w = o_data[source_word] << (64 - source_offset);
                    w |= o_data[--source_word] >> source_offset;
                    p_sum_ += __builtin_popcountll(w);
                    data_[i] |= w >> (64 - overflow);
                    data_[i - 1] |= w << overflow;
                }
                w = o_data[source_word] << (64 - source_offset);
                w |= o_data[--source_word] >> source_offset;
                w >>= 64 - overflow;
                p_sum_ += __builtin_popcountll(w);
                [[likely]] data_[0] |= w;
            }
        }
        size_ += elems;
        other->clear_last(elems);
    }

    template <class sibling_type>
    void append_all(sibling_type* other) {
        commit();
        other->commit();
        uint64_t* o_data = other->data();
        uint64_t offset = size_ % 64;
        uint64_t word = size_ / 64;
        uint64_t o_size = other->size();
        uint64_t o_p_sum = other->p_sum();
        uint64_t o_words = o_size / 64;
        o_words += o_size % 64 == 0 ? 0 : 1;
        if (offset == 0) {
            for (uint64_t i = 0; i < o_words; i++) {
                data_[word++] = o_data[i];
            }
            [[unlikely]] (void(0));
        } else {
            for (uint64_t i = 0; i < o_words; i++) {
                data_[word++] |= o_data[i] << offset;
                data_[word] |= o_data[i] >> (64 - offset);
            }
        }
        size_ += o_size;
        p_sum_ += o_p_sum;
    }

    void commit() {
        if constexpr (buffer_size == 0) return;
        if (buffer_count_ == 0) [[unlikely]] return;

        uint64_t overflow = 0;
        uint8_t overflow_length = 0;
        uint8_t underflow_length = 0;
        uint8_t current_index = 0;
        uint32_t buf = buffer_[current_index];
        uint64_t target_word = buffer_index(buf) / WORD_BITS;
        uint64_t target_offset = buffer_index(buf) % WORD_BITS;

        uint64_t words = size_ / WORD_BITS;
        words += size_ % WORD_BITS > 0 ? 1 : 0;
        for (uint64_t current_word = 0; current_word < words; current_word++) {
            uint64_t underflow =
                current_word + 1 < capacity_ ? data_[current_word + 1] : 0;
            if (overflow_length) {
                [[likely]] underflow = (underflow << overflow_length) |
                            (data_[current_word] >> (64 - overflow_length));
            }

            uint64_t new_overflow = 0;
            // If buffers need to be commit to this word:
            if (current_word == target_word && current_index < buffer_count_) {
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
                           (target_offset == 0 ? 0
                            : target_offset - start_offset == 0
                                ? 0
                                : (underflow
                                   << (64 - (target_offset - start_offset))));
                    underflow >>= target_offset - start_offset;
                    if (buffer_is_insertion(buf)) {
                        if (buffer_value(buf)) {
                            new_word |= MASK << target_offset;
                        }
                        start_offset = target_offset + 1;
                        if (underflow_length)
                            [[unlikely]] underflow_length--;
                        else
                            overflow_length++;
                    } else {
                        word >>= 1;
                        word |= underflow << 63;
                        underflow >>= 1;
                        if (overflow_length)
                            overflow_length--;
                        else
                            [[likely]] underflow_length++;
                        start_offset = target_offset;
                    }
                    current_index++;
                    if (current_index >= buffer_count_) [[unlikely]] break;
                    buf = buffer_[current_index];
                    target_word = buffer_index(buf) / WORD_BITS;
                    [[unlikely]] target_offset = buffer_index(buf) % WORD_BITS;
                }
                new_word |=
                    start_offset < 64 ? (word << start_offset) : uint64_t(0);
                new_overflow = overflow_length ? data_[current_word] >>
                                                     (64 - overflow_length)
                                               : 0;
                [[unlikely]] data_[current_word] = new_word;
            } else {
                if (underflow_length) {
                    data_[current_word] =
                        (data_[current_word] >> underflow_length) |
                        (underflow << (64 - underflow_length));
                } else if (overflow_length) {
                    new_overflow =
                        data_[current_word] >> (64 - overflow_length);
                    [[likely]] data_[current_word] =
                        (data_[current_word] << overflow_length) | overflow;
                } else {
                    overflow = 0;
                }
            }
            overflow = new_overflow;
        }
        if (capacity_ > words) [[likely]] data_[words] = 0;
        buffer_count_ = 0;
    }

    void print(bool internal_only) const {
        if (internal_only) return;
        std::cout << "{\n\"type\": \"leaf\",\n"
                  << "\"size\": " << size() << ",\n"
                  << "\"p_sum\": " << p_sum() << ",\n"
                  << "\"buffer_size\": " << int(buffer_size) << ",\n"
                  << "\"buffer\": [\n";
        for (uint8_t i = 0; i < buffer_count_; i++) {
            std::cout << "{\"is_insertion\": "
                      << buffer_is_insertion(buffer_[i]) << ", "
                      << "\"buffer_value\": " << buffer_value(buffer_[i])
                      << ", "
                      << "\"buffer_index\": " << buffer_index(buffer_[i])
                      << "}";
            if (i != buffer_count_ - 1) {
                std::cout << ",\n";
            }
        }
        std::cout << "],\n"
                  << "\"data\": [\n";
        for (uint64_t i = 0; i < capacity_; i++) {
            std::bitset<64> b(data_[i]);
            std::cout << "\"" << b << "\"";
            if (i != capacity_ - 1) {
                std::cout << ",\n";
            }
        }
        std::cout << "]}";
    }

   protected:
    uint8_t buffer_count() const { return buffer_count_; }
    bool buffer_value(uint32_t e) const { return (e & VALUE_MASK) != 0; }

    bool buffer_is_insertion(uint32_t e) const { return (e & TYPE_MASK) != 0; }

    uint64_t buffer_index(uint32_t e) const { return (e & INDEX_MASK) >> 8; }

    void set_buffer_index(uint32_t v, uint8_t i) {
        buffer_[i] = (v << 8) | (buffer_[i] & ((MASK << 7) - 1));
    }

    uint32_t create_buffer(uint32_t idx, bool t, bool v) {
        return ((idx << 8) | (t ? TYPE_MASK : uint32_t(0))) |
               (v ? VALUE_MASK : uint32_t(0));
    }

    void insert_buffer(uint8_t idx, uint32_t buf) {
        memmove(buffer_ + idx + 1, buffer_ + idx,
                (buffer_count_ - idx) * sizeof(uint32_t));
        buffer_[idx] = buf;
        buffer_count_++;
    }

    void delete_buffer_element(uint8_t idx) {
        buffer_count_--;
        memmove(buffer_ + idx, buffer_ + idx + 1,
                (buffer_count_ - idx) * sizeof(uint32_t));
        buffer_[buffer_count_] = 0;
    }

    void push_back(const bool x) {
        auto pb_size = size_;
        if constexpr (buffer_size != 0) {
            for (uint8_t i = 0; i < buffer_count_; i++) {
                pb_size += buffer_is_insertion(buffer_[i]) ? -1 : 1;
            }
        }
        size_++;

        data_[pb_size / WORD_BITS] |= uint64_t(x) << (pb_size % WORD_BITS);
        p_sum_ += uint64_t(x);
    }
};
}  // namespace bv
#endif