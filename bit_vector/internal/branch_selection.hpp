#ifndef BV_BRANCH_SELECTION_HPP
#define BV_BRANCH_SELECTION_HPP

#include <cassert>
#include <cstdint>
#include <cstring>

#include "uncopyable.hpp"

#ifndef CACHE_LINE
// Apparently the most common cache line size is 64.
#define CACHE_LINE 64
#endif

namespace bv {

/**
 * @brief Class providing support for cumulative sums.
 *
 * Supports maintaining cumulative sums for branching in internal nodes.
 *
 * Querying is implemented as a branchless binary search using the sign bit for
 * arithmetic instead of conditional moves.
 *
 * @tparam dtype    Integer type to use for indexing (uint32_t or uint64_t).
 * @tparam branches Maximum branching factor of nodes.
 */
template <class dtype, dtype branches>
class branchless_scan : uncopyable {
   private:
    /**
     * Underlying storage for cumulative sums.
     */
    dtype elems_[branches];

    static_assert((__builtin_popcount(branches) == 1) &&
                      (branches < std::numeric_limits<uint16_t>::max()),
                  "branching factor needs to be a reasonable power of 2");

   public:
    /**
     * Constructor creates an empty container for cumulative sums.
     */
    branchless_scan() : elems_() {}

    /**
     * @brief Get the cumulative sum at `index`.
     *
     * @param index Index to query.
     * @return Cumulative sum at `index`.
     */
    dtype get(uint16_t index) const { return elems_[index]; }

    dtype p_sum() const { return elems_[branches - 1]; }

    /**
     * @brief Set the value at `index` to `value`.
     *
     * Does no updating of internal sums or do parameter validation.
     *
     * A call to `get(i)` will return `v` after a `set(i, v)` call. All other
     * values will remain unchanged.
     *
     * @param index Index of value to modify.
     * @param value New value to set.
     */
    void set(uint16_t index, dtype value) { elems_[index] = value; }

    /**
     * @brief Increments all values in \f$[\mathrm{from},
     * \mathrm{array\size_})\f$ range
     *
     * Cumulative sums will always increment all subsequent values.
     *
     * The value may be negative or zero and will behave as expected.
     *
     * @tparam T Either a signer or unsigned integer type.
     * @param from       Start point of increment.
     * @param change     Value to add ot each element in the range.
     */
    template <class T>
    void increment(uint16_t from, T change) {
        for (uint16_t i = from; i < branches; i++) {
            elems_[i] += change;
        }
    }

    /**
     * @brief Inserts a new value into the cumulative sums.
     *
     * Intended for use when a new node is created between 2 existing nodes.
     * Thus `index` is greater than 0 and less than `array_size`.
     *
     * The cumulative sums of only 2 elements will be impacted since new
     * "things" are not added. Rather some of the existing "things" are
     * rebalanced, with the new element having some of the "old" "things".
     *
     * @param index Position of new element.
     * @param a_value Value of the left element.
     * @param b_value Value of the right element.
     */
    void insert(uint16_t index, dtype a_value, dtype b_value) {
        assert(index > 0);
        for (uint16_t i = branches - 1; i > index; i--) {
            elems_[i] = elems_[i - 1];
        }
        elems_[index - 1] = index != 1 ? elems_[index - 2] + a_value : a_value;
        elems_[index] = elems_[index - 1] + b_value;
    }

    /**
     * @brief Inserts a new value into the cumulative sums.
     *
     * Intended for use when a new node is created after an old node by
     * splitting. Thus `index` is greater than 0 and less than or equal to
     * `array_size`.
     *
     * Cumulative sums of only 1 element will be impacted since this is just a
     * split.
     *
     * @param index Position of new element.
     * @param value Value of the new element.
     */
    void insert(uint16_t index, dtype value) {
        assert(index > 0);
        for (uint16_t i = branches - 1; i >= index; i--) {
            elems_[i] = elems_[i - 1];
        }
        elems_[index - 1] -= value;
    }

    /**
     * @brief Removes a value from the cumulative sums
     *
     * Intended for use when merging nodes.
     *
     * Does not update the cumulative sums as it is expected that the elements
     * in the removed node have been moved.
     *
     * @param index Position of element to remove.
     */
    void remove(uint16_t index) {
        for (uint16_t i = index; i < branches - 1; i++) {
            elems_[i] = elems_[i + 1];
        }
    }

