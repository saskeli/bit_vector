#include <sys/resource.h>

#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>

#include "bit_vector/bv.hpp"
#include "deps/DYNAMIC/include/dynamic/dynamic.hpp"
#include "deps/valgrind/callgrind.h"

void help() {
    std::cout << "Benchmark some dynamic bit vectors.\n"
              << "Type and seed is required.\n"
              << "Size should be at least 10^7 and defaults to 10^7\n"
              << "Steps defaults to 100.\n\n";
    std::cout << "Usage: bench <1-130> <seed> <size> <steps>\n";
    std::cout << "   number   Specifies data structure to use\n";
    std::cout << "   <seed>   seed to use for running the test\n";
    std::cout << "   <size>   number of bits in the bitvector\n";
    std::cout << "   <steps>  How many data points to generate in the "
                 "[10^6..size] range\n\n";
    std::cout << "Example: bench 1 1337 1000000 100" << std::endl;

    exit(0);
}

template <class bit_vector, uint64_t select_offset, uint8_t type,
          uint8_t buffer, uint8_t branch, uint32_t leaf_size>
void test(uint64_t size, uint64_t steps, uint64_t seed) {
    bit_vector bv;

    std::mt19937 mt(seed);
    std::uniform_int_distribution<unsigned long long> gen(
        std::numeric_limits<std::uint64_t>::min(),
        std::numeric_limits<std::uint64_t>::max());

    using std::chrono::duration_cast;
    using std::chrono::high_resolution_clock;
    using std::chrono::microseconds;

    struct rusage ru;

    uint64_t start_size = 1000000;

    double startexp = log2(double(start_size));
    double delta = (log2(double(size)) - log2(double(start_size))) / steps;
    uint64_t ops = 100000;
    std::cerr << "startexp: " << startexp << ". delta: " << delta << std::endl;
    std::vector<uint64_t> loc, val;

    std::cout << "type\tbuffer\tbranch\tleaf_size\tsize\tremove\tinsert\tset\t"
              << "access\trank\tselect\tsize(bits)\trss\tchecksum" << std::endl;

    for (uint64_t i = 0; i < 900000; i++) {
        uint64_t aloc = gen(mt) % (i + 1);
        bool aval = gen(mt) % 2;
        bv.insert(aloc, aval);
    }
    for (uint64_t step = 1; step <= steps; step++) {
        uint64_t start = bv.size();
        uint64_t target = uint64_t(pow(2.0, startexp + delta * step));

        uint64_t checksum = 0;
        if constexpr (type == 0) {
            std::cout << "DYNAMIC\t";
        } else if constexpr (type == 1) {
            std::cout << "leaf\t";
        } else if constexpr (type == 2) {
            std::cout << "uint32_t\t";
        } else {
            std::cout << "uint64_t\t";
        }
        std::cout << int(buffer) << "\t" << int(branch) << "\t" << leaf_size << "\t"
                  << target << "\t";

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
        val.clear();
        for (uint64_t i = 0; i < ops; i++) {
            loc.push_back(gen(mt) % target);
            val.push_back(gen(mt) % 2);
        }

        t1 = high_resolution_clock::now();
        for (size_t i = 0; i < loc.size(); i++) {
            bv.set(loc[i], val[i]);
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
        getrusage(RUSAGE_SELF, &ru);
        std::cout << ru.ru_maxrss * 8 * 1024 << "\t";

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
            std::cerr << "1: DYNAMIC, 16, 0, 4096" << std::endl;
            test<dyn::suc_bv, 0, 0, 0, 16, 8192>(size, steps, seed);
            break;
        case 2:
            std::cerr << "2: leaf, 16, 0, 4096" << std::endl;
            test<dyn::ub_suc_bv, 0, 1, 0, 16, 8192>(size, steps, seed);
            break;
        case 3:
            std::cerr << "2: leaf, 16, 0, 4096" << std::endl;
            test<dyn::b_suc_bv, 0, 1, 8, 16, 8192>(size, steps, seed);
            break;
        case 4:
            std::cerr << "4: uint32_t, 16, 0, 4096" << std::endl;
            test<bv::small_bv<0, 4096, 16>, 1, 2, 0, 16, 4096>(size, steps,
                                                               seed);
            break;
        case 5:
            std::cerr << "5: uint32_t, 16, 0, 8192" << std::endl;
            test<bv::small_bv<0, 8192, 16>, 1, 2, 0, 16, 8192>(size, steps,
                                                               seed);
            break;
        case 6:
            std::cerr << "6: uint32_t, 16, 0, 16384" << std::endl;
            test<bv::small_bv<0, 16384, 16>, 1, 2, 0, 16, 16384>(size, steps,
                                                                 seed);
            break;
        case 7:
            std::cerr << "7: uint32_t, 16, 0, 32768" << std::endl;
            test<bv::small_bv<0, 32768, 16>, 1, 2, 0, 16, 32768>(size, steps,
                                                                 seed);
            break;
        case 8:
            std::cerr << "8: uint32_t, 16, 8, 4096" << std::endl;
            test<bv::small_bv<8, 4096, 16>, 1, 2, 8, 16, 4096>(size, steps,
                                                               seed);
            break;
        case 9:
            std::cerr << "9: uint32_t, 16, 8, 8192" << std::endl;
            test<bv::small_bv<8, 8192, 16>, 1, 2, 8, 16, 8192>(size, steps,
                                                               seed);
            break;
        case 10:
            std::cerr << "10: uint32_t, 16, 8, 16384" << std::endl;
            test<bv::small_bv<8, 16384, 16>, 1, 2, 8, 16, 16384>(size, steps,
                                                                 seed);
            break;
        case 11:
            std::cerr << "11: uint32_t, 16, 8, 32768" << std::endl;
            test<bv::small_bv<8, 32768, 16>, 1, 2, 8, 16, 32768>(size, steps,
                                                                 seed);
            break;
        case 12:
            std::cerr << "12: uint32_t, 16, 16, 4096" << std::endl;
            test<bv::small_bv<16, 4096, 16>, 1, 2, 16, 16, 4096>(size, steps,
                                                                 seed);
            break;
        case 13:
            std::cerr << "13: uint32_t, 16, 16, 8192" << std::endl;
            test<bv::small_bv<16, 8192, 16>, 1, 2, 16, 16, 8192>(size, steps,
                                                                 seed);
            break;
        case 14:
            std::cerr << "14: uint32_t, 16, 16, 16384" << std::endl;
            test<bv::small_bv<16, 16384, 16>, 1, 2, 16, 16, 16384>(size, steps,
                                                                   seed);
            break;
        case 15:
            std::cerr << "15: uint32_t, 16, 16, 32768" << std::endl;
            test<bv::small_bv<16, 32768, 16>, 1, 2, 16, 16, 32768>(size, steps,
                                                                   seed);
            break;
        case 16:
            std::cerr << "16: uint32_t, 16, 32, 4096" << std::endl;
            test<bv::small_bv<32, 4096, 16>, 1, 2, 32, 16, 4096>(size, steps,
                                                                 seed);
            break;
        case 17:
            std::cerr << "17: uint32_t, 16, 32, 8192" << std::endl;
            test<bv::small_bv<32, 8192, 16>, 1, 2, 32, 16, 8192>(size, steps,
                                                                 seed);
            break;
        case 18:
            std::cerr << "18: uint32_t, 16, 32, 16384" << std::endl;
            test<bv::small_bv<32, 16384, 16>, 1, 2, 32, 16, 16384>(size, steps,
                                                                   seed);
            break;
        case 19:
            std::cerr << "19: uint32_t, 16, 32, 32768" << std::endl;
            test<bv::small_bv<32, 32768, 16>, 1, 2, 32, 16, 32768>(size, steps,
                                                                   seed);
            break;
        case 20:
            std::cerr << "20: uint32_t, 32, 0, 4096" << std::endl;
            test<bv::small_bv<0, 4096, 32>, 1, 2, 0, 32, 4096>(size, steps,
                                                               seed);
            break;
        case 21:
            std::cerr << "21: uint32_t, 32, 0, 8192" << std::endl;
            test<bv::small_bv<0, 8192, 32>, 1, 2, 0, 32, 8192>(size, steps,
                                                               seed);
            break;
        case 22:
            std::cerr << "22: uint32_t, 32, 0, 16384" << std::endl;
            test<bv::small_bv<0, 16384, 32>, 1, 2, 0, 32, 16384>(size, steps,
                                                                 seed);
            break;
        case 23:
            std::cerr << "23: uint32_t, 32, 0, 32768" << std::endl;
            test<bv::small_bv<0, 32768, 32>, 1, 2, 0, 32, 32768>(size, steps,
                                                                 seed);
            break;
        case 24:
            std::cerr << "24: uint32_t, 32, 8, 4096" << std::endl;
            test<bv::small_bv<8, 4096, 32>, 1, 2, 8, 32, 4096>(size, steps,
                                                               seed);
            break;
        case 25:
            std::cerr << "25: uint32_t, 32, 8, 8192" << std::endl;
            test<bv::small_bv<8, 8192, 32>, 1, 2, 8, 32, 8192>(size, steps,
                                                               seed);
            break;
        case 26:
            std::cerr << "26: uint32_t, 32, 8, 16384" << std::endl;
            test<bv::small_bv<8, 16384, 32>, 1, 2, 8, 32, 16384>(size, steps,
                                                                 seed);
            break;
        case 27:
            std::cerr << "27: uint32_t, 32, 8, 32768" << std::endl;
            test<bv::small_bv<8, 32768, 32>, 1, 2, 8, 32, 32768>(size, steps,
                                                                 seed);
            break;
        case 28:
            std::cerr << "28: uint32_t, 32, 16, 4096" << std::endl;
            test<bv::small_bv<16, 4096, 32>, 1, 2, 16, 32, 4096>(size, steps,
                                                                 seed);
            break;
        case 29:
            std::cerr << "29: uint32_t, 32, 16, 8192" << std::endl;
            test<bv::small_bv<16, 8192, 32>, 1, 2, 16, 32, 8192>(size, steps,
                                                                 seed);
            break;
        case 30:
            std::cerr << "30: uint32_t, 32, 16, 16384" << std::endl;
            test<bv::small_bv<16, 16384, 32>, 1, 2, 16, 32, 16384>(size, steps,
                                                                   seed);
            break;
        case 31:
            std::cerr << "31: uint32_t, 32, 16, 32768" << std::endl;
            test<bv::small_bv<16, 32768, 32>, 1, 2, 16, 32, 32768>(size, steps,
                                                                   seed);
            break;
        case 32:
            std::cerr << "32: uint32_t, 32, 32, 4096" << std::endl;
            test<bv::small_bv<32, 4096, 32>, 1, 2, 32, 32, 4096>(size, steps,
                                                                 seed);
            break;
        case 33:
            std::cerr << "33: uint32_t, 32, 32, 8192" << std::endl;
            test<bv::small_bv<32, 8192, 32>, 1, 2, 32, 32, 8192>(size, steps,
                                                                 seed);
            break;
        case 34:
            std::cerr << "34: uint32_t, 32, 32, 16384" << std::endl;
            test<bv::small_bv<32, 16384, 32>, 1, 2, 32, 32, 16384>(size, steps,
                                                                   seed);
            break;
        case 35:
            std::cerr << "35: uint32_t, 32, 32, 32768" << std::endl;
            test<bv::small_bv<32, 32768, 32>, 1, 2, 32, 32, 32768>(size, steps,
                                                                   seed);
            break;
        case 36:
            std::cerr << "36: uint32_t, 64, 0, 4096" << std::endl;
            test<bv::small_bv<0, 4096, 64>, 1, 2, 0, 64, 4096>(size, steps,
                                                               seed);
            break;
        case 37:
            std::cerr << "37: uint32_t, 64, 0, 8192" << std::endl;
            test<bv::small_bv<0, 8192, 64>, 1, 2, 0, 64, 8192>(size, steps,
                                                               seed);
            break;
        case 38:
            std::cerr << "38: uint32_t, 64, 0, 16384" << std::endl;
            test<bv::small_bv<0, 16384, 64>, 1, 2, 0, 64, 16384>(size, steps,
                                                                 seed);
            break;
        case 39:
            std::cerr << "39: uint32_t, 64, 0, 32768" << std::endl;
            test<bv::small_bv<0, 32768, 64>, 1, 2, 0, 64, 32768>(size, steps,
                                                                 seed);
            break;
        case 40:
            std::cerr << "40: uint32_t, 64, 8, 4096" << std::endl;
            test<bv::small_bv<8, 4096, 64>, 1, 2, 8, 64, 4096>(size, steps,
                                                               seed);
            break;
        case 41:
            std::cerr << "41: uint32_t, 64, 8, 8192" << std::endl;
            test<bv::small_bv<8, 8192, 64>, 1, 2, 8, 64, 8192>(size, steps,
                                                               seed);
            break;
        case 42:
            std::cerr << "42: uint32_t, 64, 8, 16384" << std::endl;
            test<bv::small_bv<8, 16384, 64>, 1, 2, 8, 64, 16384>(size, steps,
                                                                 seed);
            break;
        case 43:
            std::cerr << "43: uint32_t, 64, 8, 32768" << std::endl;
            test<bv::small_bv<8, 32768, 64>, 1, 2, 8, 64, 32768>(size, steps,
                                                                 seed);
            break;
        case 44:
            std::cerr << "44: uint32_t, 64, 16, 4096" << std::endl;
            test<bv::small_bv<16, 4096, 64>, 1, 2, 16, 64, 4096>(size, steps,
                                                                 seed);
            break;
        case 45:
            std::cerr << "45: uint32_t, 64, 16, 8192" << std::endl;
            test<bv::small_bv<16, 8192, 64>, 1, 2, 16, 64, 8192>(size, steps,
                                                                 seed);
            break;
        case 46:
            std::cerr << "46: uint32_t, 64, 16, 16384" << std::endl;
            test<bv::small_bv<16, 16384, 64>, 1, 2, 16, 64, 16384>(size, steps,
                                                                   seed);
            break;
        case 47:
            std::cerr << "47: uint32_t, 64, 16, 32768" << std::endl;
            test<bv::small_bv<16, 32768, 64>, 1, 2, 16, 64, 32768>(size, steps,
                                                                   seed);
            break;
        case 48:
            std::cerr << "48: uint32_t, 64, 32, 4096" << std::endl;
            test<bv::small_bv<32, 4096, 64>, 1, 2, 32, 64, 4096>(size, steps,
                                                                 seed);
            break;
        case 49:
            std::cerr << "49: uint32_t, 64, 32, 8192" << std::endl;
            test<bv::small_bv<32, 8192, 64>, 1, 2, 32, 64, 8192>(size, steps,
                                                                 seed);
            break;
        case 50:
            std::cerr << "50: uint32_t, 64, 32, 16384" << std::endl;
            test<bv::small_bv<32, 16384, 64>, 1, 2, 32, 64, 16384>(size, steps,
                                                                   seed);
            break;
        case 51:
            std::cerr << "51: uint32_t, 64, 32, 32768" << std::endl;
            test<bv::small_bv<32, 32768, 64>, 1, 2, 32, 64, 32768>(size, steps,
                                                                   seed);
            break;
        case 52:
            std::cerr << "52: uint32_t, 128, 0, 4096" << std::endl;
            test<bv::small_bv<0, 4096, 128>, 1, 2, 0, 128, 4096>(size, steps,
                                                                 seed);
            break;
        case 53:
            std::cerr << "53: uint32_t, 128, 0, 8192" << std::endl;
            test<bv::small_bv<0, 8192, 128>, 1, 2, 0, 128, 8192>(size, steps,
                                                                 seed);
            break;
        case 54:
            std::cerr << "54: uint32_t, 128, 0, 16384" << std::endl;
            test<bv::small_bv<0, 16384, 128>, 1, 2, 0, 128, 16384>(size, steps,
                                                                   seed);
            break;
        case 55:
            std::cerr << "55: uint32_t, 128, 0, 32768" << std::endl;
            test<bv::small_bv<0, 32768, 128>, 1, 2, 0, 128, 32768>(size, steps,
                                                                   seed);
            break;
        case 56:
            std::cerr << "56: uint32_t, 128, 8, 4096" << std::endl;
            test<bv::small_bv<8, 4096, 128>, 1, 2, 8, 128, 4096>(size, steps,
                                                                 seed);
            break;
        case 57:
            std::cerr << "57: uint32_t, 128, 8, 8192" << std::endl;
            test<bv::small_bv<8, 8192, 128>, 1, 2, 8, 128, 8192>(size, steps,
                                                                 seed);
            break;
        case 58:
            std::cerr << "58: uint32_t, 128, 8, 16384" << std::endl;
            test<bv::small_bv<8, 16384, 128>, 1, 2, 8, 128, 16384>(size, steps,
                                                                   seed);
            break;
        case 59:
            std::cerr << "59: uint32_t, 128, 8, 32768" << std::endl;
            test<bv::small_bv<8, 32768, 128>, 1, 2, 8, 128, 32768>(size, steps,
                                                                   seed);
            break;
        case 60:
            std::cerr << "60: uint32_t, 128, 16, 4096" << std::endl;
            test<bv::small_bv<16, 4096, 128>, 1, 2, 16, 128, 4096>(size, steps,
                                                                   seed);
            break;
        case 61:
            std::cerr << "61: uint32_t, 128, 16, 8192" << std::endl;
            test<bv::small_bv<16, 8192, 128>, 1, 2, 16, 128, 8192>(size, steps,
                                                                   seed);
            break;
        case 62:
            std::cerr << "62: uint32_t, 128, 16, 16384" << std::endl;
            test<bv::small_bv<16, 16384, 128>, 1, 2, 16, 128, 16384>(
                size, steps, seed);
            break;
        case 63:
            std::cerr << "63: uint32_t, 128, 16, 32768" << std::endl;
            test<bv::small_bv<16, 32768, 128>, 1, 2, 16, 128, 32768>(
                size, steps, seed);
            break;
        case 64:
            std::cerr << "64: uint32_t, 128, 32, 4096" << std::endl;
            test<bv::small_bv<32, 4096, 128>, 1, 2, 32, 128, 4096>(size, steps,
                                                                   seed);
            break;
        case 65:
            std::cerr << "65: uint32_t, 128, 32, 8192" << std::endl;
            test<bv::small_bv<32, 8192, 128>, 1, 2, 32, 128, 8192>(size, steps,
                                                                   seed);
            break;
        case 66:
            std::cerr << "66: uint32_t, 128, 32, 16384" << std::endl;
            test<bv::small_bv<32, 16384, 128>, 1, 2, 32, 128, 16384>(
                size, steps, seed);
            break;
        case 67:
            std::cerr << "67: uint32_t, 128, 32, 32768" << std::endl;
            test<bv::small_bv<32, 32768, 128>, 1, 2, 32, 128, 32768>(
                size, steps, seed);
            break;
        case 68:
            std::cerr << "68: uint64_t, 16, 0, 4096" << std::endl;
            test<bv::simple_bv<0, 4096, 16>, 1, 3, 0, 16, 4096>(size, steps,
                                                                seed);
            break;
        case 69:
            std::cerr << "69: uint64_t, 16, 0, 8192" << std::endl;
            test<bv::simple_bv<0, 8192, 16>, 1, 3, 0, 16, 8192>(size, steps,
                                                                seed);
            break;
        case 70:
            std::cerr << "70: uint64_t, 16, 0, 16384" << std::endl;
            test<bv::simple_bv<0, 16384, 16>, 1, 3, 0, 16, 16384>(size, steps,
                                                                  seed);
            break;
        case 71:
            std::cerr << "71: uint64_t, 16, 0, 32768" << std::endl;
            test<bv::simple_bv<0, 32768, 16>, 1, 3, 0, 16, 32768>(size, steps,
                                                                  seed);
            break;
        case 72:
            std::cerr << "72: uint64_t, 16, 8, 4096" << std::endl;
            test<bv::simple_bv<8, 4096, 16>, 1, 3, 8, 16, 4096>(size, steps,
                                                                seed);
            break;
        case 73:
            std::cerr << "73: uint64_t, 16, 8, 8192" << std::endl;
            test<bv::simple_bv<8, 8192, 16>, 1, 3, 8, 16, 8192>(size, steps,
                                                                seed);
            break;
        case 74:
            std::cerr << "74: uint64_t, 16, 8, 16384" << std::endl;
            test<bv::simple_bv<8, 16384, 16>, 1, 3, 8, 16, 16384>(size, steps,
                                                                  seed);
            break;
        case 75:
            std::cerr << "75: uint64_t, 16, 8, 32768" << std::endl;
            test<bv::simple_bv<8, 32768, 16>, 1, 3, 8, 16, 32768>(size, steps,
                                                                  seed);
            break;
        case 76:
            std::cerr << "76: uint64_t, 16, 16, 4096" << std::endl;
            test<bv::simple_bv<16, 4096, 16>, 1, 3, 16, 16, 4096>(size, steps,
                                                                  seed);
            break;
        case 77:
            std::cerr << "77: uint64_t, 16, 16, 8192" << std::endl;
            test<bv::simple_bv<16, 8192, 16>, 1, 3, 16, 16, 8192>(size, steps,
                                                                  seed);
            break;
        case 78:
            std::cerr << "78: uint64_t, 16, 16, 16384" << std::endl;
            test<bv::simple_bv<16, 16384, 16>, 1, 3, 16, 16, 16384>(size, steps,
                                                                    seed);
            break;
        case 79:
            std::cerr << "79: uint64_t, 16, 16, 32768" << std::endl;
            test<bv::simple_bv<16, 32768, 16>, 1, 3, 16, 16, 32768>(size, steps,
                                                                    seed);
            break;
        case 80:
            std::cerr << "80: uint64_t, 16, 32, 4096" << std::endl;
            test<bv::simple_bv<32, 4096, 16>, 1, 3, 32, 16, 4096>(size, steps,
                                                                  seed);
            break;
        case 81:
            std::cerr << "81: uint64_t, 16, 32, 8192" << std::endl;
            test<bv::simple_bv<32, 8192, 16>, 1, 3, 32, 16, 8192>(size, steps,
                                                                  seed);
            break;
        case 82:
            std::cerr << "82: uint64_t, 16, 32, 16384" << std::endl;
            test<bv::simple_bv<32, 16384, 16>, 1, 3, 32, 16, 16384>(size, steps,
                                                                    seed);
            break;
        case 83:
            std::cerr << "83: uint64_t, 16, 32, 32768" << std::endl;
            test<bv::simple_bv<32, 32768, 16>, 1, 3, 32, 16, 32768>(size, steps,
                                                                    seed);
            break;
        case 84:
            std::cerr << "84: uint64_t, 32, 0, 4096" << std::endl;
            test<bv::simple_bv<0, 4096, 32>, 1, 3, 0, 32, 4096>(size, steps,
                                                                seed);
            break;
        case 85:
            std::cerr << "85: uint64_t, 32, 0, 8192" << std::endl;
            test<bv::simple_bv<0, 8192, 32>, 1, 3, 0, 32, 8192>(size, steps,
                                                                seed);
            break;
        case 86:
            std::cerr << "86: uint64_t, 32, 0, 16384" << std::endl;
            test<bv::simple_bv<0, 16384, 32>, 1, 3, 0, 32, 16384>(size, steps,
                                                                  seed);
            break;
        case 87:
            std::cerr << "87: uint64_t, 32, 0, 32768" << std::endl;
            test<bv::simple_bv<0, 32768, 32>, 1, 3, 0, 32, 32768>(size, steps,
                                                                  seed);
            break;
        case 88:
            std::cerr << "88: uint64_t, 32, 8, 4096" << std::endl;
            test<bv::simple_bv<8, 4096, 32>, 1, 3, 8, 32, 4096>(size, steps,
                                                                seed);
            break;
        case 89:
            std::cerr << "89: uint64_t, 32, 8, 8192" << std::endl;
            test<bv::simple_bv<8, 8192, 32>, 1, 3, 8, 32, 8192>(size, steps,
                                                                seed);
            break;
        case 90:
            std::cerr << "90: uint64_t, 32, 8, 16384" << std::endl;
            test<bv::simple_bv<8, 16384, 32>, 1, 3, 8, 32, 16384>(size, steps,
                                                                  seed);
            break;
        case 91:
            std::cerr << "91: uint64_t, 32, 8, 32768" << std::endl;
            test<bv::simple_bv<8, 32768, 32>, 1, 3, 8, 32, 32768>(size, steps,
                                                                  seed);
            break;
        case 92:
            std::cerr << "92: uint64_t, 32, 16, 4096" << std::endl;
            test<bv::simple_bv<16, 4096, 32>, 1, 3, 16, 32, 4096>(size, steps,
                                                                  seed);
            break;
        case 93:
            std::cerr << "93: uint64_t, 32, 16, 8192" << std::endl;
            test<bv::simple_bv<16, 8192, 32>, 1, 3, 16, 32, 8192>(size, steps,
                                                                  seed);
            break;
        case 94:
            std::cerr << "94: uint64_t, 32, 16, 16384" << std::endl;
            test<bv::simple_bv<16, 16384, 32>, 1, 3, 16, 32, 16384>(size, steps,
                                                                    seed);
            break;
        case 95:
            std::cerr << "95: uint64_t, 32, 16, 32768" << std::endl;
            test<bv::simple_bv<16, 32768, 32>, 1, 3, 16, 32, 32768>(size, steps,
                                                                    seed);
            break;
        case 96:
            std::cerr << "96: uint64_t, 32, 32, 4096" << std::endl;
            test<bv::simple_bv<32, 4096, 32>, 1, 3, 32, 32, 4096>(size, steps,
                                                                  seed);
            break;
        case 97:
            std::cerr << "97: uint64_t, 32, 32, 8192" << std::endl;
            test<bv::simple_bv<32, 8192, 32>, 1, 3, 32, 32, 8192>(size, steps,
                                                                  seed);
            break;
        case 98:
            std::cerr << "98: uint64_t, 32, 32, 16384" << std::endl;
            test<bv::simple_bv<32, 16384, 32>, 1, 3, 32, 32, 16384>(size, steps,
                                                                    seed);
            break;
        case 99:
            std::cerr << "99: uint64_t, 32, 32, 32768" << std::endl;
            test<bv::simple_bv<32, 32768, 32>, 1, 3, 32, 32, 32768>(size, steps,
                                                                    seed);
            break;
        case 100:
            std::cerr << "100: uint64_t, 64, 0, 4096" << std::endl;
            test<bv::simple_bv<0, 4096, 64>, 1, 3, 0, 64, 4096>(size, steps,
                                                                seed);
            break;
        case 101:
            std::cerr << "101: uint64_t, 64, 0, 8192" << std::endl;
            test<bv::simple_bv<0, 8192, 64>, 1, 3, 0, 64, 8192>(size, steps,
                                                                seed);
            break;
        case 102:
            std::cerr << "102: uint64_t, 64, 0, 16384" << std::endl;
            test<bv::simple_bv<0, 16384, 64>, 1, 3, 0, 64, 16384>(size, steps,
                                                                  seed);
            break;
        case 103:
            std::cerr << "103: uint64_t, 64, 0, 32768" << std::endl;
            test<bv::simple_bv<0, 32768, 64>, 1, 3, 0, 64, 32768>(size, steps,
                                                                  seed);
            break;
        case 104:
            std::cerr << "104: uint64_t, 64, 8, 4096" << std::endl;
            test<bv::simple_bv<8, 4096, 64>, 1, 3, 8, 64, 4096>(size, steps,
                                                                seed);
            break;
        case 105:
            std::cerr << "105: uint64_t, 64, 8, 8192" << std::endl;
            test<bv::simple_bv<8, 8192, 64>, 1, 3, 8, 64, 8192>(size, steps,
                                                                seed);
            break;
        case 106:
            std::cerr << "106: uint64_t, 64, 8, 16384" << std::endl;
            test<bv::simple_bv<8, 16384, 64>, 1, 3, 8, 64, 16384>(size, steps,
                                                                  seed);
            break;
        case 107:
            std::cerr << "107: uint64_t, 64, 8, 32768" << std::endl;
            test<bv::simple_bv<8, 32768, 64>, 1, 3, 8, 64, 32768>(size, steps,
                                                                  seed);
            break;
        case 108:
            std::cerr << "108: uint64_t, 64, 16, 4096" << std::endl;
            test<bv::simple_bv<16, 4096, 64>, 1, 3, 16, 64, 4096>(size, steps,
                                                                  seed);
            break;
        case 109:
            std::cerr << "109: uint64_t, 64, 16, 8192" << std::endl;
            test<bv::simple_bv<16, 8192, 64>, 1, 3, 16, 64, 8192>(size, steps,
                                                                  seed);
            break;
        case 110:
            std::cerr << "110: uint64_t, 64, 16, 16384" << std::endl;
            test<bv::simple_bv<16, 16384, 64>, 1, 3, 16, 64, 16384>(size, steps,
                                                                    seed);
            break;
        case 111:
            std::cerr << "111: uint64_t, 64, 16, 32768" << std::endl;
            test<bv::simple_bv<16, 32768, 64>, 1, 3, 16, 64, 32768>(size, steps,
                                                                    seed);
            break;
        case 112:
            std::cerr << "112: uint64_t, 64, 32, 4096" << std::endl;
            test<bv::simple_bv<32, 4096, 64>, 1, 3, 32, 64, 4096>(size, steps,
                                                                  seed);
            break;
        case 113:
            std::cerr << "113: uint64_t, 64, 32, 8192" << std::endl;
            test<bv::simple_bv<32, 8192, 64>, 1, 3, 32, 64, 8192>(size, steps,
                                                                  seed);
            break;
        case 114:
            std::cerr << "114: uint64_t, 64, 32, 16384" << std::endl;
            test<bv::simple_bv<32, 16384, 64>, 1, 3, 32, 64, 16384>(size, steps,
                                                                    seed);
            break;
        case 115:
            std::cerr << "115: uint64_t, 64, 32, 32768" << std::endl;
            test<bv::simple_bv<32, 32768, 64>, 1, 3, 32, 64, 32768>(size, steps,
                                                                    seed);
            break;
        case 116:
            std::cerr << "116: uint64_t, 128, 0, 4096" << std::endl;
            test<bv::simple_bv<0, 4096, 128>, 1, 3, 0, 128, 4096>(size, steps,
                                                                  seed);
            break;
        case 117:
            std::cerr << "117: uint64_t, 128, 0, 8192" << std::endl;
            test<bv::simple_bv<0, 8192, 128>, 1, 3, 0, 128, 8192>(size, steps,
                                                                  seed);
            break;
        case 118:
            std::cerr << "118: uint64_t, 128, 0, 16384" << std::endl;
            test<bv::simple_bv<0, 16384, 128>, 1, 3, 0, 128, 16384>(size, steps,
                                                                    seed);
            break;
        case 119:
            std::cerr << "119: uint64_t, 128, 0, 32768" << std::endl;
            test<bv::simple_bv<0, 32768, 128>, 1, 3, 0, 128, 32768>(size, steps,
                                                                    seed);
            break;
        case 120:
            std::cerr << "120: uint64_t, 128, 8, 4096" << std::endl;
            test<bv::simple_bv<8, 4096, 128>, 1, 3, 8, 128, 4096>(size, steps,
                                                                  seed);
            break;
        case 121:
            std::cerr << "121: uint64_t, 128, 8, 8192" << std::endl;
            test<bv::simple_bv<8, 8192, 128>, 1, 3, 8, 128, 8192>(size, steps,
                                                                  seed);
            break;
        case 122:
            std::cerr << "122: uint64_t, 128, 8, 16384" << std::endl;
            test<bv::simple_bv<8, 16384, 128>, 1, 3, 8, 128, 16384>(size, steps,
                                                                    seed);
            break;
        case 123:
            std::cerr << "123: uint64_t, 128, 8, 32768" << std::endl;
            test<bv::simple_bv<8, 32768, 128>, 1, 3, 8, 128, 32768>(size, steps,
                                                                    seed);
            break;
        case 124:
            std::cerr << "124: uint64_t, 128, 16, 4096" << std::endl;
            test<bv::simple_bv<16, 4096, 128>, 1, 3, 16, 128, 4096>(size, steps,
                                                                    seed);
            break;
        case 125:
            std::cerr << "125: uint64_t, 128, 16, 8192" << std::endl;
            test<bv::simple_bv<16, 8192, 128>, 1, 3, 16, 128, 8192>(size, steps,
                                                                    seed);
            break;
        case 126:
            std::cerr << "126: uint64_t, 128, 16, 16384" << std::endl;
            test<bv::simple_bv<16, 16384, 128>, 1, 3, 16, 128, 16384>(
                size, steps, seed);
            break;
        case 127:
            std::cerr << "127: uint64_t, 128, 16, 32768" << std::endl;
            test<bv::simple_bv<16, 32768, 128>, 1, 3, 16, 128, 32768>(
                size, steps, seed);
            break;
        case 128:
            std::cerr << "128: uint64_t, 128, 32, 4096" << std::endl;
            test<bv::simple_bv<32, 4096, 128>, 1, 3, 32, 128, 4096>(size, steps,
                                                                    seed);
            break;
        case 129:
            std::cerr << "129: uint64_t, 128, 32, 8192" << std::endl;
            test<bv::simple_bv<32, 8192, 128>, 1, 3, 32, 128, 8192>(size, steps,
                                                                    seed);
            break;
        case 130:
            std::cerr << "130: uint64_t, 128, 32, 16384" << std::endl;
            test<bv::simple_bv<32, 16384, 128>, 1, 3, 32, 128, 16384>(
                size, steps, seed);
            break;
        default:
            std::cerr << "131: uint64_t, 128, 32, 32768" << std::endl;
            test<bv::simple_bv<32, 32768, 128>, 1, 3, 32, 128, 32768>(
                size, steps, seed);
            break;
    }
    return 0;
}
