#ifndef BV_BRANCH_SELECTION_HPP
#define BV_BRANCH_SELECTION_HPP

#include <cassert>
#include <cstdint>
#include <cstring>

#ifndef CACHE_LINE
// Apparently the most common cache line size is 64.
#define CACHE_LINE 64
#endif

namespace bv {

template <class dtype, dtype branches>
class branchless_scan {
   protected:
    dtype elems_[branches];

    static_assert((branches == 8) || (branches == 16) || (branches == 32) ||
                      (branches == 64) || (branches == 128),
                  "branching factor needs to be a reasonable power of 2");

   public:
    branchless_scan() {
        for (dtype i = 0; i < branches; i++) {
            elems_[i] = (~dtype(0)) >> 1;
        }
    }

    dtype& operator[](size_t idx) { return elems_[idx]; }
    const dtype& operator[](size_t idx) const { return elems_[idx]; }

    dtype get(uint8_t index) const { return elems_[index]; }

    void set(uint8_t index, dtype value) { elems_[index] = value; }

    template <class T>
    void increment(uint8_t from, uint8_t to, T change) {
        for (uint8_t i = from; i < to; i++) {
            elems_[i] += change;
        }
    }

    void clear_first(uint8_t n, uint8_t array_size) {
        dtype subtrahend = elems_[n - 1];
        for (uint8_t i = n; i < array_size; i++) {
            elems_[i - n] = elems_[i] - subtrahend;
            elems_[i] = (~dtype(0)) >> 1;
        }
    }

    void clear_last(uint8_t n, uint8_t array_size) {
        for (uint8_t i = array_size - n; i < array_size; i++) {
            elems_[i] = (~dtype(0)) >> 1;
        }
    }

    void append(uint8_t n_elems, uint8_t index, branchless_scan* other) {
        dtype addend = index != 0 ? elems_[index - 1] : 0;
        for (uint8_t i = 0; i < n_elems; i++) {
            elems_[i + index] = addend + other->get(i);
        }
    }

    void prepend(uint8_t n_elems, uint8_t array_size, uint8_t o_size,
                 branchless_scan* other) {
        memmove(elems_ + n_elems, elems_, array_size * sizeof(dtype));
        dtype subtrahend = n_elems < o_size ? other->get(o_size - n_elems - 1) : 0;
        for (uint8_t i = 0; i < n_elems; i++) {
            elems_[i] = other->get(i + o_size - n_elems) - subtrahend;
        }
        for (uint8_t i = n_elems; i < array_size + n_elems; i++) {
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
    uint8_t find(dtype q) const {
        constexpr dtype SIGN_BIT = ~((~dtype(0)) >> 1);
        constexpr dtype num_bits = sizeof(dtype) * 8;
        constexpr dtype lines = CACHE_LINE / sizeof(dtype);
        for (dtype i = 0; i < branches; i += lines) {
            __builtin_prefetch(elems_ + i);
        }
        uint8_t idx;
        if constexpr (branches == 128) {
            idx = (uint8_t(1) << 6) - 1;
            idx ^= (dtype((elems_[idx] - q) & SIGN_BIT) >> (num_bits - 7)) |
                   (uint8_t(1) << 5);
            idx ^= (dtype((elems_[idx] - q) & SIGN_BIT) >> (num_bits - 6)) |
                   (uint8_t(1) << 4);
            idx ^= (dtype((elems_[idx] - q) & SIGN_BIT) >> (num_bits - 5)) |
                   (uint8_t(1) << 3);
            idx ^= (dtype((elems_[idx] - q) & SIGN_BIT) >> (num_bits - 4)) |
                   (uint8_t(1) << 2);
        } else if constexpr (branches == 64) {
            idx = (uint8_t(1) << 5) - 1;
            idx ^= (dtype((elems_[idx] - q) & SIGN_BIT) >> (num_bits - 6)) |
                   (uint8_t(1) << 4);
            idx ^= (dtype((elems_[idx] - q) & SIGN_BIT) >> (num_bits - 5)) |
                   (uint8_t(1) << 3);
            idx ^= (dtype((elems_[idx] - q) & SIGN_BIT) >> (num_bits - 4)) |
                   (uint8_t(1) << 2);
        } else if constexpr (branches == 32) {
            idx = (uint8_t(1) << 4) - 1;
            idx ^= (dtype((elems_[idx] - q) & SIGN_BIT) >> (num_bits - 5)) |
                   (uint8_t(1) << 3);
            idx ^= (dtype((elems_[idx] - q) & SIGN_BIT) >> (num_bits - 4)) |
                   (uint8_t(1) << 2);
        } else if constexpr (branches == 16) {
            idx = (uint8_t(1) << 3) - 1;
            idx ^= (dtype((elems_[idx] - q) & SIGN_BIT) >> (num_bits - 4)) |
                   (uint8_t(1) << 2);
        } else {
            idx = (uint8_t(1) << 2) - 1;
        }
        idx ^= (dtype((elems_[idx] - q) & SIGN_BIT) >> (num_bits - 3)) |
               (uint8_t(1) << 1);
        idx ^= (dtype((elems_[idx] - q) & SIGN_BIT) >> (num_bits - 2)) |
               uint8_t(1);
        return idx ^ (dtype((elems_[idx] - q) & SIGN_BIT) >> (num_bits - 1));
    }
};

}  // namespace bv

#endif