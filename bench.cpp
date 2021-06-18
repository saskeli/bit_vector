#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>

#include "bit_vector/bv.hpp"
#include "deps/DYNAMIC/include/dynamic/dynamic.hpp"
#include "deps/valgrind/callgrind.h"

typedef bv::simple_bv<8, 16384, 64> a_vec;
typedef bv::small_bv<8, 16384, 64> b_vec;
typedef bv::simple_bv<0, 16384, 64> c_vec;
typedef bv::small_bv<0, 16384, 64> d_vec;
typedef dyn::suc_bv e_vec;
typedef dyn::b_suc_bv f_vec;
typedef dyn::ub_suc_bv g_vec;

void help() {
    std::cout << "Benchmark some dynamic bit vectors.\n"
              << "Type and seed is required.\n"
              << "Size should be at least 10^7 and defaults to 10^7\n"
              << "Steps defaults to 100.\n\n";
    std::cout << "Usage: bench <1|2|3|4|5|6|7> <seed> <size> <steps>\n";
    std::cout << "   1        64-bit indexed bv with buffer size 8\n";
    std::cout << "   2        32-bit indexed bv with buffer size 8\n";
    std::cout << "   3        64-bit indexed bv with buffer size 0\n";
    std::cout << "   4        32-bit indexed bv with buffer size 0\n";
    std::cout << "   5        suc_bv from the DYNAMIC library\n";
    std::cout << "   6        based on suc_bv with buffer size 8\n";
    std::cout << "   7        based on suc_bv with buffer size 0\n";
    std::cout << "   <seed>   seed to use for running the test\n";
    std::cout << "   <size>   number of bits in the bitvector\n";
    std::cout << "   <steps>  How many data points to generate in the "
                 "[10^6..size] range\n\n";
    std::cout << "Example: benchmark 1 1337 1000000 100" << std::endl;

    exit(0);
}

template <class bit_vector>
void test(uint64_t size, uint64_t steps, uint64_t seed, uint64_t select_offset, uint64_t bv_type) {
    bit_vector bv;

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

    std::cout << "type\tsize\tremove\t\tinsert\t\taccess\t\trank\t\tselect\t\tsize(bits)\t\tchecksum"
              << std::endl;

    for (uint64_t i = 0; i < 900000; i++) {
        uint64_t aloc = gen(mt) % (i + 1);
        bool aval = gen(mt) % 2;
        bv.insert(aloc, aval);
    }
    for (uint64_t step = 1; step <= steps; step++) {
        uint64_t start = bv.size();
        uint64_t target = uint64_t(pow(2.0, startexp + delta * step));

        uint64_t checksum = 0;

        std::cout << bv_type << "\t" << target << "\t";

        for (size_t i = start; i < target; i++) {
            uint64_t aloc = gen(mt) % (i + 1);
            bool aval = gen(mt) % 2;
            bv.insert(aloc, aval);
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

        uint64_t limit = bv.rank(target - 1);
        loc.clear();
        for (size_t i = 0; i < ops; i++) {
            loc.push_back(gen(mt) % limit);
        }

        t1 = high_resolution_clock::now();
        for (size_t i = 0; i < ops; i++) {
            checksum += bv.select(loc[i] + select_offset);
        }
        t2 = high_resolution_clock::now();
        std::cout << (double)duration_cast<microseconds>(t2 - t1).count() / ops
                  << "\t";

        std::cout << bv.bit_size() << "\t";

        std::cout << checksum << std::endl;
    }
}

int main(int argc, char const *argv[]) {
    if (argc < 3) {
        help();
        return 0;
    }
    uint64_t type;
    uint64_t seed;
    uint64_t size = 10000000;
    uint64_t steps = 100;
    std::sscanf(argv[1], "%lu", &type);
    if (type < 1 || type > 7) {
        std::cerr << "Invalid type argument" << std::endl;
        help();
        return 1;
    }
    std::sscanf(argv[2], "%lu", &seed);
    if (argc > 3) {
        std::sscanf(argv[3], "%lu", &size);
        if (size < 10000000) {
            std::cerr << "Invalid size argument" << std::endl;
            help();
            return 1;
        }
    }
    if (argc > 4) {
        std::sscanf(argv[4], "%lu", &steps);
    }
    switch (type) {
    case 1:
        test<a_vec>(size, steps, seed, 1, type);
        break;
    case 2:
        test<b_vec>(size, steps, seed, 1, type);
        break;
    case 3:
        test<c_vec>(size, steps, seed, 1, type);
        break;
    case 4:
        test<d_vec>(size, steps, seed, 1, type);
        break;
    case 5:
        test<e_vec>(size, steps, seed, 0, type);
        break;
    case 6:
        test<f_vec>(size, steps, seed, 0, type);
        break;
    default:
        test<g_vec>(size, steps, seed, 0, type);
        break;
    }
    return 0;
}
