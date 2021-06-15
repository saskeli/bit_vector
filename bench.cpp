#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>

#include "bit_vector/bv.hpp"
#include "deps/DYNAMIC/include/dynamic/dynamic.hpp"
#include "deps/valgrind/callgrind.h"

typedef bv::malloc_alloc alloc;
typedef bv::simple_bv<8, 16384, 64> bit_vector;
typedef bv::small_bv<8, 16384, 64> s_bit_vector;

template <class bva, class bvb>
void check(bva* a, bvb* b, uint64_t size) {
    if (a->size() != size) {
        std::cerr << "Invalid size for structure a" << std::endl;
        exit(1);
    }
    if (b->size() != size) {
        std::cerr << "Invalid size for structure b" << std::endl;
        exit(1);
    }

    for (uint64_t i = 0; i < size; i++) {
        if (a->at(i) != b->at(i)) {
            std::cerr << "Invalid acces comparison at " << i << std::endl;
            exit(1);
        }
        if (a->rank(i) != b->rank(i)) {
            std::cerr << "Invalid rank comparison at " << i << std::endl;
            exit(1);
        }
    }

    uint64_t ones = a->rank(size);
    if (a->rank(size) != b->rank(size)) {
        std::cerr << "incompatible number of ones" << std::endl;
        exit(1);
    }

    for (uint64_t i = 0; i < ones; i++) {
        if (a->select(i + 1) != b->select(i)) {
            std::cerr << "Invalid select comparison at " << i << std::endl;
            exit(1);
        }
    }
}

