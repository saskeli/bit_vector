#include <sys/resource.h>

#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>

#include "bit_vector/bv.hpp"

#include "bit_vector/internal/gcc_pragmas.hpp"
#include "deps/valgrind/callgrind.h"
#include "dynamic/dynamic.hpp"
#include "sdsl/bit_vectors.hpp"
#pragma GCC diagnostic pop

#include "bit_vector/internal/deb.hpp"

void test_sdsl(uint64_t size, uint64_t steps, uint64_t seed) {
    bv::simple_bv<16, 16384, 64, true, true> bv;

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
        << "buffer\tbranch\tleaf_size\tseed\tsize\tremove\tinsert\tflush\tset\t"
        << "access\trank\tselect\tsize(bits)\trss\tusage\tchecksum"
        << std::endl;

    for (uint64_t i = 0; i < 900000; i++) {
        uint64_t aloc = gen(mt) % (i + 1);
        bool aval = gen(mt) % 2;
        bv.insert(aloc, aval);
    }
    for (uint64_t step = 1; step <= steps; step++) {
        uint64_t checksum = 0;
        uint64_t start = bv.size();
        uint64_t target = uint64_t(pow(2.0, startexp + delta * step));

        std::cout << "16\t64\t16384\t" << seed << "\t" << target << "\t";

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

        t1 = high_resolution_clock::now();
        sdsl::bit_vector sdsl_bv(bv.size());
        bv.dump(sdsl_bv.data());
        t2 = high_resolution_clock::now();
        double f_time = (double)duration_cast<microseconds>(t2 - t1).count();

        loc.clear();
        val.clear();
        for (uint64_t i = 0; i < ops; i++) {
            loc.push_back(gen(mt) % target);
            val.push_back(gen(mt) % 2);
        }

        t1 = high_resolution_clock::now();
        for (size_t i = 0; i < loc.size(); i++) {
            sdsl_bv[loc[i]] = val[i];
        }
        t2 = high_resolution_clock::now();
        for (size_t i = 0; i < loc.size(); i++) {
            bv.set(loc[i], val[i]);
        }
        double t_set =
            (double)duration_cast<microseconds>(t2 - t1).count() / ops;

        t1 = high_resolution_clock::now();
        sdsl::bit_vector::rank_1_type sdsl_rank(&sdsl_bv);
        sdsl::bit_vector::select_1_type sdsl_select(&sdsl_bv);
        t2 = high_resolution_clock::now();
        f_time += (double)duration_cast<microseconds>(t2 - t1).count();
        std::cout << f_time << "\t" << t_set << "\t";

        loc.clear();
        for (size_t i = 0; i < ops; i++) {
            loc.push_back(gen(mt) % target);
        }

        t1 = high_resolution_clock::now();
        //*
        for (size_t i = 0; i < ops; i++) {
            checksum += sdsl_bv[loc[i]];
        }  //*/
        t2 = high_resolution_clock::now();
        std::cout << (double)duration_cast<microseconds>(t2 - t1).count() / ops
                  << "\t";

        loc.clear();
        for (size_t i = 0; i < ops; i++) {
            loc.push_back(gen(mt) % target);
        }

        t1 = high_resolution_clock::now();
        //*
        for (size_t i = 0; i < ops; i++) {
            checksum += sdsl_rank(loc[i]);
        }  //*/
        t2 = high_resolution_clock::now();
        std::cout << (double)duration_cast<microseconds>(t2 - t1).count() / ops
                  << "\t";

        uint64_t limit = sdsl_rank(target - 1);
        loc.clear();
        for (size_t i = 0; i < ops; i++) {
            loc.push_back(gen(mt) % limit);
        }

        t1 = high_resolution_clock::now();
        //*
        for (size_t i = 0; i < ops; i++) {
            checksum += sdsl_select(loc[i] + 1);
        }  //*/
        t2 = high_resolution_clock::now();
        std::cout << (double)duration_cast<microseconds>(t2 - t1).count() / ops
                  << "\t";

        std::cout << 8 * (sdsl::size_in_bytes(sdsl_bv) +
                          sdsl::size_in_bytes(sdsl_rank) +
                          sdsl::size_in_bytes(sdsl_select)) +
                         bv.bit_size()
                  << "\t";
        getrusage(RUSAGE_SELF, &ru);
        std::cout << ru.ru_maxrss * 8 * 1024 << "\t";
        std::cout << "1\t";

        std::cout << checksum << std::endl;
    }
}

