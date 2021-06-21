#include <sys/resource.h>

#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>

#include "bit_vector/bv.hpp"
#include "deps/DYNAMIC/include/dynamic/dynamic.hpp"

template <class bv_type>
void run(uint64_t seed, uint64_t n, uint64_t iters) {
    std::mt19937 mt(seed);
    std::uniform_int_distribution<unsigned long long> gen(
        std::numeric_limits<std::uint64_t>::min(),
        std::numeric_limits<std::uint64_t>::max());

    struct rusage ru;

    bv_type bv;

    for (uint64_t i = 0; i < n; i++) {
        uint64_t idx = gen(mt) % (i + 1);
        uint64_t val = gen(mt) % 2;
        bv.insert(idx, val);
    }

    std::cout << "epoc\tsmall_bits\tsmall_rss\tbig_bits\tbig_rss" << std::endl;

    for (uint64_t epoc = 0; epoc < iters; epoc++) {
        std::cout << epoc << "\t" << std::flush;
        for (uint64_t i = 0; i < n >> 1; i++) {
            uint64_t idx = gen(mt) % (n - i);
            bv.remove(idx);
        }
        std::cout << bv.bit_size() << "\t" << std::flush;
        getrusage(RUSAGE_SELF, &ru);
        std::cout << ru.ru_maxrss * 8 * 1024 << "\t" << std::flush;

        for (uint64_t i = 0; i < n >> 1; i++) {
            uint64_t idx = gen(mt) % (n - (n >> 1) + i);
            uint64_t val = gen(mt) % 2;
            bv.insert(idx, val);
        }

        std::cout << bv.bit_size() << "\t" << std::flush;
        getrusage(RUSAGE_SELF, &ru);
        std::cout << ru.ru_maxrss * 8 * 1024 << std::endl;
    }
}

int main(int argc, char const *argv[]) {
    if (argc <= 2) {
        std::cerr << "Need seed and type" << std::endl;
        return 1;
    }
    uint64_t seed;
    std::sscanf(argv[1], "%lu", &seed);
    uint64_t type;
    std::sscanf(argv[2], "%lu", &type);

    uint64_t n = 1000000;
    if (argc >= 4) {
        std::sscanf(argv[3], "%lu", &n);
    }
    uint64_t iters = 200;
    if (argc >= 5) {
        std::sscanf(argv[5], "%lu", &iters);
    }

    switch (type) {
        case 1:
            std::cerr << "Testing suc_bv seed = " << seed << ", n = " << n
                      << ", iterations = " << iters << std::endl;
            run<dyn::suc_bv>(seed, n, iters);
            break;
        default:
            std::cerr << "Testing bv seed = " << seed << ", n = " << n
                      << ", iterations = " << iters << std::endl;
            run<bv::bv>(seed, n, iters);
            break;
    }
    return 0;
}
