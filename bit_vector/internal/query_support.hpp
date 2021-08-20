
#ifndef BV_QUERY_SUPPORT_HPP
#define BV_QUERY_SUPPORT_HPP

#include <cassert>
#include <cstdint>
#include <vector>

namespace bv {

template <class dtype, class leaf_type>
struct r_elem {
    dtype p_size;
    dtype p_sum;
    dtype select_index;
    dtype internal_offset;
    leaf_type* leaf;

    r_elem(dtype size, dtype sum, dtype offset, leaf_type* l) {
        p_size = size;
        p_sum = sum;
        leaf = l;
        select_index = 0;
        internal_offset = offset;
    }
};

template <class dtype, class leaf_type, dtype block_size>
class query_support {
   protected:
    dtype size_ = 0;
    dtype sum_ = 0;
    std::vector<r_elem<dtype, leaf_type>> elems_ =
        std::vector<r_elem<dtype, leaf_type>>();

   public:
    void append(leaf_type* leaf) {
        dtype i = elems_.size();
        dtype a_size = leaf->size();
        while (size_ + a_size > i * block_size) {
            dtype i_rank = leaf->rank(i * block_size - size_);
            elems_.push_back(
                r_elem<dtype, leaf_type>(size_, sum_, i_rank, leaf));
            i++;
        }
        size_ += a_size;
        sum_ += leaf->p_sum();
    }

    void finalize() {
        if (sum_ <= elems_.size()) {
            for (size_t i = 0; i < sum_; i++) {
                elems_[i].select_index = dumb_select(i + 1);
            }
            [[unlikely]] (void(0));
        } else {
            for (size_t i = 0; i < elems_.size(); i++) {
                elems_[i].select_index = s_select(1 + (i * sum_) / elems_.size());
            }
        }
    }

    dtype size() const { return size_; }
    dtype p_sum() const { return sum_; }

    bool at(dtype i) const {
        dtype idx = i / block_size;
        r_elem<dtype, leaf_type> e = elems_[idx];
        if (e.p_size + e.leaf->size() <= i) {
            [[unlikely]] e = elems_[idx + 1];
        }
        return e.leaf->at(i - e.p_size);
    }

    dtype rank(dtype i) const {
        dtype idx = block_size;
        idx = i / idx;
        r_elem<dtype, leaf_type> e = elems_[idx];
        if (e.p_size + e.leaf->size() < i) {
            e = elems_[++idx];
            [[unlikely]] return e.p_sum + e.leaf->rank(i - e.p_size);
        }
        dtype offs = idx * block_size - e.p_size;
        return e.p_sum + e.internal_offset + e.leaf->rank(i - e.p_size, offs);
    }

    dtype select(dtype i) const {
        if (i == 0) [[unlikely]]
            return -1;
        if (sum_ <= elems_.size()) {
            [[unlikely]] return elems_[i - 1].select_index;
        }
        dtype idx = elems_.size() * i / (sum_ + 1);
        if (idx == elems_.size()) [[unlikely]] idx--;
        dtype a_idx = elems_[idx].select_index;
        dtype b_idx = a_idx == elems_.size() - 1 ? elems_.size() - 1
                                                 : elems_[idx + 1].select_index;
        if (b_idx - a_idx > 1) [[unlikely]] return dumb_select(i);
        r_elem<dtype, leaf_type> e = elems_[a_idx];
        if (e.p_sum + e.leaf->p_sum() < i) {
            [[unlikely]] e = elems_[a_idx + 1];
        }
        return e.p_size + e.leaf->select(i - e.p_sum);
    }

    uint64_t bit_size() const {
        return (sizeof(query_support) +
                elems_.capacity() * sizeof(r_elem<dtype, leaf_type>)) *
               8;
    }

    void print(bool internal_only) const {
        std::cout << "{\n\"type\": \"rank_support\",\n"
                  << "\"size\": " << size_ << ",\n"
                  << "\"sum\": " << sum_ << ",\n"
                  << "\"number of elems\": " << elems_.size() << ",\n"
                  << "\"p_sizes\": [";
        for (size_t i = 0; i < elems_.size(); i++) {
            std::cout << elems_[i].p_size;
            if (i != elems_.size() - 1) {
                std::cout << ", ";
            }
        }
        std::cout << "],\n"
                  << "\"p_sums\": [";
        for (size_t i = 0; i < elems_.size(); i++) {
            std::cout << elems_[i].p_sum;
            if (i != elems_.size() - 1) {
                std::cout << ", ";
            }
        }
        std::cout << "],\n"
                  << "\"internal_offsets\": [";
        for (size_t i = 0; i < elems_.size(); i++) {
            std::cout << elems_[i].internal_offset;
            if (i != elems_.size() - 1) {
                std::cout << ", ";
            }
        }
        std::cout << "],\n"
                  << "\"select_index\": [";
        for (size_t i = 0; i < elems_.size(); i++) {
            std::cout << elems_[i].select_index;
            if (i != elems_.size() - 1) {
                std::cout << ", ";
            }
        }
        std::cout << "],\n"
                  << "\"nodes\": [";
        for (uint8_t i = 0; i < elems_.size(); i++) {
            elems_[i].leaf->print(internal_only);
            if (i != elems_.size() - 1) {
                std::cout << ",";
            }
            std::cout << "\n";
        }
        std::cout << "]}" << std::endl;
    }

   protected:
    dtype s_select(dtype i) const {
        dtype idx = 0;
        dtype b = elems_.size() - 1;
        while (idx < b) {
            dtype m = (idx + b + 1) / 2;
            if (elems_[m].p_sum + elems_[m].internal_offset >= i) {
                b = m - 1;
            } else {
                idx = m;
            }
        }
        r_elem<dtype, leaf_type> e = elems_[idx];
        while (e.p_sum + e.leaf->p_sum() < i) {
            idx++;
            [[unlikely]] e = elems_[idx];
        }
        return idx;
    }

    dtype dumb_select(dtype i) const {
        dtype idx = 0;
        dtype b = elems_.size() - 1;
        while (idx < b) {
            dtype m = (idx + b + 1) / 2;
            if (elems_[m].p_sum >= i) {
                b = m - 1;
            } else {
                idx = m;
            }
        }
        r_elem<dtype, leaf_type> e = elems_[idx];
        while (e.p_sum + e.leaf->p_sum() < i) {
            idx++;
            [[unlikely]] e = elems_[idx];
        }
        return e.p_size + e.leaf->select(i - e.p_sum);
    }
};

}  // namespace bv

#endif