template <class bit_vector, uint64_t select_offset, uint16_t buffer,
          uint8_t branch, uint32_t leaf_size, bool flush = false>
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
        << "buffer\tbranch\tleaf_size\tseed\tsize\tremove\tinsert\tset\tflush\t"
        << "access\trank\tselect\tsize(bits)\trss\tusage\tchecksum"
        << std::endl;

    for (uint64_t i = 0; i < 900000; i++) {
        uint64_t aloc = gen(mt) % (i + 1);
        bool aval = gen(mt) % 2;
        bv.insert(aloc, aval);
    }
    for (uint64_t step = 1; step <= steps; step++) {
        uint64_t checksum = 0;
        uint64_t start = bv.size();
        uint64_t target = uint64_t(pow(2.0, startexp + delta * step));

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

        t1 = high_resolution_clock::now();
        if constexpr (flush) {
            bv.flush();
        }
        t2 = high_resolution_clock::now();
        double f_timing = (double)duration_cast<microseconds>(t2 - t1).count();

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

        std::cout << f_timing << "\t";

        loc.clear();
        for (size_t i = 0; i < ops; i++) {
            loc.push_back(gen(mt) % target);
        }

        t1 = high_resolution_clock::now();
        //*
        for (size_t i = 0; i < ops; i++) {
            checksum += bv.at(loc[i]);
        }  //*/
        t2 = high_resolution_clock::now();
        std::cout << (double)duration_cast<microseconds>(t2 - t1).count() / ops
                  << "\t";

        loc.clear();
        for (size_t i = 0; i < ops; i++) {
            loc.push_back(gen(mt) % target);
        }

        t1 = high_resolution_clock::now();
        //*
        for (size_t i = 0; i < ops; i++) {
            checksum += bv.rank(loc[i]);
        }  //*/
        t2 = high_resolution_clock::now();
        std::cout << (double)duration_cast<microseconds>(t2 - t1).count() / ops
                  << "\t";

        uint64_t limit = bv.rank(target - 1);
        loc.clear();
        for (size_t i = 0; i < ops; i++) {
            uint64_t p_val = gen(mt) % limit;
            loc.push_back(p_val);
        }

        t1 = high_resolution_clock::now();
        //*
        for (size_t i = 0; i < ops; i++) {
            checksum += bv.select(loc[i] + select_offset);
        }  //*/
        t2 = high_resolution_clock::now();
        std::cout << (double)duration_cast<microseconds>(t2 - t1).count() / ops
                  << "\t";

        std::cout << bv.bit_size() << "\t";
        getrusage(RUSAGE_SELF, &ru);
        std::cout << ru.ru_maxrss * 8 * 1024 << "\t";
        if constexpr (select_offset == 0) {
            std::cout << "0\t";
        } else {
            std::cout << bv.leaf_usage() << "\t";
        }

        std::cout << checksum << std::endl;
    }
}

void help() {
    std::cout << "Benchmark some dynamic bit vectors.\n"
              << "Type and seed is required.\n"
              << "Size should be at least 10^7 and defaults to 10^7\n"
              << "Steps defaults to 100.\n\n";
    std::cout << "Usage: bench <type> <seed> <size> <steps>\n";
    std::cout << "   <type>   0 for dynamic\n"
              << "            1 for new implementation\n"
              << "            2 for new implementation buffer 8\n"
              << "            3 for new implementation with no buffer\n"
              << "            4 for sdsl bit_vector\n"
              << "            5 for tree with hybrid rlz leaves\n"
              << "            6 for implementation with unsorted buffers\n";
    std::cout << "   <seed>   seed to use for running the test\n";
    std::cout << "   <size>   number of bits in the bitvector\n";
    std::cout << "   <steps>  How many data points to generate in the "
                 "[10^6..size] range\n\n";
    std::cout << "Example: bench 1 1337 1000000 100" << std::endl;
    exit(0);
}

int main(int argc, char const* argv[]) {
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

    if (type == 0) {
        std::cerr << "DYNAMIC, 8, 0, 8192" << std::endl;
        test<dyn::suc_bv, 0, 0, 8, 8192>(size, steps, seed);
    } else if (type == 1) {
        std::cerr << "uint64_t, 64, 16, 16384" << std::endl;
        test<bv::simple_bv<16, 16384, 64, true, true>, 1, 16, 64, 16384>(
            size, steps, seed);
    } else if (type == 2) {
        std::cerr << "uint64_t, 64, 16, 16384" << std::endl;
        test<bv::simple_bv<8, 16384, 64, true, true>, 1, 8, 64, 16384>(
            size, steps, seed);
    } else if (type == 3) {
        std::cerr << "uint64_t, 64, 0, 16384" << std::endl;
        test<bv::simple_bv<0, 16384, 64, true, true>, 1, 0, 64, 16384>(
            size, steps, seed);
    } else if (type == 4) {
        std::cerr << "uint64_t, 0, 0, 0" << std::endl;
        test_sdsl(size, steps, seed);
    } else if (type == 5) {
        std::cerr << "uint64_t, 64, 16, 16384" << std::endl;
        test<bv::simple_bv<16, 16384, 64, true, true, true>, 1, 16, 64, 16384,
             false>(size, steps, seed);
    } else if (type == 6) {
        std::cerr << "unsorted, 64, 512, 16384" << std::endl;
        test<bv::simple_bv<512, 16384, 64, true, true, false, false>, 1, 512, 64,
             16384, false>(size, steps, seed);
    }
    return 0;
}
