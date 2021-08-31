
#ifndef BV_QUERY_SUPPORT_HPP
#define BV_QUERY_SUPPORT_HPP

#include <cassert>
#include <cstdint>
#include <vector>

namespace bv {

/**
 * @brief Block storage used by the query support structure.
 */
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

/**
 * @brief Support structure for bv::bit_vector to enable fast queries.
 *
 * Precalculates results for rank, select and access queries along with leaf
 * pointers to enable faster queries while the bit vector remains unmodified.
 *
 * Leaves should be appended in natural order. After all leaves have been added,
 * the structure should be finalized to ensure properly precalculated select
 * blocks.
 *
 * If the underlying structure changes, querying the support structure may lead
 * to undefined behaviour. The support structure should be discarded
 * (`delete(q)`) as it becomes outdated.
 *
 * @tparam dtype     Integer type to use for indexing (uint32_t or uint64_t).
 * @tparam leaf_type Leaf type used by the relevant bv::bit_vector.
 */
template <class dtype, class leaf_type, dtype block_size>
class query_support {
   protected:
    dtype size_ = 0;
    dtype sum_ = 0;
    std::vector<r_elem<dtype, leaf_type>> elems_ =
        std::vector<r_elem<dtype, leaf_type>>();

   public:
    /**
     * @brief Add leaf reference to the support structure.
     *
     * Block results are calculated and stored for \f$\approx
     * \frac{\mathrm{leaf->size()}}{\mathrm{block\_size}}\f$ positions for the
     * leaf.
     *
     * @param leaf Pointer to the leaf to add.
     */
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

    /**
     * @brief Prepare the structure for querying
     *
     * Precalculates and stores results for some select queries to speed up
     * subsequent select queries.
     *
     * For sparse bit vectors where p_sum < size / block_size, locations for all
     * 1-bits are precalculated, to allow constant time select queries.
     */
    void finalize() {
        if (sum_ <= elems_.size()) {
            for (size_t i = 0; i < sum_; i++) {
                elems_[i].select_index = dumb_select(i + 1);
            }
            [[unlikely]] (void(0));
        } else {
            for (size_t i = 0; i < elems_.size(); i++) {
                uint64_t s_trg = 1 + (i * sum_) / elems_.size();
                uint64_t s_idx = s_select(s_trg);
                elems_[i].select_index = s_idx;
            }
        }
    }

    /**
     * @brief Number of bits stored in the bit vector.
     *
     * @return Number of bits stored in the bit vector.
     */
    dtype size() const { return size_; }
    /**
     * @brief Number of 1-bits stored in the bit vector.
     *
     * @return Number of 1-bits stored in the bit vector.
     */
    dtype p_sum() const { return sum_; }

    /**
     * @brief Return value of bit at index \f$i\f$ in the underlying bit vector.
     *
     * Uses precalculated partial block sums to locate the leaf containing the
     * \f$i\f$<sup>th</sup> bit in constant time.
     *
     * @param i Index to query.
     * @return Value of bit at index \f$i\f$.
     */
    bool at(dtype i) const {
        dtype idx = i / block_size;
        r_elem<dtype, leaf_type> e = elems_[idx];
        if (e.p_size + e.leaf->size() <= i) {
            [[unlikely]] e = elems_[idx + 1];
        }
        return e.leaf->at(i - e.p_size);
    }

    /**
     * @brief Number of 1-bits up to position \f$i\f$ in the underlying bit
     * vector.
     *
     * Precalculated results are used to locate the the leaf containing the
     * \f$i\f$<sup>th</sup> bit in constant time. at most
     * \f$\mathrm{block\_size}\f$ are read from the leaf, to calculate the
     * result for the rank query based on precalculated block results.
     *
     * @param i Number of elements to include in the "summation".
     *
     * @return \f$\sum_{i = 0}^{i - 1} \mathrm{bv}[i]\f$.
     */
    dtype rank(dtype i) const {
        dtype idx = block_size;
        idx = i / idx;
        if (idx == elems_.size()) idx--;
        r_elem<dtype, leaf_type> e = elems_[idx];
        if (e.p_size + e.leaf->size() < i) {
            e = elems_[++idx];
            [[unlikely]] return e.p_sum + e.leaf->rank(i - e.p_size);
        }
        dtype offs = idx * block_size - e.p_size;
        return e.p_sum + e.internal_offset + e.leaf->rank(i - e.p_size, offs);
    }