    /**
     * @brief Removes the first `n` elements from the structure.
     *
     * Intended for use when rebalancing. A sibling has taken over some of the
     * children and this is called to remove the moved children from the current
     * node. Subsequent cumulative sums will be updated accordingly.
     *
     * @param n          Number of elements to remove.
     */
    void clear_first(uint16_t n) {
        dtype subtrahend = elems_[n - 1];
        dtype m_v = elems_[branches - 1] - subtrahend;
        for (uint16_t i = n; i < branches; i++) {
            elems_[i - n] = elems_[i] - subtrahend;
        }
        for (uint16_t i = branches - n; i < branches; ++i) {
            elems_[i] = m_v;
        }
    }

    /**
     * @brief Removes the last `n` elements from the structure.
     *
     * Intended for use when rebalancing. A sibling has taken over some of the
     * children and this is called to remove the moved children from the current
     * node.
     *
     * @param n          Number of elements to remove.
     * @param array_size Number of elements stored.
     */
    void clear_last(uint16_t n, uint16_t array_size) {
        dtype m_v = n < array_size ? elems_[array_size - n - 1] : 0;
        for (uint16_t i = array_size - n; i < branches; i++) {
            elems_[i] = m_v;
        }
    }

    /**
     * @brief Adds elements from `other` to the end of `this`
     *
     * Intended for use when rebalancing. Adds `n_elems` elements from `other`
     * to the end of "these" cumulative sums. Should be subsequently removed
     * from `other`
     *
     * @param n_elems    Number of elements to append.
     * @param array_size Number of elements stored in `this`.
     * @param other      Other cumulative size structure to copy from.
     */
    void append(uint16_t n_elems, uint16_t array_size,
                const branchless_scan* const other) {
        dtype addend = array_size != 0 ? elems_[array_size - 1] : 0;
        for (uint16_t i = 0; i < n_elems; i++) {
            elems_[i + array_size] = addend + other->get(i);
        }
        for (uint16_t i = n_elems + array_size; i < branches; ++i) {
            elems_[i] = elems_[array_size + n_elems - 1];
        }
    }

    /**
     * @brief Adds single element to the end of `this`
     *
     * Used when splitting root nodes.
     *
     * @param index Size of this.
     * @param value Value to append.
     */
    void append(uint16_t index, dtype value) {
        if (index == 0) {
            elems_[index] = value;
        } else {
            elems_[index] = elems_[index - 1] + value;
        }
        for (uint16_t i = index + 1; i < branches; ++i) {
            elems_[i] = elems_[i - 1];
        }
    }

    /**
     * @brief Adds elements form `other` to the start of `this`
     *
     * Intended for use when rebalancing. Adds `n_elemes` elements from `other`
     * to the start of "these" cumulative sums. Should be subsequently removed
     * from `other`
     *
     * @param n_elems    Number of elements to append.
     * @param o_size     Number of elements stored in `other`.
     * @param other      Other cumulative size structure to copy from.
     */
    void prepend(uint16_t n_elems, uint16_t o_size,
                 const branchless_scan* const other) {
        memmove(elems_ + n_elems, elems_, (branches - n_elems) * sizeof(dtype));
        dtype subtrahend =
            n_elems < o_size ? other->get(o_size - n_elems - 1) : 0;
        for (uint16_t i = 0; i < n_elems; i++) {
            elems_[i] = other->get(i + o_size - n_elems) - subtrahend;
        }
        for (uint16_t i = n_elems; i < branches; i++) {
            elems_[i] += elems_[n_elems - 1];
        }
    }

    /**
     * @brief Find the lowest child index s.t. the cumulative sum at the index
     * is at least q.
     *
     * This is implemented as a branchless binary search that uses the "sign
     * bit" for index manipulations instead of conditional moves. This is the
     * reason for limiting the maximum data structure size to `(~dtype(0)) >>
     * 1`.
     *
     * If \f$q > \f$ `elems_[branches - 1]`, this will return either the index
     * of the last child or the index following the last child. Either way, this
     * will likely very quickly lead to undefined behaviour and segmentation
     * fault.
     *
     * If `elems_[branches - 1] > (~(dtype(0)) >> 1`, querying is considered
     * undefined behaviour.
     *
     * This conditionally compiles the binary search based on acceptable branch
     * factors. For low branching factors this is expected to be slightly slower
     * than efficient vectorized linear searches. For higher branching factors
     * this branchless binary search should be faster as long as cache
     * performance is good. Aggressive prefetching is done in an attempt to
     * ensure that cache misses don't occur during querying. See
     * https://github.com/saskeli/search_microbench for a simple benchmark.
     *
     * @param q Query target
     * @return \f$\underset{i}{\mathrm{arg min}}(\mathrm{cum\_sums}[i] \geq
     * q)\f$.
     */
    uint16_t find(dtype q) const {
        constexpr dtype SIGN_BIT = ~((~dtype(0)) >> 1);
        constexpr dtype num_bits = sizeof(dtype) * 8;
        constexpr dtype lines = CACHE_LINE / sizeof(dtype);
        constexpr const uint16_t u_bits = 30 - __builtin_clz(branches);
        for (dtype i = 0; i < branches; i += lines) {
            __builtin_prefetch(elems_ + i);
        }
        uint16_t idx = (uint16_t(1) << u_bits) - 1;
        for (uint16_t i = u_bits; i > 0; --i) {
            idx ^= (dtype((elems_[idx] - q) & SIGN_BIT) >> (num_bits - i - 1)) |
                   (uint16_t(1) << (i - 1));
        }
        idx ^= (dtype((elems_[idx] - q) & SIGN_BIT) >> (num_bits - 1));
        return idx;
    }

