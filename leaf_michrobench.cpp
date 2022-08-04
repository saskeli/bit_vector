
#include <chrono>
#include <iostream>
#include <random>

#include "bit_vector/bv.hpp"

struct result {
    double insert;
    double remove;
    double access;
    double rank;
    double select;
    uint64_t checksum;

    result(double ins, double rem, double acc, double ran, double sel,
           uint64_t che)
        : insert(ins),
          remove(rem),
          access(acc),
          rank(ran),
          select(sel),
          checksum(che) {}

    result()
        : insert(0), remove(0), access(0), rank(0), select(0), checksum(0) {}

    result operator+(result rhs) const {
        return {insert + rhs.insert, remove + rhs.remove,
                access + rhs.access, rank + rhs.rank,
                select + rhs.select, checksum + rhs.checksum};
    }

    result& operator+=(result rhs) {
        insert += rhs.insert;
        remove += rhs.remove;
        access += rhs.access;
        rank += rhs.rank;
        select += rhs.select;
        checksum += rhs.checksum;
        return *this;
    }
};

template <class leaf, class alloc, class random, class source>
result test(uint32_t* arr, uint32_t n, alloc* allocator, random gen, source src) {
    using std::chrono::duration_cast;
    using std::chrono::high_resolution_clock;
    using std::chrono::nanoseconds;

    auto* l = allocator->template allocate_leaf<leaf>(600);
    result res;

    for (uint32_t i = 0; i < n; i++) {
        arr[i * 2] = gen(src) % (i + 1);
        arr[i * 2 + 1] = gen(src) % 2;
    }
    auto t1 = high_resolution_clock::now();
    for (uint32_t i = 0; i < n; i++) {
        l->insert(arr[i * 2], arr[i * 2 + 1]);
    }
    auto t2 = high_resolution_clock::now();
    res.insert = (double)duration_cast<nanoseconds>(t2 - t1).count();

    for (uint32_t i = 0; i < n; i++) {
        arr[i] = gen(src) % n;
    }
    t1 = high_resolution_clock::now();
    for (uint32_t i = 0; i < n; i++) {
        res.checksum += l->at(arr[i]);
    }
    t2 = high_resolution_clock::now();
    res.access = (double)duration_cast<nanoseconds>(t2 - t1).count();

    for (uint32_t i = 0; i < n; i++) {
        arr[i] = gen(src) % n;
    }
    t1 = high_resolution_clock::now();
    for (uint32_t i = 0; i < n; i++) {
        res.checksum += l->rank(arr[i]);
    }
    t2 = high_resolution_clock::now();
    res.rank = (double)duration_cast<nanoseconds>(t2 - t1).count();

    uint32_t max = l->p_sum();
    max += max ? 0 : 1;
    for (uint32_t i = 0; i < n; i++) {
        arr[i] = (gen(src) % max) + 1;
    }
    t1 = high_resolution_clock::now();
    for (uint32_t i = 0; i < n; i++) {
        res.checksum += l->select(arr[i]);
    }
    t2 = high_resolution_clock::now();
    res.select = (double)duration_cast<nanoseconds>(t2 - t1).count();

    for (uint32_t i = 0; i < n; i++) {
        arr[i] = gen(src) % (n - i);
    }
    t1 = high_resolution_clock::now();
    for (uint32_t i = 0; i < n; i++) {
        res.checksum += l->remove(arr[i]);
    }
    t2 = high_resolution_clock::now();
    res.remove = (double)duration_cast<nanoseconds>(t2 - t1).count();

    std::cout << res.insert / n << "\t"
              << res.remove / n << "\t"
              << res.access / n << "\t"
              << res.rank / n << "\t"
              << res.select / n << std::endl;
    return res;
    allocator->deallocate_leaf(l);
}

void help() {
    std::cout << "Benchmark some leaves.\n\n";
    std::cout << "Usage: bench <type> <seed> <size> <steps>\n";
    std::cout << "   <type>   0 for unbuffered leaf\n"
              << "            1 for 6-bit gap leaf\n"
              << "            2 for 7-bit gap leaf\n"
              << "            3 for 8-bit gap leaf\n"
              << "            4 for buffered (8) leaf\n"
              << "            5 for buffered (16) leaf\n"
              << "            6 for 6-bit 64-block leaf\n"
              << "            7 for 6-bit 128-block leaf\n";
    std::cout << "   <seed>   seed to use for running the test\n";
    std::cout << "   <size>   number of bits in the bitvector. (<= 10000, "
                 "10000 default)\n";
    std::cout << "   <steps>  How many times should the tests be repeated. "
                 "(100 by default) \n\n";
    std::cout << "Example: bench 1 1337" << std::endl;
    exit(0);
}

int main(int argc, char const* argv[]) {
    auto* allocator = new bv::malloc_alloc();

    if (argc < 3) {
        help();
    }

    uint32_t type;
    uint32_t seed;
    uint32_t size = 10000;
    uint32_t steps = 100;

    std::sscanf(argv[1], "%u", &type);
    std::sscanf(argv[2], "%u", &seed);

    if (argc > 3) {
        std::sscanf(argv[3], "%u", &size);
        if (size > 10000) {
            std::cerr << "Invalid size argument" << std::endl;
            help();
        }
    }

    if (argc > 4) {
        std::sscanf(argv[4], "%u", &steps);
    }

    result res;
    uint32_t* arr = (uint32_t*)malloc(size * 2 * sizeof(uint32_t));
    std::mt19937 mt(seed);
    std::uniform_int_distribution<unsigned long long> gen(
        std::numeric_limits<std::uint64_t>::min(),
        std::numeric_limits<std::uint64_t>::max());

    std::cerr << "Michrobench: " << type << ", " << seed << ", " << size << ", "
              << steps << std::endl;
    std::cout << "insert\tremove\taccess\trank\tselect\n";

    for (uint32_t i = 0; i < steps; i++) {
        if (type == 0) {
            res += test<bv::leaf<0, 16384>>(
                arr, size, allocator, gen, mt);
        } else if (type == 1) {
            res += test<bv::gap_leaf<16384, 32, 6>>(
                arr, size, allocator, gen, mt);
        } else if (type == 2) {
            res += test<bv::gap_leaf<16384, 32, 7>>(
                arr, size, allocator, gen, mt);
        } else if (type == 3) {
            res += test<bv::gap_leaf<16384, 32, 8>>(
                arr, size, allocator, gen, mt);
        } else if (type == 4) {
            res += test<bv::leaf<8, 16384>>(
                arr, size, allocator, gen, mt);
        } else if (type == 5) {
            res += test<bv::leaf<16, 16384>>(
                arr, size, allocator, gen, mt);
        } else if (type == 6) {
            res += test<bv::gap_leaf<16384, 64, 6>>(
                arr, size, allocator, gen, mt);
        } else if (type == 7) {
            res += test<bv::gap_leaf<16384, 128, 6>>(
                arr, size, allocator, gen, mt);
        }
    }

    std::cerr << "Overall means:\n";
    std::cerr << "insert\tremove\taccess\trank\tselect\n";
    std::cerr << res.insert / (size * steps) << "\t"
              << res.remove / (size * steps) << "\t"
              << res.access / (size * steps) << "\t"
              << res.rank / (size * steps) << "\t"
              << res.select / (size * steps) << std::endl;
    delete allocator;
    free(arr);
}