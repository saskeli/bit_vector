#include <sys/resource.h>

#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <cmath>

#include "bit_vector/bv.hpp"

bool get_val(uint64_t i, uint64_t lim, uint64_t val) {
    double x = ((20.0 * i) / lim) - 10.0;
    return val < atan(x) / M_PI_2;
}

template <class bit_vector>
void test(uint64_t size, uint64_t steps, uint64_t seed) {
    bit_vector bv;

    std::mt19937 mt(seed);
    std::uniform_int_distribution<unsigned long long> gen(
        std::numeric_limits<std::uint64_t>::min(),
        std::numeric_limits<std::uint64_t>::max());
    std::uniform_real_distribution dist(-1.0, 1.0);

    using std::chrono::duration_cast;
    using std::chrono::high_resolution_clock;
    using std::chrono::microseconds;

    struct rusage ru;

    uint64_t start_size = 1000000;

    double startexp = log2(double(start_size));
    double delta = (log2(double(size)) - log2(double(start_size))) / steps;
    uint64_t ops = 100000;
    std::cerr << "seed: " << seed << ". startexp: " << startexp << ". delta: " << delta << std::endl;
    std::vector<uint64_t> loc, val;

    std::cout << "seed\tsize\tremove\tinsert\tset\taccess\trank\tselect\tsize("
                 "bits)\trss\tchecksum" << std::endl;

    for (uint64_t i = 0; i < 900000; i++) {
        uint64_t aloc = gen(mt) % (i + 1);
        bool aval = get_val(aloc, i + 1, dist(mt));
        bv.insert(aloc, aval);
    }
    for (uint64_t step = 1; step <= steps; step++) {
        uint64_t checksum = 0;
        uint64_t start = bv.size();
        uint64_t target = uint64_t(pow(2.0, startexp + delta * step));

        std::cout << seed << "\t" << target << "\t" << std::flush;

        for (size_t i = start; i < target; i++) {
            uint64_t aloc = gen(mt) % (i + 1);
            bool aval = get_val(aloc, i + 1, dist(mt));
            //if (i == 72515787) {
            //    std::cout << "Inserting " << aval << " to position " << aloc << std::endl;
            //    //bv.print();
            //}
            bv.insert(aloc, aval);
            //if (i == 72515787) {
            //    //bv.print();
            //    std::cout << "Validating" << std::endl;
            //    bv.validate();
            //}
            //if (i > 72443596) {
            //    std::cerr << i << std::endl;
            //    bv.validate();
            //}
        }
        
        bv.validate();

        loc.clear();
        for (uint64_t i = target; i > target - ops; i--) {
            loc.push_back(gen(mt) % i);
        }

        auto t1 = high_resolution_clock::now();
        for (size_t i = 0; i < ops; i++) {
            //if (target == 11220184 && i == 4876) {
            //    std::cout << "Remove " << i << " targetting " << loc[i] << std::endl;
            //    //bv.print();
            //}
            bv.remove(loc[i]);
            //if (target == 11220184 && i == 4876) {
            //    //bv.print();
            //    bv.validate();
            //}
        }
        auto t2 = high_resolution_clock::now();
        std::cout << (double)duration_cast<microseconds>(t2 - t1).count() / ops
                  << "\t" << std::flush;
        bv.validate();

        loc.clear();
        val.clear();
        for (uint64_t i = bv.size(); i < target; i++) {
            uint64_t aloc = gen(mt) % (i + 1);
            loc.push_back(aloc);
            val.push_back(get_val(aloc, i, dist(mt)));
        }

        t1 = high_resolution_clock::now();
        for (size_t i = 0; i < loc.size(); i++) {
            bv.insert(loc[i], val[i]);
        }
        t2 = high_resolution_clock::now();
        std::cout << (double)duration_cast<microseconds>(t2 - t1).count() / ops
                  << "\t" << std::flush;
        bv.validate();

        loc.clear();
        val.clear();
        for (uint64_t i = 0; i < ops; i++) {
            uint64_t aloc = gen(mt) % target;
            loc.push_back(aloc);
            val.push_back(get_val(aloc, target, dist(mt)));
        }

        t1 = high_resolution_clock::now();
        for (size_t i = 0; i < loc.size(); i++) {
            bv.set(loc[i], val[i]);
        }
        t2 = high_resolution_clock::now();
        std::cout << (double)duration_cast<microseconds>(t2 - t1).count() / ops
                  << "\t" << std::flush;
        bv.validate();

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
                  << "\t" << std::flush;

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
                  << "\t" << std::flush;

        uint64_t limit = bv.rank(target - 1);
        loc.clear();
        for (size_t i = 0; i < ops; i++) {
            uint64_t val = gen(mt) % limit;
            loc.push_back(val);
        }

        t1 = high_resolution_clock::now();
        //*
        for (size_t i = 0; i < ops; i++) {
            checksum += bv.select(loc[i] + 1);
        }  //*/
        t2 = high_resolution_clock::now();
        std::cout << (double)duration_cast<microseconds>(t2 - t1).count() / ops
                  << "\t" << std::flush;

        std::cout << bv.bit_size() << "\t";
        getrusage(RUSAGE_SELF, &ru);
        std::cout << ru.ru_maxrss * 8 * 1024 << "\t";
        //std::cout << bv.leaf_usage() << "\t";

        std::cout << checksum << std::endl;
    }
    //bv.print();
}

void help() {
    std::cout << "Benchmark bit vectors with non-uniform random input.\n"
              << "Type and seed is required.\n"
              << "Size should be at least 10^7 and defaults to 10^7\n"
              << "Steps defaults to 100.\n\n";
    std::cout << "Usage: bench <type> <seed> <size> <steps>\n";
    std::cout << "   <type>   0 for uncompressed buffered bv\n"
              << "            1 for hybrid-rle bv\n"
              << "            3 for small hybrid-rle bv\n";
    std::cout << "   <seed>   seed to use for running the test\n";
    std::cout << "   <size>   number of bits in the bitvector\n";
    std::cout << "   <steps>  How many data points to generate in the "
                 "[10^6..size] range\n\n";
    std::cout << "Example: bench 0 1337 10000000 100" << std::endl;
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
        test<bv::bv>(size, steps, seed);
    } else if (type == 1) {
        test<bv::simple_bv<16, 16384, 64, true, true, true>>(size, steps, seed);
    } else {
        test<bv::simple_bv<4, 2048, 64, true, true, true>>(size, steps, seed);
    } 

    return 0;
}