    std::ostream& print(std::ostream& out = std::cout) const {
        std::cout << "[";
        for (uint16_t i = 0; i < branches; ++i) {
            std::cout << elems_[i] << (i == branches - 1 ? "]\n" : ", ");
        }
        std::cout << std::flush;
        return out;
    }
};

template <class dtype, dtype branches>
class heap_order_branching {
   private:
    static_assert((__builtin_popcount(branches) == 1) && (branches > 1) &&
                      (branches < std::numeric_limits<uint16_t>::max()),
                  "branching factor needs to be a reasonable power of 2");
    inline static dtype scratch_[branches];
    dtype values_[branches];

   public:
    heap_order_branching() : values_() {}

    /**
     * @brief Get the cumulative sum at `index`.
     *
     * @param index Index to query.
     * @return Cumulative sum at `index`.
     */
    dtype get(uint16_t index) const {
        if (index == branches - 1) {
            return values_[0];
        }
        ++index;
        uint16_t trg = branches / 2;
        uint16_t offset = trg / 2;
        uint16_t idx = 1;
        dtype res = 0;
        const constexpr uint16_t levels = 31 - __builtin_clz(branches);
        for (uint32_t i = 0; i < levels; ++i) {
            bool b = index >= trg;
            res += b * values_[idx];
            trg += b * 2 * offset - offset;
            idx = idx * 2 + b;
            offset /= 2;
        }
        return res;
    }

    /**
     * @brief Gets the total for the entire structure.
     *
     * @return Cumulative sum at `branches - 1`.
     */
    dtype p_sum() const { return values_[0]; }

    /**
     * @brief Set the value at `index` to `value`.
     *
     * Does no updating of internal sums or do parameter validation.
     *
     * A call to `get(i)` will return `v` after a `set(i, v)` call. All other
     * values will remain unchanged.
     *
     * @param index Index of value to modify.
     * @param value New value to set.
     */
    void set(uint16_t index, dtype value) {
        if (index == branches - 1) {
            values_[0] = value;
            return;
        }
        ++index;
        uint16_t trg = branches / 2;
        uint16_t offset = trg / 2;
        uint16_t idx = 1;
        int64_t res = value;
        const constexpr uint16_t levels = 31 - __builtin_clz(branches);
        for (uint32_t i = 0; i < levels; ++i) {
            bool b = index >= trg;
            res -= b * values_[idx];
            trg += b * 2 * offset - offset;
            idx = idx * 2 + b;
            offset /= 2;
        }
        --index;
        trg = branches / 2;
        offset = trg / 2;
        idx = 1;
        for (uint32_t i = 0; i < levels; ++i) {
            bool b = index >= trg;
            values_[idx] += (!b) * res;
            trg += b * 2 * offset - offset;
            idx = idx * 2 + b;
            offset /= 2;
        }
        values_[0] += res;
    }

    /**
     * @brief Increments all values in \f$[\mathrm{from},
     * \mathrm{array\size_})\f$ range
     *
     * Cumulative sums will always increment all subsequent values. Has to be
     * passed as a parameter, since the value is not maintained by the
     * `branch_selection` object.
     *
     * The value may be negative or zero and will behave as expected.
     *
     * @tparam T Either a signer or unsigned integer type.
     * @param index      Element to increment.
     * @param change     Value to add.
     */
    template <class T>
    void increment(uint16_t index, T change) {
        if (index == branches - 1) {
            values_[0] += change;
            return;
        }
        uint16_t trg = branches / 2;
        uint16_t offset = trg / 2;
        uint16_t idx = 1;
        const constexpr uint16_t levels = 31 - __builtin_clz(branches);
        for (uint32_t i = 0; i < levels; ++i) {
            bool b = index >= trg;
            values_[idx] += (!b) * change;
            trg += b * 2 * offset - offset;
            idx = idx * 2 + b;
            offset /= 2;
        }
        values_[0] += change;
    }

