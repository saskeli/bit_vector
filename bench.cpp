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

    std::cout
        << "type\tbuffer\tbranch\tleaf_size\tseed\tsize\tremove\tinsert\tset\t"
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
        } else if constexpr (type == 3) {
            std::cout << "uint64_t\t";
        } else if constexpr (type == 4) {
            std::cout << "n_avx_32\t";
        } else {
            std::cout << "n_avx_64\t";
        }
        std::cout << int(buffer) << "\t" << int(branch) << "\t" << leaf_size
                  << "\t" << seed << "\t" << target << "\t";

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
    bool avx = true;
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
    if (argc > 5) {
        uint64_t avx_val;
        std::sscanf(argv[5], "%lu", &avx_val);
        avx = avx_val ? true : false;
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
            std::cerr << "3: leaf, 16, 0, 4096" << std::endl;
            test<dyn::b_suc_bv, 0, 1, 8, 16, 8192>(size, steps, seed);
            break;
        case 4:
            if (avx) {
                std::cerr << "4: uint64_t, 16, 0, 4096" << std::endl;
                test<bv::simple_bv<0, 4096, 16>, 1, 3, 0, 16, 4096>(size, steps,
                                                                    seed);
            } else {
                std::cerr << "4: n_avx_64, 16, 0, 4096" << std::endl;
                test<bv::simple_bv<0, 4096, 16, false>, 1, 3, 0, 16, 4096>(
                    size, steps, seed);
            }
            break;
        case 5:
            if (avx) {
                std::cerr << "5: uint64_t, 16, 0, 8192" << std::endl;
                test<bv::simple_bv<0, 8192, 16>, 1, 3, 0, 16, 8192>(size, steps,
                                                                    seed);
            } else {
                std::cerr << "5: n_avx_64, 16, 0, 8192" << std::endl;
                test<bv::simple_bv<0, 8192, 16, false>, 1, 3, 0, 16, 8192>(
                    size, steps, seed);
            }
            break;
        case 6:
            if (avx) {
                std::cerr << "6: uint64_t, 16, 0, 16384" << std::endl;
                test<bv::simple_bv<0, 16384, 16>, 1, 3, 0, 16, 16384>(
                    size, steps, seed);
            } else {
                std::cerr << "6: n_avx_64, 16, 0, 16384" << std::endl;
                test<bv::simple_bv<0, 16384, 16, false>, 1, 3, 0, 16, 16384>(
                    size, steps, seed);
            }
            break;
        case 7:
            if (avx) {
                std::cerr << "7: uint64_t, 16, 0, 32768" << std::endl;
                test<bv::simple_bv<0, 32768, 16>, 1, 3, 0, 16, 32768>(
                    size, steps, seed);
            } else {
                std::cerr << "7: n_avx_64, 16, 0, 32768" << std::endl;
                test<bv::simple_bv<0, 32768, 16, false>, 1, 3, 0, 16, 32768>(
                    size, steps, seed);
            }
            break;
        case 8:
            if (avx) {
                std::cerr << "8: uint64_t, 16, 8, 4096" << std::endl;
                test<bv::simple_bv<8, 4096, 16>, 1, 3, 8, 16, 4096>(size, steps,
                                                                    seed);
            } else {
                std::cerr << "8: n_avx_64, 16, 8, 4096" << std::endl;
                test<bv::simple_bv<8, 4096, 16, false>, 1, 3, 8, 16, 4096>(
                    size, steps, seed);
            }
            break;
        case 9:
            if (avx) {
                std::cerr << "9: uint64_t, 16, 8, 8192" << std::endl;
                test<bv::simple_bv<8, 8192, 16>, 1, 3, 8, 16, 8192>(size, steps,
                                                                    seed);
            } else {
                std::cerr << "9: n_avx_64, 16, 8, 8192" << std::endl;
                test<bv::simple_bv<8, 8192, 16, false>, 1, 3, 8, 16, 8192>(
                    size, steps, seed);
            }
            break;
        case 10:
            if (avx) {
                std::cerr << "10: uint64_t, 16, 8, 16384" << std::endl;
                test<bv::simple_bv<8, 16384, 16>, 1, 3, 8, 16, 16384>(
                    size, steps, seed);
            } else {
                std::cerr << "10: n_avx_64, 16, 8, 16384" << std::endl;
                test<bv::simple_bv<8, 16384, 16, false>, 1, 3, 8, 16, 16384>(
                    size, steps, seed);
            }
            break;
        case 11:
            if (avx) {
                std::cerr << "11: uint64_t, 16, 8, 32768" << std::endl;
                test<bv::simple_bv<8, 32768, 16>, 1, 3, 8, 16, 32768>(
                    size, steps, seed);
            } else {
                std::cerr << "11: n_avx_64, 16, 8, 32768" << std::endl;
                test<bv::simple_bv<8, 32768, 16, false>, 1, 3, 8, 16, 32768>(
                    size, steps, seed);
            }
            break;
        case 12:
            if (avx) {
                std::cerr << "12: uint64_t, 16, 16, 4096" << std::endl;
                test<bv::simple_bv<16, 4096, 16>, 1, 3, 16, 16, 4096>(
                    size, steps, seed);
            } else {
                std::cerr << "12: n_avx_64, 16, 16, 4096" << std::endl;
                test<bv::simple_bv<16, 4096, 16, false>, 1, 3, 16, 16, 4096>(
                    size, steps, seed);
            }
            break;
        case 13:
            if (avx) {
                std::cerr << "13: uint64_t, 16, 16, 8192" << std::endl;
                test<bv::simple_bv<16, 8192, 16>, 1, 3, 16, 16, 8192>(
                    size, steps, seed);
            } else {
                std::cerr << "13: n_avx_64, 16, 16, 8192" << std::endl;
                test<bv::simple_bv<16, 8192, 16, false>, 1, 3, 16, 16, 8192>(
                    size, steps, seed);
            }
            break;
        case 14:
            if (avx) {
                std::cerr << "14: uint64_t, 16, 16, 16384" << std::endl;
                test<bv::simple_bv<16, 16384, 16>, 1, 3, 16, 16, 16384>(
                    size, steps, seed);
            } else {
                std::cerr << "14: n_avx_64, 16, 16, 16384" << std::endl;
                test<bv::simple_bv<16, 16384, 16, false>, 1, 3, 16, 16, 16384>(
                    size, steps, seed);
            }
            break;
        case 15:
            if (avx) {
                std::cerr << "15: uint64_t, 16, 16, 32768" << std::endl;
                test<bv::simple_bv<16, 32768, 16>, 1, 3, 16, 16, 32768>(
                    size, steps, seed);
            } else {
                std::cerr << "15: n_avx_64, 16, 16, 32768" << std::endl;
                test<bv::simple_bv<16, 32768, 16, false>, 1, 3, 16, 16, 32768>(
                    size, steps, seed);
            }
            break;
        case 16:
            if (avx) {
                std::cerr << "16: uint64_t, 16, 32, 4096" << std::endl;
                test<bv::simple_bv<32, 4096, 16>, 1, 3, 32, 16, 4096>(
                    size, steps, seed);
            } else {
                std::cerr << "16: n_avx_64, 16, 32, 4096" << std::endl;
                test<bv::simple_bv<32, 4096, 16, false>, 1, 3, 32, 16, 4096>(
                    size, steps, seed);
            }
            break;
        case 17:
            if (avx) {
                std::cerr << "17: uint64_t, 16, 32, 8192" << std::endl;
                test<bv::simple_bv<32, 8192, 16>, 1, 3, 32, 16, 8192>(
                    size, steps, seed);
            } else {
                std::cerr << "17: n_avx_64, 16, 32, 8192" << std::endl;
                test<bv::simple_bv<32, 8192, 16, false>, 1, 3, 32, 16, 8192>(
                    size, steps, seed);
            }
            break;
        case 18:
            if (avx) {
                std::cerr << "18: uint64_t, 16, 32, 16384" << std::endl;
                test<bv::simple_bv<32, 16384, 16>, 1, 3, 32, 16, 16384>(
                    size, steps, seed);
            } else {
                std::cerr << "18: n_avx_64, 16, 32, 16384" << std::endl;
                test<bv::simple_bv<32, 16384, 16, false>, 1, 3, 32, 16, 16384>(
                    size, steps, seed);
            }
            break;
        case 19:
            if (avx) {
                std::cerr << "19: uint64_t, 16, 32, 32768" << std::endl;
                test<bv::simple_bv<32, 32768, 16>, 1, 3, 32, 16, 32768>(
                    size, steps, seed);
            } else {
                std::cerr << "19: n_avx_64, 16, 32, 32768" << std::endl;
                test<bv::simple_bv<32, 32768, 16, false>, 1, 3, 32, 16, 32768>(
                    size, steps, seed);
            }
            break;
        case 20:
            if (avx) {
                std::cerr << "20: uint64_t, 32, 0, 4096" << std::endl;
                test<bv::simple_bv<0, 4096, 32>, 1, 3, 0, 32, 4096>(size, steps,
                                                                    seed);
            } else {
                std::cerr << "20: n_avx_64, 32, 0, 4096" << std::endl;
                test<bv::simple_bv<0, 4096, 32, false>, 1, 3, 0, 32, 4096>(
                    size, steps, seed);
            }
            break;
        case 21:
            if (avx) {
                std::cerr << "21: uint64_t, 32, 0, 8192" << std::endl;
                test<bv::simple_bv<0, 8192, 32>, 1, 3, 0, 32, 8192>(size, steps,
                                                                    seed);
            } else {
                std::cerr << "21: n_avx_64, 32, 0, 8192" << std::endl;
                test<bv::simple_bv<0, 8192, 32, false>, 1, 3, 0, 32, 8192>(
                    size, steps, seed);
            }
            break;
        case 22:
            if (avx) {
                std::cerr << "22: uint64_t, 32, 0, 16384" << std::endl;
                test<bv::simple_bv<0, 16384, 32>, 1, 3, 0, 32, 16384>(
                    size, steps, seed);
            } else {
                std::cerr << "22: n_avx_64, 32, 0, 16384" << std::endl;
                test<bv::simple_bv<0, 16384, 32, false>, 1, 3, 0, 32, 16384>(
                    size, steps, seed);
            }
            break;
        case 23:
            if (avx) {
                std::cerr << "23: uint64_t, 32, 0, 32768" << std::endl;
                test<bv::simple_bv<0, 32768, 32>, 1, 3, 0, 32, 32768>(
                    size, steps, seed);
            } else {
                std::cerr << "23: n_avx_64, 32, 0, 32768" << std::endl;
                test<bv::simple_bv<0, 32768, 32, false>, 1, 3, 0, 32, 32768>(
                    size, steps, seed);
            }
            break;
        case 24:
            if (avx) {
                std::cerr << "24: uint64_t, 32, 8, 4096" << std::endl;
                test<bv::simple_bv<8, 4096, 32>, 1, 3, 8, 32, 4096>(size, steps,
                                                                    seed);
            } else {
                std::cerr << "24: n_avx_64, 32, 8, 4096" << std::endl;
                test<bv::simple_bv<8, 4096, 32, false>, 1, 3, 8, 32, 4096>(
                    size, steps, seed);
            }
            break;
        case 25:
            if (avx) {
                std::cerr << "25: uint64_t, 32, 8, 8192" << std::endl;
                test<bv::simple_bv<8, 8192, 32>, 1, 3, 8, 32, 8192>(size, steps,
                                                                    seed);
            } else {
                std::cerr << "25: n_avx_64, 32, 8, 8192" << std::endl;
                test<bv::simple_bv<8, 8192, 32, false>, 1, 3, 8, 32, 8192>(
                    size, steps, seed);
            }
            break;
        case 26:
            if (avx) {
                std::cerr << "26: uint64_t, 32, 8, 16384" << std::endl;
                test<bv::simple_bv<8, 16384, 32>, 1, 3, 8, 32, 16384>(
                    size, steps, seed);
            } else {
                std::cerr << "26: n_avx_64, 32, 8, 16384" << std::endl;
                test<bv::simple_bv<8, 16384, 32, false>, 1, 3, 8, 32, 16384>(
                    size, steps, seed);
            }
            break;
        case 27:
            if (avx) {
                std::cerr << "27: uint64_t, 32, 8, 32768" << std::endl;
                test<bv::simple_bv<8, 32768, 32>, 1, 3, 8, 32, 32768>(
                    size, steps, seed);
            } else {
                std::cerr << "27: n_avx_64, 32, 8, 32768" << std::endl;
                test<bv::simple_bv<8, 32768, 32, false>, 1, 3, 8, 32, 32768>(
                    size, steps, seed);
            }
            break;
        case 28:
            if (avx) {
                std::cerr << "28: uint64_t, 32, 16, 4096" << std::endl;
                test<bv::simple_bv<16, 4096, 32>, 1, 3, 16, 32, 4096>(
                    size, steps, seed);
            } else {
                std::cerr << "28: n_avx_64, 32, 16, 4096" << std::endl;
                test<bv::simple_bv<16, 4096, 32, false>, 1, 3, 16, 32, 4096>(
                    size, steps, seed);
            }
            break;
        case 29:
            if (avx) {
                std::cerr << "29: uint64_t, 32, 16, 8192" << std::endl;
                test<bv::simple_bv<16, 8192, 32>, 1, 3, 16, 32, 8192>(
                    size, steps, seed);
            } else {
                std::cerr << "29: n_avx_64, 32, 16, 8192" << std::endl;
                test<bv::simple_bv<16, 8192, 32, false>, 1, 3, 16, 32, 8192>(
                    size, steps, seed);
            }
            break;
        case 30:
            if (avx) {
                std::cerr << "30: uint64_t, 32, 16, 16384" << std::endl;
                test<bv::simple_bv<16, 16384, 32>, 1, 3, 16, 32, 16384>(
                    size, steps, seed);
            } else {
                std::cerr << "30: n_avx_64, 32, 16, 16384" << std::endl;
                test<bv::simple_bv<16, 16384, 32, false>, 1, 3, 16, 32, 16384>(
                    size, steps, seed);
            }
            break;
        case 31:
            if (avx) {
                std::cerr << "31: uint64_t, 32, 16, 32768" << std::endl;
                test<bv::simple_bv<16, 32768, 32>, 1, 3, 16, 32, 32768>(
                    size, steps, seed);
            } else {
                std::cerr << "31: n_avx_64, 32, 16, 32768" << std::endl;
                test<bv::simple_bv<16, 32768, 32, false>, 1, 3, 16, 32, 32768>(
                    size, steps, seed);
            }
            break;
        case 32:
            if (avx) {
                std::cerr << "32: uint64_t, 32, 32, 4096" << std::endl;
                test<bv::simple_bv<32, 4096, 32>, 1, 3, 32, 32, 4096>(
                    size, steps, seed);
            } else {
                std::cerr << "32: n_avx_64, 32, 32, 4096" << std::endl;
                test<bv::simple_bv<32, 4096, 32, false>, 1, 3, 32, 32, 4096>(
                    size, steps, seed);
            }
            break;
        case 33:
            if (avx) {
                std::cerr << "33: uint64_t, 32, 32, 8192" << std::endl;
                test<bv::simple_bv<32, 8192, 32>, 1, 3, 32, 32, 8192>(
                    size, steps, seed);
            } else {
                std::cerr << "33: n_avx_64, 32, 32, 8192" << std::endl;
                test<bv::simple_bv<32, 8192, 32, false>, 1, 3, 32, 32, 8192>(
                    size, steps, seed);
            }
            break;
        case 34:
            if (avx) {
                std::cerr << "34: uint64_t, 32, 32, 16384" << std::endl;
                test<bv::simple_bv<32, 16384, 32>, 1, 3, 32, 32, 16384>(
                    size, steps, seed);
            } else {
                std::cerr << "34: n_avx_64, 32, 32, 16384" << std::endl;
                test<bv::simple_bv<32, 16384, 32, false>, 1, 3, 32, 32, 16384>(
                    size, steps, seed);
            }
            break;
        case 35:
            if (avx) {
                std::cerr << "35: uint64_t, 32, 32, 32768" << std::endl;
                test<bv::simple_bv<32, 32768, 32>, 1, 3, 32, 32, 32768>(
                    size, steps, seed);
            } else {
                std::cerr << "35: n_avx_64, 32, 32, 32768" << std::endl;
                test<bv::simple_bv<32, 32768, 32, false>, 1, 3, 32, 32, 32768>(
                    size, steps, seed);
            }
            break;
        case 36:
            if (avx) {
                std::cerr << "36: uint64_t, 64, 0, 4096" << std::endl;
                test<bv::simple_bv<0, 4096, 64>, 1, 3, 0, 64, 4096>(size, steps,
                                                                    seed);
            } else {
                std::cerr << "36: n_avx_64, 64, 0, 4096" << std::endl;
                test<bv::simple_bv<0, 4096, 64, false>, 1, 3, 0, 64, 4096>(
                    size, steps, seed);
            }
            break;
        case 37:
            if (avx) {
                std::cerr << "37: uint64_t, 64, 0, 8192" << std::endl;
                test<bv::simple_bv<0, 8192, 64>, 1, 3, 0, 64, 8192>(size, steps,
                                                                    seed);
            } else {
                std::cerr << "37: n_avx_64, 64, 0, 8192" << std::endl;
                test<bv::simple_bv<0, 8192, 64, false>, 1, 3, 0, 64, 8192>(
                    size, steps, seed);
            }
            break;
        case 38:
            if (avx) {
                std::cerr << "38: uint64_t, 64, 0, 16384" << std::endl;
                test<bv::simple_bv<0, 16384, 64>, 1, 3, 0, 64, 16384>(
                    size, steps, seed);
            } else {
                std::cerr << "38: n_avx_64, 64, 0, 16384" << std::endl;
                test<bv::simple_bv<0, 16384, 64, false>, 1, 3, 0, 64, 16384>(
                    size, steps, seed);
            }
            break;
        case 39:
            if (avx) {
                std::cerr << "39: uint64_t, 64, 0, 32768" << std::endl;
                test<bv::simple_bv<0, 32768, 64>, 1, 3, 0, 64, 32768>(
                    size, steps, seed);
            } else {
                std::cerr << "39: n_avx_64, 64, 0, 32768" << std::endl;
                test<bv::simple_bv<0, 32768, 64, false>, 1, 3, 0, 64, 32768>(
                    size, steps, seed);
            }
            break;
        case 40:
            if (avx) {
                std::cerr << "40: uint64_t, 64, 8, 4096" << std::endl;
                test<bv::simple_bv<8, 4096, 64>, 1, 3, 8, 64, 4096>(size, steps,
                                                                    seed);
            } else {
                std::cerr << "40: n_avx_64, 64, 8, 4096" << std::endl;
                test<bv::simple_bv<8, 4096, 64, false>, 1, 3, 8, 64, 4096>(
                    size, steps, seed);
            }
            break;
        case 41:
            if (avx) {
                std::cerr << "41: uint64_t, 64, 8, 8192" << std::endl;
                test<bv::simple_bv<8, 8192, 64>, 1, 3, 8, 64, 8192>(size, steps,
                                                                    seed);
            } else {
                std::cerr << "41: n_avx_64, 64, 8, 8192" << std::endl;
                test<bv::simple_bv<8, 8192, 64, false>, 1, 3, 8, 64, 8192>(
                    size, steps, seed);
            }
            break;
        case 42:
            if (avx) {
                std::cerr << "42: uint64_t, 64, 8, 16384" << std::endl;
                test<bv::simple_bv<8, 16384, 64>, 1, 3, 8, 64, 16384>(
                    size, steps, seed);
            } else {
                std::cerr << "42: n_avx_64, 64, 8, 16384" << std::endl;
                test<bv::simple_bv<8, 16384, 64, false>, 1, 3, 8, 64, 16384>(
                    size, steps, seed);
            }
            break;
        case 43:
            if (avx) {
                std::cerr << "43: uint64_t, 64, 8, 32768" << std::endl;
                test<bv::simple_bv<8, 32768, 64>, 1, 3, 8, 64, 32768>(
                    size, steps, seed);
            } else {
                std::cerr << "43: n_avx_64, 64, 8, 32768" << std::endl;
                test<bv::simple_bv<8, 32768, 64, false>, 1, 3, 8, 64, 32768>(
                    size, steps, seed);
            }
            break;
        case 44:
            if (avx) {
                std::cerr << "44: uint64_t, 64, 16, 4096" << std::endl;
                test<bv::simple_bv<16, 4096, 64>, 1, 3, 16, 64, 4096>(
                    size, steps, seed);
            } else {
                std::cerr << "44: n_avx_64, 64, 16, 4096" << std::endl;
                test<bv::simple_bv<16, 4096, 64, false>, 1, 3, 16, 64, 4096>(
                    size, steps, seed);
            }
            break;
        case 45:
            if (avx) {
                std::cerr << "45: uint64_t, 64, 16, 8192" << std::endl;
                test<bv::simple_bv<16, 8192, 64>, 1, 3, 16, 64, 8192>(
                    size, steps, seed);
            } else {
                std::cerr << "45: n_avx_64, 64, 16, 8192" << std::endl;
                test<bv::simple_bv<16, 8192, 64, false>, 1, 3, 16, 64, 8192>(
                    size, steps, seed);
            }
            break;
        case 46:
            if (avx) {
                std::cerr << "46: uint64_t, 64, 16, 16384" << std::endl;
                test<bv::simple_bv<16, 16384, 64>, 1, 3, 16, 64, 16384>(
                    size, steps, seed);
            } else {
                std::cerr << "46: n_avx_64, 64, 16, 16384" << std::endl;
                test<bv::simple_bv<16, 16384, 64, false>, 1, 3, 16, 64, 16384>(
                    size, steps, seed);
            }
            break;
        case 47:
            if (avx) {
                std::cerr << "47: uint64_t, 64, 16, 32768" << std::endl;
                test<bv::simple_bv<16, 32768, 64>, 1, 3, 16, 64, 32768>(
                    size, steps, seed);
            } else {
                std::cerr << "47: n_avx_64, 64, 16, 32768" << std::endl;
                test<bv::simple_bv<16, 32768, 64, false>, 1, 3, 16, 64, 32768>(
                    size, steps, seed);
            }
            break;
        case 48:
            if (avx) {
                std::cerr << "48: uint64_t, 64, 32, 4096" << std::endl;
                test<bv::simple_bv<32, 4096, 64>, 1, 3, 32, 64, 4096>(
                    size, steps, seed);
            } else {
                std::cerr << "48: n_avx_64, 64, 32, 4096" << std::endl;
                test<bv::simple_bv<32, 4096, 64, false>, 1, 3, 32, 64, 4096>(
                    size, steps, seed);
            }
            break;
        case 49:
            if (avx) {
                std::cerr << "49: uint64_t, 64, 32, 8192" << std::endl;
                test<bv::simple_bv<32, 8192, 64>, 1, 3, 32, 64, 8192>(
                    size, steps, seed);
            } else {
                std::cerr << "49: n_avx_64, 64, 32, 8192" << std::endl;
                test<bv::simple_bv<32, 8192, 64, false>, 1, 3, 32, 64, 8192>(
                    size, steps, seed);
            }
            break;
        case 50:
            if (avx) {
                std::cerr << "50: uint64_t, 64, 32, 16384" << std::endl;
                test<bv::simple_bv<32, 16384, 64>, 1, 3, 32, 64, 16384>(
                    size, steps, seed);
            } else {
                std::cerr << "50: n_avx_64, 64, 32, 16384" << std::endl;
                test<bv::simple_bv<32, 16384, 64, false>, 1, 3, 32, 64, 16384>(
                    size, steps, seed);
            }
            break;
        case 51:
            if (avx) {
                std::cerr << "51: uint64_t, 64, 32, 32768" << std::endl;
                test<bv::simple_bv<32, 32768, 64>, 1, 3, 32, 64, 32768>(
                    size, steps, seed);
            } else {
                std::cerr << "51: n_avx_64, 64, 32, 32768" << std::endl;
                test<bv::simple_bv<32, 32768, 64, false>, 1, 3, 32, 64, 32768>(
                    size, steps, seed);
            }
            break;
        case 52:
            if (avx) {
                std::cerr << "52: uint64_t, 128, 0, 4096" << std::endl;
                test<bv::simple_bv<0, 4096, 128>, 1, 3, 0, 128, 4096>(
                    size, steps, seed);
            } else {
                std::cerr << "52: n_avx_64, 128, 0, 4096" << std::endl;
                test<bv::simple_bv<0, 4096, 128, false>, 1, 3, 0, 128, 4096>(
                    size, steps, seed);
            }
            break;
        case 53:
            if (avx) {
                std::cerr << "53: uint64_t, 128, 0, 8192" << std::endl;
                test<bv::simple_bv<0, 8192, 128>, 1, 3, 0, 128, 8192>(
                    size, steps, seed);
            } else {
                std::cerr << "53: n_avx_64, 128, 0, 8192" << std::endl;
                test<bv::simple_bv<0, 8192, 128, false>, 1, 3, 0, 128, 8192>(
                    size, steps, seed);
            }
            break;
        case 54:
            if (avx) {
                std::cerr << "54: uint64_t, 128, 0, 16384" << std::endl;
                test<bv::simple_bv<0, 16384, 128>, 1, 3, 0, 128, 16384>(
                    size, steps, seed);
            } else {
                std::cerr << "54: n_avx_64, 128, 0, 16384" << std::endl;
                test<bv::simple_bv<0, 16384, 128, false>, 1, 3, 0, 128, 16384>(
                    size, steps, seed);
            }
            break;
        case 55:
            if (avx) {
                std::cerr << "55: uint64_t, 128, 0, 32768" << std::endl;
                test<bv::simple_bv<0, 32768, 128>, 1, 3, 0, 128, 32768>(
                    size, steps, seed);
            } else {
                std::cerr << "55: n_avx_64, 128, 0, 32768" << std::endl;
                test<bv::simple_bv<0, 32768, 128, false>, 1, 3, 0, 128, 32768>(
                    size, steps, seed);
            }
            break;
        case 56:
            if (avx) {
                std::cerr << "56: uint64_t, 128, 8, 4096" << std::endl;
                test<bv::simple_bv<8, 4096, 128>, 1, 3, 8, 128, 4096>(
                    size, steps, seed);
            } else {
                std::cerr << "56: n_avx_64, 128, 8, 4096" << std::endl;
                test<bv::simple_bv<8, 4096, 128, false>, 1, 3, 8, 128, 4096>(
                    size, steps, seed);
            }
            break;
        case 57:
            if (avx) {
                std::cerr << "57: uint64_t, 128, 8, 8192" << std::endl;
                test<bv::simple_bv<8, 8192, 128>, 1, 3, 8, 128, 8192>(
                    size, steps, seed);
            } else {
                std::cerr << "57: n_avx_64, 128, 8, 8192" << std::endl;
                test<bv::simple_bv<8, 8192, 128, false>, 1, 3, 8, 128, 8192>(
                    size, steps, seed);
            }
            break;
        case 58:
            if (avx) {
                std::cerr << "58: uint64_t, 128, 8, 16384" << std::endl;
                test<bv::simple_bv<8, 16384, 128>, 1, 3, 8, 128, 16384>(
                    size, steps, seed);
            } else {
                std::cerr << "58: n_avx_64, 128, 8, 16384" << std::endl;
                test<bv::simple_bv<8, 16384, 128, false>, 1, 3, 8, 128, 16384>(
                    size, steps, seed);
            }
            break;
        case 59:
            if (avx) {
                std::cerr << "59: uint64_t, 128, 8, 32768" << std::endl;
                test<bv::simple_bv<8, 32768, 128>, 1, 3, 8, 128, 32768>(
                    size, steps, seed);
            } else {
                std::cerr << "59: n_avx_64, 128, 8, 32768" << std::endl;
                test<bv::simple_bv<8, 32768, 128, false>, 1, 3, 8, 128, 32768>(
                    size, steps, seed);
            }
            break;
        case 60:
            if (avx) {
                std::cerr << "60: uint64_t, 128, 16, 4096" << std::endl;
                test<bv::simple_bv<16, 4096, 128>, 1, 3, 16, 128, 4096>(
                    size, steps, seed);
            } else {
                std::cerr << "60: n_avx_64, 128, 16, 4096" << std::endl;
                test<bv::simple_bv<16, 4096, 128, false>, 1, 3, 16, 128, 4096>(
                    size, steps, seed);
            }
            break;
        case 61:
            if (avx) {
                std::cerr << "61: uint64_t, 128, 16, 8192" << std::endl;
                test<bv::simple_bv<16, 8192, 128>, 1, 3, 16, 128, 8192>(
                    size, steps, seed);
            } else {
                std::cerr << "61: n_avx_64, 128, 16, 8192" << std::endl;
                test<bv::simple_bv<16, 8192, 128, false>, 1, 3, 16, 128, 8192>(
                    size, steps, seed);
            }
            break;
        case 62:
            if (avx) {
                std::cerr << "62: uint64_t, 128, 16, 16384" << std::endl;
                test<bv::simple_bv<16, 16384, 128>, 1, 3, 16, 128, 16384>(
                    size, steps, seed);
            } else {
                std::cerr << "62: n_avx_64, 128, 16, 16384" << std::endl;
                test<bv::simple_bv<16, 16384, 128, false>, 1, 3, 16, 128,
                     16384>(size, steps, seed);
            }
            break;
        case 63:
            if (avx) {
                std::cerr << "63: uint64_t, 128, 16, 32768" << std::endl;
                test<bv::simple_bv<16, 32768, 128>, 1, 3, 16, 128, 32768>(
                    size, steps, seed);
            } else {
                std::cerr << "63: n_avx_64, 128, 16, 32768" << std::endl;
                test<bv::simple_bv<16, 32768, 128, false>, 1, 3, 16, 128,
                     32768>(size, steps, seed);
            }
            break;
        case 64:
            if (avx) {
                std::cerr << "64: uint64_t, 128, 32, 4096" << std::endl;
                test<bv::simple_bv<32, 4096, 128>, 1, 3, 32, 128, 4096>(
                    size, steps, seed);
            } else {
                std::cerr << "64: n_avx_64, 128, 32, 4096" << std::endl;
                test<bv::simple_bv<32, 4096, 128, false>, 1, 3, 32, 128, 4096>(
                    size, steps, seed);
            }
            break;
        case 65:
            if (avx) {
                std::cerr << "65: uint64_t, 128, 32, 8192" << std::endl;
                test<bv::simple_bv<32, 8192, 128>, 1, 3, 32, 128, 8192>(
                    size, steps, seed);
            } else {
                std::cerr << "65: n_avx_64, 128, 32, 8192" << std::endl;
                test<bv::simple_bv<32, 8192, 128, false>, 1, 3, 32, 128, 8192>(
                    size, steps, seed);
            }
            break;
        case 66:
            if (avx) {
                std::cerr << "66: uint64_t, 128, 32, 16384" << std::endl;
                test<bv::simple_bv<32, 16384, 128>, 1, 3, 32, 128, 16384>(
                    size, steps, seed);
            } else {
                std::cerr << "66: n_avx_64, 128, 32, 16384" << std::endl;
                test<bv::simple_bv<32, 16384, 128, false>, 1, 3, 32, 128,
                     16384>(size, steps, seed);
            }
            break;
        case 67:
            if (avx) {
                std::cerr << "67: uint64_t, 128, 32, 32768" << std::endl;
                test<bv::simple_bv<32, 32768, 128>, 1, 3, 32, 128, 32768>(
                    size, steps, seed);
            } else {
                std::cerr << "67: n_avx_64, 128, 32, 32768" << std::endl;
                test<bv::simple_bv<32, 32768, 128, false>, 1, 3, 32, 128,
                     32768>(size, steps, seed);
            }
            break;
        case 68:
            if (avx) {
                std::cerr << "68: uint32_t, 16, 0, 4096" << std::endl;
                test<bv::small_bv<0, 4096, 16>, 1, 2, 0, 16, 4096>(size, steps,
                                                                   seed);
            } else {
                std::cerr << "68: n_avx_32, 16, 0, 4096" << std::endl;
                test<bv::small_bv<0, 4096, 16, false>, 1, 2, 0, 16, 4096>(
                    size, steps, seed);
            }
            break;
        case 69:
            if (avx) {
                std::cerr << "69: uint32_t, 16, 0, 8192" << std::endl;
                test<bv::small_bv<0, 8192, 16>, 1, 2, 0, 16, 8192>(size, steps,
                                                                   seed);
            } else {
                std::cerr << "69: n_avx_32, 16, 0, 8192" << std::endl;
                test<bv::small_bv<0, 8192, 16, false>, 1, 2, 0, 16, 8192>(
                    size, steps, seed);
            }
            break;
        case 70:
            if (avx) {
                std::cerr << "70: uint32_t, 16, 0, 16384" << std::endl;
                test<bv::small_bv<0, 16384, 16>, 1, 2, 0, 16, 16384>(
                    size, steps, seed);
            } else {
                std::cerr << "70: n_avx_32, 16, 0, 16384" << std::endl;
                test<bv::small_bv<0, 16384, 16, false>, 1, 2, 0, 16, 16384>(
                    size, steps, seed);
            }
            break;
        case 71:
            if (avx) {
                std::cerr << "71: uint32_t, 16, 0, 32768" << std::endl;
                test<bv::small_bv<0, 32768, 16>, 1, 2, 0, 16, 32768>(
                    size, steps, seed);
            } else {
                std::cerr << "71: n_avx_32, 16, 0, 32768" << std::endl;
                test<bv::small_bv<0, 32768, 16, false>, 1, 2, 0, 16, 32768>(
                    size, steps, seed);
            }
            break;
        case 72:
            if (avx) {
                std::cerr << "72: uint32_t, 16, 8, 4096" << std::endl;
                test<bv::small_bv<8, 4096, 16>, 1, 2, 8, 16, 4096>(size, steps,
                                                                   seed);
            } else {
                std::cerr << "72: n_avx_32, 16, 8, 4096" << std::endl;
                test<bv::small_bv<8, 4096, 16, false>, 1, 2, 8, 16, 4096>(
                    size, steps, seed);
            }
            break;
        case 73:
            if (avx) {
                std::cerr << "73: uint32_t, 16, 8, 8192" << std::endl;
                test<bv::small_bv<8, 8192, 16>, 1, 2, 8, 16, 8192>(size, steps,
                                                                   seed);
            } else {
                std::cerr << "73: n_avx_32, 16, 8, 8192" << std::endl;
                test<bv::small_bv<8, 8192, 16, false>, 1, 2, 8, 16, 8192>(
                    size, steps, seed);
            }
            break;
        case 74:
            if (avx) {
                std::cerr << "74: uint32_t, 16, 8, 16384" << std::endl;
                test<bv::small_bv<8, 16384, 16>, 1, 2, 8, 16, 16384>(
                    size, steps, seed);
            } else {
                std::cerr << "74: n_avx_32, 16, 8, 16384" << std::endl;
                test<bv::small_bv<8, 16384, 16, false>, 1, 2, 8, 16, 16384>(
                    size, steps, seed);
            }
            break;
        case 75:
            if (avx) {
                std::cerr << "75: uint32_t, 16, 8, 32768" << std::endl;
                test<bv::small_bv<8, 32768, 16>, 1, 2, 8, 16, 32768>(
                    size, steps, seed);
            } else {
                std::cerr << "75: n_avx_32, 16, 8, 32768" << std::endl;
                test<bv::small_bv<8, 32768, 16, false>, 1, 2, 8, 16, 32768>(
                    size, steps, seed);
            }
            break;
        case 76:
            if (avx) {
                std::cerr << "76: uint32_t, 16, 16, 4096" << std::endl;
                test<bv::small_bv<16, 4096, 16>, 1, 2, 16, 16, 4096>(
                    size, steps, seed);
            } else {
                std::cerr << "76: n_avx_32, 16, 16, 4096" << std::endl;
                test<bv::small_bv<16, 4096, 16, false>, 1, 2, 16, 16, 4096>(
                    size, steps, seed);
            }
            break;
        case 77:
            if (avx) {
                std::cerr << "77: uint32_t, 16, 16, 8192" << std::endl;
                test<bv::small_bv<16, 8192, 16>, 1, 2, 16, 16, 8192>(
                    size, steps, seed);
            } else {
                std::cerr << "77: n_avx_32, 16, 16, 8192" << std::endl;
                test<bv::small_bv<16, 8192, 16, false>, 1, 2, 16, 16, 8192>(
                    size, steps, seed);
            }
            break;
        case 78:
            if (avx) {
                std::cerr << "78: uint32_t, 16, 16, 16384" << std::endl;
                test<bv::small_bv<16, 16384, 16>, 1, 2, 16, 16, 16384>(
                    size, steps, seed);
            } else {
                std::cerr << "78: n_avx_32, 16, 16, 16384" << std::endl;
                test<bv::small_bv<16, 16384, 16, false>, 1, 2, 16, 16, 16384>(
                    size, steps, seed);
            }
            break;
        case 79:
            if (avx) {
                std::cerr << "79: uint32_t, 16, 16, 32768" << std::endl;
                test<bv::small_bv<16, 32768, 16>, 1, 2, 16, 16, 32768>(
                    size, steps, seed);
            } else {
                std::cerr << "79: n_avx_32, 16, 16, 32768" << std::endl;
                test<bv::small_bv<16, 32768, 16, false>, 1, 2, 16, 16, 32768>(
                    size, steps, seed);
            }
            break;
        case 80:
            if (avx) {
                std::cerr << "80: uint32_t, 16, 32, 4096" << std::endl;
                test<bv::small_bv<32, 4096, 16>, 1, 2, 32, 16, 4096>(
                    size, steps, seed);
            } else {
                std::cerr << "80: n_avx_32, 16, 32, 4096" << std::endl;
                test<bv::small_bv<32, 4096, 16, false>, 1, 2, 32, 16, 4096>(
                    size, steps, seed);
            }
            break;
        case 81:
            if (avx) {
                std::cerr << "81: uint32_t, 16, 32, 8192" << std::endl;
                test<bv::small_bv<32, 8192, 16>, 1, 2, 32, 16, 8192>(
                    size, steps, seed);
            } else {
                std::cerr << "81: n_avx_32, 16, 32, 8192" << std::endl;
                test<bv::small_bv<32, 8192, 16, false>, 1, 2, 32, 16, 8192>(
                    size, steps, seed);
            }
            break;
        case 82:
            if (avx) {
                std::cerr << "82: uint32_t, 16, 32, 16384" << std::endl;
                test<bv::small_bv<32, 16384, 16>, 1, 2, 32, 16, 16384>(
                    size, steps, seed);
            } else {
                std::cerr << "82: n_avx_32, 16, 32, 16384" << std::endl;
                test<bv::small_bv<32, 16384, 16, false>, 1, 2, 32, 16, 16384>(
                    size, steps, seed);
            }
            break;
        case 83:
            if (avx) {
                std::cerr << "83: uint32_t, 16, 32, 32768" << std::endl;
                test<bv::small_bv<32, 32768, 16>, 1, 2, 32, 16, 32768>(
                    size, steps, seed);
            } else {
                std::cerr << "83: n_avx_32, 16, 32, 32768" << std::endl;
                test<bv::small_bv<32, 32768, 16, false>, 1, 2, 32, 16, 32768>(
                    size, steps, seed);
            }
            break;
        case 84:
            if (avx) {
                std::cerr << "84: uint32_t, 32, 0, 4096" << std::endl;
                test<bv::small_bv<0, 4096, 32>, 1, 2, 0, 32, 4096>(size, steps,
                                                                   seed);
            } else {
                std::cerr << "84: n_avx_32, 32, 0, 4096" << std::endl;
                test<bv::small_bv<0, 4096, 32, false>, 1, 2, 0, 32, 4096>(
                    size, steps, seed);
            }
            break;
        case 85:
            if (avx) {
                std::cerr << "85: uint32_t, 32, 0, 8192" << std::endl;
                test<bv::small_bv<0, 8192, 32>, 1, 2, 0, 32, 8192>(size, steps,
                                                                   seed);
            } else {
                std::cerr << "85: n_avx_32, 32, 0, 8192" << std::endl;
                test<bv::small_bv<0, 8192, 32, false>, 1, 2, 0, 32, 8192>(
                    size, steps, seed);
            }
            break;
        case 86:
            if (avx) {
                std::cerr << "86: uint32_t, 32, 0, 16384" << std::endl;
                test<bv::small_bv<0, 16384, 32>, 1, 2, 0, 32, 16384>(
                    size, steps, seed);
            } else {
                std::cerr << "86: n_avx_32, 32, 0, 16384" << std::endl;
                test<bv::small_bv<0, 16384, 32, false>, 1, 2, 0, 32, 16384>(
                    size, steps, seed);
            }
            break;
        case 87:
            if (avx) {
                std::cerr << "87: uint32_t, 32, 0, 32768" << std::endl;
                test<bv::small_bv<0, 32768, 32>, 1, 2, 0, 32, 32768>(
                    size, steps, seed);
            } else {
                std::cerr << "87: n_avx_32, 32, 0, 32768" << std::endl;
                test<bv::small_bv<0, 32768, 32, false>, 1, 2, 0, 32, 32768>(
                    size, steps, seed);
            }
            break;
        case 88:
            if (avx) {
                std::cerr << "88: uint32_t, 32, 8, 4096" << std::endl;
                test<bv::small_bv<8, 4096, 32>, 1, 2, 8, 32, 4096>(size, steps,
                                                                   seed);
            } else {
                std::cerr << "88: n_avx_32, 32, 8, 4096" << std::endl;
                test<bv::small_bv<8, 4096, 32, false>, 1, 2, 8, 32, 4096>(
                    size, steps, seed);
            }
            break;
        case 89:
            if (avx) {
                std::cerr << "89: uint32_t, 32, 8, 8192" << std::endl;
                test<bv::small_bv<8, 8192, 32>, 1, 2, 8, 32, 8192>(size, steps,
                                                                   seed);
            } else {
                std::cerr << "89: n_avx_32, 32, 8, 8192" << std::endl;
                test<bv::small_bv<8, 8192, 32, false>, 1, 2, 8, 32, 8192>(
                    size, steps, seed);
            }
            break;
        case 90:
            if (avx) {
                std::cerr << "90: uint32_t, 32, 8, 16384" << std::endl;
                test<bv::small_bv<8, 16384, 32>, 1, 2, 8, 32, 16384>(
                    size, steps, seed);
            } else {
                std::cerr << "90: n_avx_32, 32, 8, 16384" << std::endl;
                test<bv::small_bv<8, 16384, 32, false>, 1, 2, 8, 32, 16384>(
                    size, steps, seed);
            }
            break;
        case 91:
            if (avx) {
                std::cerr << "91: uint32_t, 32, 8, 32768" << std::endl;
                test<bv::small_bv<8, 32768, 32>, 1, 2, 8, 32, 32768>(
                    size, steps, seed);
            } else {
                std::cerr << "91: n_avx_32, 32, 8, 32768" << std::endl;
                test<bv::small_bv<8, 32768, 32, false>, 1, 2, 8, 32, 32768>(
                    size, steps, seed);
            }
            break;
        case 92:
            if (avx) {
                std::cerr << "92: uint32_t, 32, 16, 4096" << std::endl;
                test<bv::small_bv<16, 4096, 32>, 1, 2, 16, 32, 4096>(
                    size, steps, seed);
            } else {
                std::cerr << "92: n_avx_32, 32, 16, 4096" << std::endl;
                test<bv::small_bv<16, 4096, 32, false>, 1, 2, 16, 32, 4096>(
                    size, steps, seed);
            }
            break;
        case 93:
            if (avx) {
                std::cerr << "93: uint32_t, 32, 16, 8192" << std::endl;
                test<bv::small_bv<16, 8192, 32>, 1, 2, 16, 32, 8192>(
                    size, steps, seed);
            } else {
                std::cerr << "93: n_avx_32, 32, 16, 8192" << std::endl;
                test<bv::small_bv<16, 8192, 32, false>, 1, 2, 16, 32, 8192>(
                    size, steps, seed);
            }
            break;
        case 94:
            if (avx) {
                std::cerr << "94: uint32_t, 32, 16, 16384" << std::endl;
                test<bv::small_bv<16, 16384, 32>, 1, 2, 16, 32, 16384>(
                    size, steps, seed);
            } else {
                std::cerr << "94: n_avx_32, 32, 16, 16384" << std::endl;
                test<bv::small_bv<16, 16384, 32, false>, 1, 2, 16, 32, 16384>(
                    size, steps, seed);
            }
            break;
        case 95:
            if (avx) {
                std::cerr << "95: uint32_t, 32, 16, 32768" << std::endl;
                test<bv::small_bv<16, 32768, 32>, 1, 2, 16, 32, 32768>(
                    size, steps, seed);
            } else {
                std::cerr << "95: n_avx_32, 32, 16, 32768" << std::endl;
                test<bv::small_bv<16, 32768, 32, false>, 1, 2, 16, 32, 32768>(
                    size, steps, seed);
            }
            break;
        case 96:
            if (avx) {
                std::cerr << "96: uint32_t, 32, 32, 4096" << std::endl;
                test<bv::small_bv<32, 4096, 32>, 1, 2, 32, 32, 4096>(
                    size, steps, seed);
            } else {
                std::cerr << "96: n_avx_32, 32, 32, 4096" << std::endl;
                test<bv::small_bv<32, 4096, 32, false>, 1, 2, 32, 32, 4096>(
                    size, steps, seed);
            }
            break;
        case 97:
            if (avx) {
                std::cerr << "97: uint32_t, 32, 32, 8192" << std::endl;
                test<bv::small_bv<32, 8192, 32>, 1, 2, 32, 32, 8192>(
                    size, steps, seed);
            } else {
                std::cerr << "97: n_avx_32, 32, 32, 8192" << std::endl;
                test<bv::small_bv<32, 8192, 32, false>, 1, 2, 32, 32, 8192>(
                    size, steps, seed);
            }
            break;
        case 98:
            if (avx) {
                std::cerr << "98: uint32_t, 32, 32, 16384" << std::endl;
                test<bv::small_bv<32, 16384, 32>, 1, 2, 32, 32, 16384>(
                    size, steps, seed);
            } else {
                std::cerr << "98: n_avx_32, 32, 32, 16384" << std::endl;
                test<bv::small_bv<32, 16384, 32, false>, 1, 2, 32, 32, 16384>(
                    size, steps, seed);
            }
            break;
        case 99:
            if (avx) {
                std::cerr << "99: uint32_t, 32, 32, 32768" << std::endl;
                test<bv::small_bv<32, 32768, 32>, 1, 2, 32, 32, 32768>(
                    size, steps, seed);
            } else {
                std::cerr << "99: n_avx_32, 32, 32, 32768" << std::endl;
                test<bv::small_bv<32, 32768, 32, false>, 1, 2, 32, 32, 32768>(
                    size, steps, seed);
            }
            break;
        case 100:
            if (avx) {
                std::cerr << "100: uint32_t, 64, 0, 4096" << std::endl;
                test<bv::small_bv<0, 4096, 64>, 1, 2, 0, 64, 4096>(size, steps,
                                                                   seed);
            } else {
                std::cerr << "100: n_avx_32, 64, 0, 4096" << std::endl;
                test<bv::small_bv<0, 4096, 64, false>, 1, 2, 0, 64, 4096>(
                    size, steps, seed);
            }
            break;
        case 101:
            if (avx) {
                std::cerr << "101: uint32_t, 64, 0, 8192" << std::endl;
                test<bv::small_bv<0, 8192, 64>, 1, 2, 0, 64, 8192>(size, steps,
                                                                   seed);
            } else {
                std::cerr << "101: n_avx_32, 64, 0, 8192" << std::endl;
                test<bv::small_bv<0, 8192, 64, false>, 1, 2, 0, 64, 8192>(
                    size, steps, seed);
            }
            break;
        case 102:
            if (avx) {
                std::cerr << "102: uint32_t, 64, 0, 16384" << std::endl;
                test<bv::small_bv<0, 16384, 64>, 1, 2, 0, 64, 16384>(
                    size, steps, seed);
            } else {
                std::cerr << "102: n_avx_32, 64, 0, 16384" << std::endl;
                test<bv::small_bv<0, 16384, 64, false>, 1, 2, 0, 64, 16384>(
                    size, steps, seed);
            }
            break;
        case 103:
            if (avx) {
                std::cerr << "103: uint32_t, 64, 0, 32768" << std::endl;
                test<bv::small_bv<0, 32768, 64>, 1, 2, 0, 64, 32768>(
                    size, steps, seed);
            } else {
                std::cerr << "103: n_avx_32, 64, 0, 32768" << std::endl;
                test<bv::small_bv<0, 32768, 64, false>, 1, 2, 0, 64, 32768>(
                    size, steps, seed);
            }
            break;
        case 104:
            if (avx) {
                std::cerr << "104: uint32_t, 64, 8, 4096" << std::endl;
                test<bv::small_bv<8, 4096, 64>, 1, 2, 8, 64, 4096>(size, steps,
                                                                   seed);
            } else {
                std::cerr << "104: n_avx_32, 64, 8, 4096" << std::endl;
                test<bv::small_bv<8, 4096, 64, false>, 1, 2, 8, 64, 4096>(
                    size, steps, seed);
            }
            break;
        case 105:
            if (avx) {
                std::cerr << "105: uint32_t, 64, 8, 8192" << std::endl;
                test<bv::small_bv<8, 8192, 64>, 1, 2, 8, 64, 8192>(size, steps,
                                                                   seed);
            } else {
                std::cerr << "105: n_avx_32, 64, 8, 8192" << std::endl;
                test<bv::small_bv<8, 8192, 64, false>, 1, 2, 8, 64, 8192>(
                    size, steps, seed);
            }
            break;
        case 106:
            if (avx) {
                std::cerr << "106: uint32_t, 64, 8, 16384" << std::endl;
                test<bv::small_bv<8, 16384, 64>, 1, 2, 8, 64, 16384>(
                    size, steps, seed);
            } else {
                std::cerr << "106: n_avx_32, 64, 8, 16384" << std::endl;
                test<bv::small_bv<8, 16384, 64, false>, 1, 2, 8, 64, 16384>(
                    size, steps, seed);
            }
            break;
        case 107:
            if (avx) {
                std::cerr << "107: uint32_t, 64, 8, 32768" << std::endl;
                test<bv::small_bv<8, 32768, 64>, 1, 2, 8, 64, 32768>(
                    size, steps, seed);
            } else {
                std::cerr << "107: n_avx_32, 64, 8, 32768" << std::endl;
                test<bv::small_bv<8, 32768, 64, false>, 1, 2, 8, 64, 32768>(
                    size, steps, seed);
            }
            break;
        case 108:
            if (avx) {
                std::cerr << "108: uint32_t, 64, 16, 4096" << std::endl;
                test<bv::small_bv<16, 4096, 64>, 1, 2, 16, 64, 4096>(
                    size, steps, seed);
            } else {
                std::cerr << "108: n_avx_32, 64, 16, 4096" << std::endl;
                test<bv::small_bv<16, 4096, 64, false>, 1, 2, 16, 64, 4096>(
                    size, steps, seed);
            }
            break;
        case 109:
            if (avx) {
                std::cerr << "109: uint32_t, 64, 16, 8192" << std::endl;
                test<bv::small_bv<16, 8192, 64>, 1, 2, 16, 64, 8192>(
                    size, steps, seed);
            } else {
                std::cerr << "109: n_avx_32, 64, 16, 8192" << std::endl;
                test<bv::small_bv<16, 8192, 64, false>, 1, 2, 16, 64, 8192>(
                    size, steps, seed);
            }
            break;
        case 110:
            if (avx) {
                std::cerr << "110: uint32_t, 64, 16, 16384" << std::endl;
                test<bv::small_bv<16, 16384, 64>, 1, 2, 16, 64, 16384>(
                    size, steps, seed);
            } else {
                std::cerr << "110: n_avx_32, 64, 16, 16384" << std::endl;
                test<bv::small_bv<16, 16384, 64, false>, 1, 2, 16, 64, 16384>(
                    size, steps, seed);
            }
            break;
        case 111:
            if (avx) {
                std::cerr << "111: uint32_t, 64, 16, 32768" << std::endl;
                test<bv::small_bv<16, 32768, 64>, 1, 2, 16, 64, 32768>(
                    size, steps, seed);
            } else {
                std::cerr << "111: n_avx_32, 64, 16, 32768" << std::endl;
                test<bv::small_bv<16, 32768, 64, false>, 1, 2, 16, 64, 32768>(
                    size, steps, seed);
            }
            break;
        case 112:
            if (avx) {
                std::cerr << "112: uint32_t, 64, 32, 4096" << std::endl;
                test<bv::small_bv<32, 4096, 64>, 1, 2, 32, 64, 4096>(
                    size, steps, seed);
            } else {
                std::cerr << "112: n_avx_32, 64, 32, 4096" << std::endl;
                test<bv::small_bv<32, 4096, 64, false>, 1, 2, 32, 64, 4096>(
                    size, steps, seed);
            }
            break;
        case 113:
            if (avx) {
                std::cerr << "113: uint32_t, 64, 32, 8192" << std::endl;
                test<bv::small_bv<32, 8192, 64>, 1, 2, 32, 64, 8192>(
                    size, steps, seed);
            } else {
                std::cerr << "113: n_avx_32, 64, 32, 8192" << std::endl;
                test<bv::small_bv<32, 8192, 64, false>, 1, 2, 32, 64, 8192>(
                    size, steps, seed);
            }
            break;
        case 114:
            if (avx) {
                std::cerr << "114: uint32_t, 64, 32, 16384" << std::endl;
                test<bv::small_bv<32, 16384, 64>, 1, 2, 32, 64, 16384>(
                    size, steps, seed);
            } else {
                std::cerr << "114: n_avx_32, 64, 32, 16384" << std::endl;
                test<bv::small_bv<32, 16384, 64, false>, 1, 2, 32, 64, 16384>(
                    size, steps, seed);
            }
            break;
        case 115:
            if (avx) {
                std::cerr << "115: uint32_t, 64, 32, 32768" << std::endl;
                test<bv::small_bv<32, 32768, 64>, 1, 2, 32, 64, 32768>(
                    size, steps, seed);
            } else {
                std::cerr << "115: n_avx_32, 64, 32, 32768" << std::endl;
                test<bv::small_bv<32, 32768, 64, false>, 1, 2, 32, 64, 32768>(
                    size, steps, seed);
            }
            break;
        case 116:
            if (avx) {
                std::cerr << "116: uint32_t, 128, 0, 4096" << std::endl;
                test<bv::small_bv<0, 4096, 128>, 1, 2, 0, 128, 4096>(
                    size, steps, seed);
            } else {
                std::cerr << "116: n_avx_32, 128, 0, 4096" << std::endl;
                test<bv::small_bv<0, 4096, 128, false>, 1, 2, 0, 128, 4096>(
                    size, steps, seed);
            }
            break;
        case 117:
            if (avx) {
                std::cerr << "117: uint32_t, 128, 0, 8192" << std::endl;
                test<bv::small_bv<0, 8192, 128>, 1, 2, 0, 128, 8192>(
                    size, steps, seed);
            } else {
                std::cerr << "117: n_avx_32, 128, 0, 8192" << std::endl;
                test<bv::small_bv<0, 8192, 128, false>, 1, 2, 0, 128, 8192>(
                    size, steps, seed);
            }
            break;
        case 118:
            if (avx) {
                std::cerr << "118: uint32_t, 128, 0, 16384" << std::endl;
                test<bv::small_bv<0, 16384, 128>, 1, 2, 0, 128, 16384>(
                    size, steps, seed);
            } else {
                std::cerr << "118: n_avx_32, 128, 0, 16384" << std::endl;
                test<bv::small_bv<0, 16384, 128, false>, 1, 2, 0, 128, 16384>(
                    size, steps, seed);
            }
            break;
        case 119:
            if (avx) {
                std::cerr << "119: uint32_t, 128, 0, 32768" << std::endl;
                test<bv::small_bv<0, 32768, 128>, 1, 2, 0, 128, 32768>(
                    size, steps, seed);
            } else {
                std::cerr << "119: n_avx_32, 128, 0, 32768" << std::endl;
                test<bv::small_bv<0, 32768, 128, false>, 1, 2, 0, 128, 32768>(
                    size, steps, seed);
            }
            break;
        case 120:
            if (avx) {
                std::cerr << "120: uint32_t, 128, 8, 4096" << std::endl;
                test<bv::small_bv<8, 4096, 128>, 1, 2, 8, 128, 4096>(
                    size, steps, seed);
            } else {
                std::cerr << "120: n_avx_32, 128, 8, 4096" << std::endl;
                test<bv::small_bv<8, 4096, 128, false>, 1, 2, 8, 128, 4096>(
                    size, steps, seed);
            }
            break;
        case 121:
            if (avx) {
                std::cerr << "121: uint32_t, 128, 8, 8192" << std::endl;
                test<bv::small_bv<8, 8192, 128>, 1, 2, 8, 128, 8192>(
                    size, steps, seed);
            } else {
                std::cerr << "121: n_avx_32, 128, 8, 8192" << std::endl;
                test<bv::small_bv<8, 8192, 128, false>, 1, 2, 8, 128, 8192>(
                    size, steps, seed);
            }
            break;
        case 122:
            if (avx) {
                std::cerr << "122: uint32_t, 128, 8, 16384" << std::endl;
                test<bv::small_bv<8, 16384, 128>, 1, 2, 8, 128, 16384>(
                    size, steps, seed);
            } else {
                std::cerr << "122: n_avx_32, 128, 8, 16384" << std::endl;
                test<bv::small_bv<8, 16384, 128, false>, 1, 2, 8, 128, 16384>(
                    size, steps, seed);
            }
            break;
        case 123:
            if (avx) {
                std::cerr << "123: uint32_t, 128, 8, 32768" << std::endl;
                test<bv::small_bv<8, 32768, 128>, 1, 2, 8, 128, 32768>(
                    size, steps, seed);
            } else {
                std::cerr << "123: n_avx_32, 128, 8, 32768" << std::endl;
                test<bv::small_bv<8, 32768, 128, false>, 1, 2, 8, 128, 32768>(
                    size, steps, seed);
            }
            break;
        case 124:
            if (avx) {
                std::cerr << "124: uint32_t, 128, 16, 4096" << std::endl;
                test<bv::small_bv<16, 4096, 128>, 1, 2, 16, 128, 4096>(
                    size, steps, seed);
            } else {
                std::cerr << "124: n_avx_32, 128, 16, 4096" << std::endl;
                test<bv::small_bv<16, 4096, 128, false>, 1, 2, 16, 128, 4096>(
                    size, steps, seed);
            }
            break;
        case 125:
            if (avx) {
                std::cerr << "125: uint32_t, 128, 16, 8192" << std::endl;
                test<bv::small_bv<16, 8192, 128>, 1, 2, 16, 128, 8192>(
                    size, steps, seed);
            } else {
                std::cerr << "125: n_avx_32, 128, 16, 8192" << std::endl;
                test<bv::small_bv<16, 8192, 128, false>, 1, 2, 16, 128, 8192>(
                    size, steps, seed);
            }
            break;
        case 126:
            if (avx) {
                std::cerr << "126: uint32_t, 128, 16, 16384" << std::endl;
                test<bv::small_bv<16, 16384, 128>, 1, 2, 16, 128, 16384>(
                    size, steps, seed);
            } else {
                std::cerr << "126: n_avx_32, 128, 16, 16384" << std::endl;
                test<bv::small_bv<16, 16384, 128, false>, 1, 2, 16, 128, 16384>(
                    size, steps, seed);
            }
            break;
        case 127:
            if (avx) {
                std::cerr << "127: uint32_t, 128, 16, 32768" << std::endl;
                test<bv::small_bv<16, 32768, 128>, 1, 2, 16, 128, 32768>(
                    size, steps, seed);
            } else {
                std::cerr << "127: n_avx_32, 128, 16, 32768" << std::endl;
                test<bv::small_bv<16, 32768, 128, false>, 1, 2, 16, 128, 32768>(
                    size, steps, seed);
            }
            break;
        case 128:
            if (avx) {
                std::cerr << "128: uint32_t, 128, 32, 4096" << std::endl;
                test<bv::small_bv<32, 4096, 128>, 1, 2, 32, 128, 4096>(
                    size, steps, seed);
            } else {
                std::cerr << "128: n_avx_32, 128, 32, 4096" << std::endl;
                test<bv::small_bv<32, 4096, 128, false>, 1, 2, 32, 128, 4096>(
                    size, steps, seed);
            }
            break;
        case 129:
            if (avx) {
                std::cerr << "129: uint32_t, 128, 32, 8192" << std::endl;
                test<bv::small_bv<32, 8192, 128>, 1, 2, 32, 128, 8192>(
                    size, steps, seed);
            } else {
                std::cerr << "129: n_avx_32, 128, 32, 8192" << std::endl;
                test<bv::small_bv<32, 8192, 128, false>, 1, 2, 32, 128, 8192>(
                    size, steps, seed);
            }
            break;
        case 130:
            if (avx) {
                std::cerr << "130: uint32_t, 128, 32, 16384" << std::endl;
                test<bv::small_bv<32, 16384, 128>, 1, 2, 32, 128, 16384>(
                    size, steps, seed);
            } else {
                std::cerr << "130: n_avx_32, 128, 32, 16384" << std::endl;
                test<bv::small_bv<32, 16384, 128, false>, 1, 2, 32, 128, 16384>(
                    size, steps, seed);
            }
            break;
        default:
            if (avx) {
                std::cerr << "131: uint32_t, 128, 32, 32768" << std::endl;
                test<bv::small_bv<32, 32768, 128>, 1, 2, 32, 128, 32768>(
                    size, steps, seed);
            } else {
                std::cerr << "131: n_avx_32, 128, 32, 32768" << std::endl;
                test<bv::small_bv<32, 32768, 128, false>, 1, 2, 32, 128, 32768>(
                    size, steps, seed);
            }
            break;
    }
    return 0;
}
