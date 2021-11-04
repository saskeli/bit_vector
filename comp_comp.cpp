#include <sys/resource.h>

#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <cmath>

#include "bit_vector/bv.hpp"
#include "bit_vector/internal/deb.hpp"

template <class bva, class bvb>
void check(bva* a, bvb* b, uint64_t s_offset = 0) {
    uint64_t size = a->size();
    assert(size == b->size());
    for (uint64_t i = 0; i < size; i++) {
        if (a->at(i) != b->at(i)) {
            std::cerr << "a->at(" << i << ") = " << a->at(i) << ", b->at(" << i
                      << ") = " << b->at(i) << std::endl;
            assert(a->at(i) == b->at(i));
        }
        assert(a->rank(i) == b->rank(i));
    }

    uint64_t ones = a->rank(size);
    assert(b->rank(size) == ones);
    for (uint64_t i = 0; i < ones; i++) {
        uint64_t ex = b->select(i + s_offset);
        uint64_t ac = a->select(i + 1);
        if (ex != ac) {
            std::cerr << "select(" << i + 1 << ") should be " << ex
                      << ", was " << ac << std::endl;
        }
        assert(ac == ex);
    }
}

bool get_val(uint64_t i, uint64_t lim, uint64_t val) {
    double x = ((20.0 * i) / lim) - 10.0;
    return val < atan(x) / M_PI_2;
}

template <class bit_vector>
void test(uint64_t size, uint64_t steps, uint64_t seed) {
    uint64_t threshold = 69183097;
    bit_vector bv;
    bv::bv control;

    std::mt19937 mt(seed);
    std::uniform_int_distribution<unsigned long long> gen(
        std::numeric_limits<std::uint64_t>::min(),
        std::numeric_limits<std::uint64_t>::max());
    std::uniform_real_distribution dist(-1.0, 1.0);

    using std::chrono::duration_cast;
    using std::chrono::high_resolution_clock;
    using std::chrono::microseconds;

    uint64_t start_size = 1000000;

    double startexp = log2(double(start_size));
    double delta = (log2(double(size)) - log2(double(start_size))) / steps;
    uint64_t ops = 100000;
    std::cerr << "startexp: " << startexp << ". delta: " << delta << std::endl;
    std::vector<uint64_t> loc, val;

    for (uint64_t i = 0; i < 900000; i++) {
        uint64_t aloc = gen(mt) % (i + 1);
        bool aval = get_val(aloc, i + 1, dist(mt));
        bv.insert(aloc, aval);
        control.insert(aloc, aval);
    }
    for (uint64_t step = 1; step <= steps; step++) {
        uint64_t start = bv.size();
        uint64_t target = uint64_t(pow(2.0, startexp + delta * step));

        std::cerr << "sizing to " << target;
        for (size_t i = start; i < target; i++) {
            uint64_t aloc = gen(mt) % (i + 1);
            bool aval = get_val(aloc, i + 1, dist(mt));
            if (i == 69133053) {
                bv::do_debug = true;
                //bv.print();
                std::cout << "\nInserting " << aval << " to position " << aloc << std::endl;
            }
            bv.insert(aloc, aval);
            if (bv::do_debug) {
                std::cout << "===============================================================" << std::endl;
            }
            control.insert(aloc, aval);
            if (i == 69133053) {
                //bv.print();
                bv::do_debug = false;
                std::cout << "Validating" << std::endl;
                bv.validate();
                check(&bv, &control, 1);
            }
            //if (i > 69133050) {
            //    std::cerr << "i = " << i << std::endl;
            //    check(&bv, &control, 1);
            //}
        }

        std::cerr << " ... ";
        if (target >= threshold) {
            check(&bv, &control, 1);
        }
        std::cerr << "OK" << std::endl;

        loc.clear();
        for (uint64_t i = target; i > target - ops; i--) {
            loc.push_back(gen(mt) % i);
        }
        std::cerr << "remove ";
        for (size_t i = 0; i < ops; i++) {
            //if (target == 11220184 && i == 4876) {
            //    std::cout << "Remove " << i << " targetting " << loc[i] << std::endl;
            //    bv.print();
            //}
            bv.remove(loc[i]);
            control.remove(loc[i]);
            //if (target == 11220184 && i == 4876) {
            //    bv.print();
            //    bv.validate();
            //}
        }
        std::cerr << " ... ";
        if (target >= threshold) {
            check(&bv, &control, 1);
        }
        std::cerr << "OK" << std::endl;

        loc.clear();
        val.clear();
        for (uint64_t i = bv.size(); i < target; i++) {
            uint64_t aloc = gen(mt) % (i + 1);
            loc.push_back(aloc);
            val.push_back(get_val(aloc, i, dist(mt)));
        }
        std::cerr << "insert ";
        for (size_t i = 0; i < loc.size(); i++) {
            bv.insert(loc[i], val[i]);
            control.insert(loc[i], val[i]);
        }
        std::cerr << " ... ";
        if (target >= threshold) {
            check(&bv, &control, 1);
        }
        std::cerr << "OK" << std::endl;

        loc.clear();
        val.clear();
        for (uint64_t i = 0; i < ops; i++) {
            uint64_t aloc = gen(mt) % target;
            loc.push_back(aloc);
            val.push_back(get_val(aloc, target, dist(mt)));
        }
        std::cerr << "set ";
        for (size_t i = 0; i < loc.size(); i++) {
            bv.set(loc[i], val[i]);
            control.set(loc[i], val[i]);
        }
        std::cerr << " ... ";
        if (target >= threshold) {
            check(&bv, &control, 1);
        }
        std::cerr << "OK" << std::endl;

        uint64_t checksum = 0;

        std::cerr << "at: ";
        loc.clear();
        for (size_t i = 0; i < ops; i++) {
            loc.push_back(gen(mt) % target);
        }
        for (size_t i = 0; i < ops; i++) {
            checksum += bv.at(loc[i]);
            checksum -= bv.at(loc[i]);
        }
        std::cerr << checksum << std::endl;

        std::cerr << "rank: ";
        loc.clear();
        for (size_t i = 0; i < ops; i++) {
            loc.push_back(gen(mt) % target);
        }
        for (size_t i = 0; i < ops; i++) {
            checksum += bv.rank(loc[i]);
            checksum -= bv.rank(loc[i]);
        }
        std::cerr << checksum << std::endl;

        std::cerr << "select: ";
        uint64_t limit = bv.rank(target - 1);
        loc.clear();
        for (size_t i = 0; i < ops; i++) {
            uint64_t val = gen(mt) % limit;
            loc.push_back(val);
        }
        for (size_t i = 0; i < ops; i++) {
            checksum += bv.select(loc[i] + 1);
            checksum -= control.select(loc[i] + 1);
        }
        std::cerr << checksum << std::endl;
    }
}

void help() {
    std::cout << "Benchmark bit vectors with non-uniform random input.\n"
              << "Type and seed is required.\n"
              << "Size should be at least 10^7 and defaults to 10^7\n"
              << "Steps defaults to 100.\n\n";
    std::cout << "Usage: bench <type> <seed> <size> <steps>\n";
    std::cout << "   <type>   0 for uncompressed buffered bv\n"
              << "            1 for hybrid-rle bv\n"
              << "            2 for small hybrid-rle bv\n";
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
    } else if (type == 1){
        test<bv::simple_bv<16, 16384, 64, true, true, true>>(size, steps, seed);
    } else {
        test<bv::simple_bv<4, 2048, 64, true, true, true>>(size, steps, seed);
    }

    return 0;
}