    /**
     * @brief Inserts a new value into the cumulative sums.
     *
     * Intended for use when a new node is created between 2 existing nodes.
     * Thus `index` is greater than 0 and less than `array_size`.
     *
     * The cumulative sums of only 2 elements will be impacted since new
     * "things" are not added. Rather some of the existing "things" are
     * rebalanced, with the new element having some of the "old" "things".
     *
     * @param index Position of new element.
     * @param a_value Value of the left element.
     * @param b_value Value of the right element.
     */
    void insert(uint16_t index, dtype a_value, dtype b_value) {
        assert(index > 0);
        to_scratch();
        for (uint16_t i = branches - 1; i > index; i--) {
            scratch_[i] = scratch_[i - 1];
        }
        scratch_[index - 1] = index != 1 ? scratch_[index - 2] + a_value : a_value;
        scratch_[index] = scratch_[index - 1] + b_value;
        from_scratch();
    }

    /**
     * @brief Inserts a new value into the cumulative sums.
     *
     * Intended for use when a new node is created after an old node by
     * splitting. Thus `index` is greater than 0 and less than or equal to
     * `array_size`.
     *
     * Cumulative sums of only 1 element will be impacted since this is just a
     * split.
     *
     * @param index Position of new element.
     * @param value Value of the new element.
     */
    void insert(uint16_t index, dtype value) {
        assert(index > 0);
        to_scratch();
        for (uint16_t i = branches - 1; i >= index; i--) {
            scratch_[i] = scratch_[i - 1];
        }
        scratch_[index - 1] -= value;
        from_scratch();
    }

    /**
     * @brief Removes a value from the cumulative sums
     *
     * Intended for use when merging nodes.
     *
     * Does not update the cumulative sums as it is expected that the elements
     * in the removed node have been moved.
     *
     * @param index Position of element to remove.
     */
    void remove(uint16_t index) {
        to_scratch();
        for (uint16_t i = index; i < branches - 1; i++) {
            scratch_[i] = scratch_[i + 1];
        }
        from_scratch();
    }

    /**
     * @brief Removes the first `n` elements from the structure.
     *
     * Intended for use when rebalancing. A sibling has taken over some of the
     * children and this is called to remove the moved children from the current
     * node. Subsequent cumulative sums will be updated accordingly.
     *
     * @param n          Number of elements to remove.
     */
    void clear_first(uint16_t n) {
        to_scratch();
        dtype subtrahend = scratch_[n - 1];
        dtype m_v = scratch_[branches - 1] - subtrahend;
        for (uint16_t i = n; i < branches; i++) {
            scratch_[i - n] = scratch_[i] - subtrahend;
        }
        for (uint16_t i = branches - n; i < branches; ++i) {
            scratch_[i] = m_v;
        }
        from_scratch();
    }

    /**
     * @brief Removes the last `n` elements from the structure.
     *
     * Intended for use when rebalancing. A sibling has taken over some of the
     * children and this is called to remove the moved children from the current
     * node.
     *
     * @param n          Number of elements to remove.
     * @param array_size Number of elements stored.
     */
    void clear_last(uint16_t n, uint16_t array_size) {
        to_scratch();
        dtype m_v = n < array_size ? scratch_[array_size - n - 1] : 0;
        for (uint16_t i = array_size - n; i < branches; i++) {
            scratch_[i] = m_v;
        }
        from_scratch();
    }

    /**
     * @brief Adds elements from `other` to the end of `this`
     *
     * Intended for use when rebalancing. Adds `n_elems` elements from `other`
     * to the end of "these" cumulative sums. Should be subsequently removed
     * from `other`
     *
     * @param n_elems    Number of elements to append.
     * @param array_size Number of elements stored in `this`.
     * @param other      Other cumulative size structure to copy from.
     */
    void append(uint16_t n_elems, uint16_t array_size,
                heap_order_branching* other) {
        other->to_scratch();
        dtype o_arr[branches];
        std::memcpy(o_arr, scratch_, branches * sizeof(dtype));
        to_scratch();
        dtype addend = array_size != 0 ? scratch_[array_size - 1] : 0;
        for (uint16_t i = 0; i < n_elems; i++) {
            scratch_[i + array_size] = addend + o_arr[i];
        }
        for (uint16_t i = n_elems + array_size; i < branches; ++i) {
            scratch_[i] = scratch_[array_size + n_elems - 1];
        }
        from_scratch();
    }