int main() {
    uint64_t size = 1000000000;
    uint64_t steps = 100;
    bit_vector bv;
    s_bit_vector cbv;

    std::random_device rd;
    uint64_t seed = rd();
    std::mt19937 mt(seed);
    std::uniform_int_distribution<unsigned long long> gen(
        std::numeric_limits<std::uint64_t>::min(),
        std::numeric_limits<std::uint64_t>::max());

    using std::chrono::duration_cast;
    using std::chrono::high_resolution_clock;
    using std::chrono::microseconds;

    uint64_t start_size = 1000000;

    double startexp = log2(double(start_size));
    double delta = (log2(double(size)) - log2(double(start_size))) / steps;
    uint64_t ops = 100000;
    std::cerr << "startexp: " << startexp << ". delta: " << delta;
    std::cerr << "\nWith seed: " << seed << std::endl;
    std::vector<uint64_t> loc, val;

    std::cout << "Size\tremove\tc_remove\tinsert\tc_insert\taccess\tc_"
                 "access\trank\tc_rank\tselect\tc_select\tsize(bits)\tc_size("
                 "bits)\tchecksum"
              << std::endl;

    for (uint64_t i = 0; i < 900000; i++) {
        uint64_t aloc = gen(mt) % (i + 1);
        bool aval = gen(mt) % 2;
        bv.insert(aloc, aval);
        cbv.insert(aloc, aval);
    }
    for (uint64_t step = 1; step <= steps; step++) {
        uint64_t start = bv.size();
        uint64_t target = uint64_t(pow(2.0, startexp + delta * step));

        uint64_t checksum = 0;

        std::cout << target << "\t";

        for (size_t i = start; i < target; i++) {
            uint64_t aloc = gen(mt) % (i + 1);
            bool aval = gen(mt) % 2;
            bv.insert(aloc, aval);
            cbv.insert(aloc, aval);
        }

        loc.clear();
        for (uint64_t i = target; i > target - ops; i--) {
            loc.push_back(gen(mt) % i);
        }

        auto t1 = high_resolution_clock::now();
        for (size_t i = 0; i < ops; i++) {
            bv.remove(loc[i]);
        }
        auto t2 = high_resolution_clock::now();
        std::cout << (double)duration_cast<microseconds>(t2 - t1).count() / ops
                  << "\t";

        t1 = high_resolution_clock::now();
        for (size_t i = 0; i < ops; i++) {
            cbv.remove(loc[i]);
        }
        t2 = high_resolution_clock::now();
        std::cout << (double)duration_cast<microseconds>(t2 - t1).count() / ops
                  << "\t";

        loc.clear();
        val.clear();
        for (uint64_t i = bv.size(); i < target; i++) {
            loc.push_back(gen(mt) % (i + 1));
            val.push_back(gen(mt) % 2);
        }

        t1 = high_resolution_clock::now();
        for (size_t i = 0; i < loc.size(); i++) {
            bv.insert(loc[i], val[i]);
        }
        t2 = high_resolution_clock::now();
        std::cout << (double)duration_cast<microseconds>(t2 - t1).count() / ops
                  << "\t";

        t1 = high_resolution_clock::now();
        for (size_t i = 0; i < loc.size(); i++) {
            cbv.insert(loc[i], val[i]);
        }
        t2 = high_resolution_clock::now();
        std::cout << (double)duration_cast<microseconds>(t2 - t1).count() / ops
                  << "\t";

        loc.clear();
        for (size_t i = 0; i < ops; i++) {
            loc.push_back(gen(mt) % target);
        }

        t1 = high_resolution_clock::now();
        for (size_t i = 0; i < ops; i++) {
            checksum += bv.at(loc[i]);
        }
        t2 = high_resolution_clock::now();
        std::cout << (double)duration_cast<microseconds>(t2 - t1).count() / ops
                  << "\t";

        t1 = high_resolution_clock::now();
        for (size_t i = 0; i < ops; i++) {
            checksum -= cbv.at(loc[i]);
        }
        t2 = high_resolution_clock::now();
        std::cout << (double)duration_cast<microseconds>(t2 - t1).count() / ops
                  << "\t";

        loc.clear();
        for (size_t i = 0; i < ops; i++) {
            loc.push_back(gen(mt) % target);
        }

        t1 = high_resolution_clock::now();
        for (size_t i = 0; i < ops; i++) {
            checksum += bv.rank(loc[i]);
        }
        t2 = high_resolution_clock::now();
        std::cout << (double)duration_cast<microseconds>(t2 - t1).count() / ops
                  << "\t";

        t1 = high_resolution_clock::now();
        for (size_t i = 0; i < ops; i++) {
            checksum -= cbv.rank(loc[i]);
        }
        t2 = high_resolution_clock::now();
        std::cout << (double)duration_cast<microseconds>(t2 - t1).count() / ops
                  << "\t";

        uint64_t limit = bv.rank(target - 1);
        assert(limit == cbv.rank(target - 1));
        loc.clear();
        for (size_t i = 0; i < ops; i++) {
            loc.push_back(gen(mt) % limit);
        }

        t1 = high_resolution_clock::now();
        for (size_t i = 0; i < ops; i++) {
            checksum += bv.select(loc[i] + 1);
        }
        t2 = high_resolution_clock::now();
        std::cout << (double)duration_cast<microseconds>(t2 - t1).count() / ops
                  << "\t";

        t1 = high_resolution_clock::now();
        for (size_t i = 0; i < ops; i++) {
            checksum -= cbv.select(loc[i] + 1);
        }
        t2 = high_resolution_clock::now();
        std::cout << (double)duration_cast<microseconds>(t2 - t1).count() / ops
                  << "\t";

        std::cout << bv.bit_size() << "\t";
        std::cout << cbv.bit_size() << "\t";

        std::cout << checksum << std::endl;
    }

    /*uint64_t tot_size = bv.size();
    auto t1 = high_resolution_clock::now();
    check<bit_vector, dyn::suc_bv>(&bv, &cbv, tot_size);
    auto t2 = high_resolution_clock::now();
    std::cerr << "Data structure validation finished in "
              << (double)duration_cast<microseconds>(t2 - t1).count() << " us."
              << std::endl;*/
}