    /**
     * @brief Index of the \f$i\f$<sup>th</sup> 1-bit in the data structure.
     *
     * For fairly dense approximately uniformly distributed bit vectors, the
     * block containing the \f$i\f$<sup>th</sup> 1-bit can be located in
     * constant time. For less dense or non-uniformly distributed bit vectors
     * the block containing the \f$i\f$<sup>th</sup> 1-bit is located either
     * in constant time or logarithmic time depending on the local bit
     * distribution in the vicinity of the \f$i\f$<sup>th</sup> bit.
     *
     * After the correct block is located, at most `block_size` bits are scanned
     * for a single leaf.
     *
     * For sparse bit vectors where p_sum < size / block_size, locations for all
     * 1-bits have been precalculated, allowing constant time select queries.
     *
     * @param i Selection target.
     * @return \f$\underset{j \in [0..n)}{\mathrm{arg min}}\left(\sum_{k = 0}^j
     * \mathrm{bv}[k]\right) =  i\f$.
     */
    dtype select(dtype i) const {
        if (sum_ <= elems_.size()) {
            [[unlikely]] return elems_[i - 1].select_index;
        }
        dtype idx = elems_.size() * i / (sum_ + 1);
        if (idx == elems_.size()) {
            [[unlikely]] idx--;
        }
        dtype a_idx = elems_[idx].select_index;
        dtype b_idx =
            idx < elems_.size() - 1 ? elems_[idx + 1].select_index : a_idx;
        if (b_idx - a_idx > 1 || idx  == elems_.size() - 1) {
            [[unlikely]] a_idx = s_select(i);
        }
        r_elem<dtype, leaf_type> e = elems_[a_idx];
        if (e.p_sum + e.leaf->p_sum() < i) {
            e = elems_[a_idx + 1];
            [[unlikely]] return e.p_size + e.leaf->select(i - e.p_sum);
        }
        dtype s_pos = a_idx * block_size - e.p_size;
        if (s_pos == 0) {
            [[unlikely]] return e.p_size + e.leaf->select(i - e.p_sum);
        }
        return e.p_size + e.leaf->select(i - e.p_sum,
                                         a_idx * block_size - e.p_size,
                                         e.internal_offset);
    }

    /**
     * @brief Number of bits allocated for the support structure.
     *
     * The space allocated for the leaves is not included in the total as those
     * are accounted for by the bv.bit_size().
     *
     * @return Number of bits allocated for the support structure.
     */
    uint64_t bit_size() const {
        return (sizeof(query_support) +
                elems_.capacity() * sizeof(r_elem<dtype, leaf_type>)) *
               8;
    }

    /**
     * @brief Outputs json representation of the support structure.
     *
     * @param internal_only Should raw leaf data be omitted from the output.
     */
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
    /**
     * @brief Locate the block containing the \f$i\f$<sup>th</sup> 1-bit.
     *
     * Used for building the select support index for dense bit vectors.
     *
     * @param i Selection target.
     *
     * @return The index of the block containing the \f$i\f$<sup>th</sup> 1-bit.
     */
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

    /**
     * @brief Index of the \f$i\f$<sup>th</sup> 1-bit in the data structure.
     *
     * Used for populating precalculated sums for sparese vectors.
     *
     * @param i Selection target.
     *
     * @return @return \f$\underset{j \in [0..n)}{\mathrm{arg min}}\left(\sum_{k
     * = 0}^j \mathrm{bv}[k]\right) =  i\f$.
     */
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