    /**
     * @brief Adds single element to the end of `this`
     *
     * Used when splitting root nodes.
     *
     * @param index Size of this.
     * @param value Value to append.
     */
    void append(uint16_t index, dtype value) {
        increment(index, value);
    }

    /**
     * @brief Adds elements form `other` to the start of `this`
     *
     * Intended for use when rebalancing. Adds `n_elemes` elements from `other`
     * to the start of "these" cumulative sums. Should be subsequently removed
     * from `other`
     *
     * @param n_elems    Number of elements to append.
     * @param o_size     Number of elements stored in `other`.
     * @param other      Other cumulative size structure to copy from.
     */
    void prepend(uint16_t n_elems, uint16_t o_size,
                 heap_order_branching* other) {
        to_scratch();
        dtype tmp[branches];
        std::memcpy(tmp, scratch_, branches * sizeof(dtype));
        other->to_scratch();
        dtype subtrahend =
            n_elems < o_size ? scratch_[o_size - n_elems - 1]: 0;
        for (uint16_t i = 0; i < n_elems; i++) {
            scratch_[i] = scratch_[i + o_size - n_elems] - subtrahend;
        }
        for (uint16_t i = n_elems; i < branches; i++) {
            scratch_[i] = tmp[i - n_elems] + scratch_[n_elems - 1];
        }
        from_scratch();
    }

    /**
     * @brief Find the lowest child index s.t. the cumulative sum at the index
     * is at least q.
     *
     * If \f$q > \f$ `elems_[branches - 1]`, this will return either the index
     * of the last child or the index following the last child. Either way, this
     * will likely very quickly lead to undefined behaviour and segmentation
     * fault.
     *
     * If `elems_[branches - 1] > (~(dtype(0)) >> 1`, querying is considered
     * undefined behaviour.
     *
     * This conditionally compiles the binary search based on acceptable branch
     * factors. For low branching factors this is expected to be slightly slower
     * than efficient vectorized linear searches. For higher branchin factors
     * this branchless binary search should be faster as long as cache
     * performance is good. Agressive prefetching is done in an attempt to
     * ensure that cache misses don't occur during querying. See
     * https://github.com/saskeli/search_microbench for a simple benchmark.
     *
     * @param q Query target
     * @return \f$\underset{i}{\mathrm{arg min}}(\mathrm{cum\_sums}[i] \geq
     * q)\f$.
     */
    uint16_t find(dtype q) const { 
        uint16_t idx = 1;
        uint16_t trg = 0;
        for (uint16_t offset = branches / 2; offset > 0; offset /= 2) {
            bool b = values_[idx] < q;
            q -= b * values_[idx];
            idx = idx * 2 + b;
            trg += b * offset;
        }
        return trg;
    }

    std::ostream& print(std::ostream& out = std::cout) const {
        std::cout << "[";
        for (uint16_t i = 0; i < branches; ++i) {
            std::cout << values_[i] << (i == branches - 1 ? "]\n" : ", ");
        }
        std::cout << std::flush;
        return out;
    }

   private:
    template<uint16_t idx>
    __attribute__((always_inline)) void to_scratch(dtype partial, uint16_t& target_idx) {
        if constexpr (idx >= branches / 2) {
            if (target_idx > 0) {
                scratch_[target_idx++] = partial;
            }
            scratch_[target_idx++] = partial + values_[idx];
        } else {
            to_scratch<idx * 2>(partial, target_idx);
            to_scratch<idx * 2 + 1>(partial + values_[idx], target_idx);
        }
    }

    void to_scratch() {
        uint16_t target_idx = 0;
        to_scratch<1>(0, target_idx);
        scratch_[branches - 1] = values_[0];
    }

    template<uint16_t offset>
    __attribute__((always_inline)) void from_scratch(dtype partial, uint16_t source_idx, uint16_t target_index) {
        values_[target_index] = scratch_[source_idx] - partial;
        if constexpr (offset) {
            from_scratch<offset / 2>(partial, source_idx - offset, target_index * 2);
            from_scratch<offset / 2>(partial + values_[target_index], source_idx + offset, target_index * 2 + 1);
        }
    }

    void from_scratch() {
        from_scratch<branches / 4>(0, branches / 2 - 1, 1);
        values_[0] = scratch_[branches - 1];
    }
};

}  // namespace bv

#endif