
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
    dtype internl_offset;
    leaf_type* leaf;

    r_elem(dtype size, dtype sum, dtype offset, leaf_type* l) {
        p_size = size;
        p_sum = sum;
        leaf = l;
        select_index = 0;
        internl_offset = offset;
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
            //TODO: FIX THIS!
            elems_.push_back(r_elem<dtype, leaf_type>(size_, sum_, 0, leaf));
            i++;
        }
        size_ += a_size;
        sum_ += leaf->p_sum();
    }

    void finalize() {

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
        dtype idx = i / block_size;
        r_elem<dtype, leaf_type> e = elems_[idx];
        if (e.p_size + e.leaf->size() <= i) {
            [[unlikely]] e = elems_[idx + 1];
        }
        return e.p_sum + e.leaf->rank(i - e.p_size);
    }

    dtype select(dtype i) const {
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
};

}  // namespace bv

#endif