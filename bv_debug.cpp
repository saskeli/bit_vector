#include <iostream>
#include <iomanip>
#include <cassert>
#include <random>
#include <chrono>

//#include "deps/DYNAMIC/include/dynamic/dynamic.hpp"

#include "bit_vector/allocator.hpp"
#include "bit_vector/simple_leaf.hpp"
#include "bit_vector/simple_node.hpp"
#include "bit_vector/bit_vector.hpp"

typedef uint32_t data_type;
typedef simple_leaf<8> leaf;
typedef simple_node<leaf, 8192> node;
typedef malloc_alloc alloc;
typedef bit_vector<leaf, node, malloc_alloc, 8192> simple_bv;

int main() {
    uint64_t size = 1000000;
    uint64_t steps = 100;

    simple_bv ubv;

    std::random_device rd;
    std::mt19937 gen(rd());

    using std::chrono::duration_cast;
    using std::chrono::high_resolution_clock;
    using std::chrono::microseconds;

    uint64_t N = 100000;

    for (uint64_t i = 1; i <= size; i++) {
        uint64_t pos = gen() % i;
        uint64_t val = gen() % 2;
        ubv.insert(pos, val);
    }

    double step = 1.0 / (steps - 1);
    std::vector<uint64_t> ops, loc, val;
    uint64_t checksum = 0;

    std::cout << "P\tcontrol\tchecksum" << std::endl;

    for (uint64_t mul = 0; mul < steps; mul++) {
        double p = step * mul;
        std::cout << p << "\t";
        std::bernoulli_distribution bool_dist(p);
        ops.clear();
        checksum = 0;

        for (uint64_t opn = 0; opn < N; opn++) {
            ops.push_back(bool_dist(gen));
            loc.push_back(gen() % size);
            val.push_back(gen() % 2);
        }

        auto t1 = high_resolution_clock::now();
        for (uint64_t opn = 0; opn < N; opn++) {
            if (ops[opn]) {
                ubv.insert(loc[opn], val[opn]);
            } else {
                checksum += ubv.rank(loc[opn]);
            }
        }
        auto t2 = high_resolution_clock::now();
        std::cout << (double)duration_cast<microseconds>(t2 - t1).count() / N
                  << "\t";

        while (ubv.size() > size) {
            uint64_t pos = gen() % ubv.size();
            ubv.remove(pos);
        }

        std::cout << checksum << std::endl;
    }
}