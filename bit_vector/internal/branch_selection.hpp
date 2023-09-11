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

    static_assert((branches == 8) || (branches == 16) || (branches == 32) ||
                      (branches == 64) || (branches == 128) || (branches == 256) ||
                      (branches == 512) || (branches == 1024) || (branches == 2048),
                  "branching factor needs to be a reasonable power of 2");

   public:
    /**
     * Constructor creates an empty container for cumulative sums.
     */
    branchless_scan() {
        for (dtype i = 0; i < branches; i++) {
            elems_[i] = (~dtype(0)) >> 1;
        }
    }

    /**
     * @brief Get the cumulative sum at `index`.
     *
     * @param index Index to query.
     * @return Cumulative sum at `index`.
     */
    dtype get(uint16_t index) const { return elems_[index]; }

    /**
     * @brief Set the value at `index` to `value`.
     *
     * Does no updating of internal sums or parameter validation.
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
     * Cumulative sums will always increment all subsequent values. Has to be
     * passed as a parameter, since the value is not maintained by the
     * `branch_selection` object.
     *
     * The value may be negative or zero and will behave as expected.
     *
     * @tparam T Either a signer or unsigned integer type.
     * @param from       Start point of increment.
     * @param array_size End point of increment (size of array).
     * @param change     Value to add ot each element in the range.
     */
    template <class T>
    void increment(uint16_t from, uint16_t array_size, T change) {
        for (uint16_t i = from; i < array_size; i++) {
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
     * @param array_size Number of elements currently in the array.
     * @param a_value Value of the left element.
     * @param b_value Value of the right element.
     */
    void insert(uint16_t index, uint16_t array_size, dtype a_value,
                dtype b_value) {
        assert(index > 0);
        for (uint16_t i = array_size; i > index; i--) {
            elems_[i] = elems_[i - 1];
        }
        elems_[index - 1] = index != 1 ? elems_[index - 2] + a_value : a_value;
        elems_[index] = elems_[index - 1] + b_value;
    }

    /**
     * @brief Inserts a new value into the cumulative sums.
     * 
     * Intended for use when a new node is created after an old node by splitting. 
     * Thus `index` is greater than 0 and less than or equal to `array_size`.
     * 
     * Cumulative sums of only 1 element will be impacted since this is just a split.
     * 
     * @param index Position of new element.
     * @param array_size Number of elements currently in the array.
     * @param value Value of the new element.
     */
    void insert(uint16_t index, uint16_t array_size, dtype value) {
        assert(index > 0);
        for (uint16_t i = array_size; i >= index; i--) {
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
     * @param array_size Number of elements in the array.
     */
    void remove(uint16_t index, uint16_t array_size) {
        for (uint16_t i = index; i < array_size - 1; i++) {
            elems_[i] = elems_[i + 1];
        }
        elems_[array_size - 1] = (~dtype(0)) >> 1;
    }

    /**
     * @brief Removes the first `n` elements from the structure.
     *
     * Intended for use when rebalancing. A sibling has taken over some of the
     * children and this is called to remove the moved children from the current
     * node. Subsequent cumulative sums will be updated accordingly.
     *
     * @param n          Number of elements to remove.
     * @param array_size Number of elements stored.
     */
    void clear_first(uint16_t n, uint16_t array_size) {
        dtype subtrahend = elems_[n - 1];
        for (uint16_t i = n; i < array_size; i++) {
            elems_[i - n] = elems_[i] - subtrahend;
            elems_[i] = (~dtype(0)) >> 1;
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
        for (uint16_t i = array_size - n; i < array_size; i++) {
            elems_[i] = (~dtype(0)) >> 1;
        }
    }

    /**
     * @brief Adds elements from `other` to the end of `this`
     *
     * Intended for use when rebalancing. Adds `n_elemes` elements from `other`
     * to the end of "these" cumulative sums. Should be subsequently removed
     * from `other`
     *
     * @param n_elems    Number of elements to append.
     * @param array_size Number of elements stored in `this`.
     * @param other      Other cumulative size structure to copy from.
     */
    void append(uint16_t n_elems, uint16_t array_size, const branchless_scan* const other) {
        dtype addend = array_size != 0 ? elems_[array_size - 1] : 0;
        for (uint16_t i = 0; i < n_elems; i++) {
            elems_[i + array_size] = addend + other->get(i);
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
    }

    /**
     * @brief Adds elements form `other` to the start of `this`
     *
     * Intended for use when rebalancing. Adds `n_elemes` elements from `other`
     * to the start of "these" cumulative sums. Should be subsequently removed
     * from `other`
     *
     * @param n_elems    Number of elements to append.
     * @param array_size Number of elements stored in `this`.
     * @param o_size     Number of elements stored in `other`.
     * @param other      Other cumulative size structure to copy from.
     */
    void prepend(uint16_t n_elems, uint16_t array_size, uint16_t o_size,
                 const branchless_scan* const other) {
        memmove(elems_ + n_elems, elems_, array_size * sizeof(dtype));
        dtype subtrahend =
            n_elems < o_size ? other->get(o_size - n_elems - 1) : 0;
        for (uint16_t i = 0; i < n_elems; i++) {
            elems_[i] = other->get(i + o_size - n_elems) - subtrahend;
        }
        for (uint16_t i = n_elems; i < array_size + n_elems; i++) {
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
        constexpr dtype SIGN_BIT = ~((~dtype(0)) >> 1);
        constexpr dtype num_bits = sizeof(dtype) * 8;
        constexpr dtype lines = CACHE_LINE / sizeof(dtype);
        for (dtype i = 0; i < branches; i += lines) {
            __builtin_prefetch(elems_ + i);
        }
        uint16_t idx;
        if constexpr (branches == 2048) {
            idx = (uint16_t(1) << 10) - 1;
        }
        if constexpr (branches == 1024) {
            idx = (uint16_t(1) << 9) - 1;
        } else if constexpr (branches > 1024) {
            idx ^= (dtype((elems_[idx] - q) & SIGN_BIT) >> (num_bits - 11)) |
                   (uint16_t(1) << 9);
        }
        if constexpr (branches == 512) {
            idx = (uint16_t(1) << 8) - 1;
        } else if constexpr (branches > 512) {
            idx ^= (dtype((elems_[idx] - q) & SIGN_BIT) >> (num_bits - 10)) |
                   (uint16_t(1) << 8);
        }
        if constexpr (branches == 256) {
            idx = (uint16_t(1) << 7) - 1;
        } else if constexpr (branches > 256) {
            idx ^= (dtype((elems_[idx] - q) & SIGN_BIT) >> (num_bits - 9)) |
                   (uint16_t(1) << 7);
        }
        if constexpr (branches == 128) {
            idx = (uint16_t(1) << 6) - 1;
        } else if constexpr (branches > 128) {
            idx ^= (dtype((elems_[idx] - q) & SIGN_BIT) >> (num_bits - 8)) |
                   (uint16_t(1) << 6);
        }
            
        if constexpr (branches == 64) {
            idx = (uint16_t(1) << 5) - 1;
        } else if constexpr (branches > 64) {
            idx ^= (dtype((elems_[idx] - q) & SIGN_BIT) >> (num_bits - 7)) |
                   (uint16_t(1) << 5);
        }
            
        if constexpr (branches == 32) {
            idx = (uint16_t(1) << 4) - 1;
        } else if constexpr (branches > 32) {
            idx ^= (dtype((elems_[idx] - q) & SIGN_BIT) >> (num_bits - 6)) |
                   (uint16_t(1) << 4);
        }
        if constexpr (branches == 16) {
            idx = (uint16_t(1) << 3) - 1;
        } else if constexpr (branches > 16) {
            idx ^= (dtype((elems_[idx] - q) & SIGN_BIT) >> (num_bits - 5)) |
                   (uint16_t(1) << 3);
        }
        
        if constexpr (branches == 8) {
            idx = (uint16_t(1) << 2) - 1;
        } else {
            idx ^= (dtype((elems_[idx] - q) & SIGN_BIT) >> (num_bits - 4)) |
                   (uint16_t(1) << 2);
        }

        idx ^= (dtype((elems_[idx] - q) & SIGN_BIT) >> (num_bits - 3)) |
               (uint16_t(1) << 1);
        idx ^= (dtype((elems_[idx] - q) & SIGN_BIT) >> (num_bits - 2)) |
               uint16_t(1);
        return idx ^ (dtype((elems_[idx] - q) & SIGN_BIT) >> (num_bits - 1));
    }
};

}  // namespace bv

#endif