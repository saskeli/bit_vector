#include <sys/resource.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>

#include "bit_vector/bv.hpp"

#define SIZE 16384

template <class bit_vector, uint64_t select_offset>
void test(uint64_t size, uint64_t steps, uint64_t seed) {
    bit_vector bv;

    std::mt19937 mt(seed);
    std::uniform_int_distribution<unsigned long long> gen(
        std::numeric_limits<std::uint64_t>::min(),
        std::numeric_limits<std::uint64_t>::max());

    using std::chrono::duration_cast;
    using std::chrono::high_resolution_clock;
    using std::chrono::microseconds;

    uint64_t start_size = 100000;

    double startexp = log2(double(start_size));
    double delta = (log2(double(size)) - log2(double(start_size))) / steps;
    uint64_t ops = 100000;
    std::cerr << "startexp: " << startexp << ". delta: " << delta << std::endl;
    std::vector<uint64_t> loc, val;

    std::cout << "type\tsize\tflush\taccess\trank\tselect" << std::endl;

    for (uint64_t i = 0; i < start_size - ops; i++) {
        uint64_t aloc = gen(mt) % (i + 1);
        bool aval = gen(mt) % 2;
        bv.insert(aloc, aval);
    }
    for (uint64_t step = 1; step <= steps; step++) {
        uint64_t checksum = 0;
        uint64_t start = bv.size();
        uint64_t target = uint64_t(pow(2.0, startexp + delta * step));
        double flush[] = {0.0, 0.0, 0.0, 0.0};
        double sup[] = {0.0, 0.0, 0.0, 0.0};

        for (size_t i = start; i < target; i++) {
            uint64_t aloc = gen(mt) % (i + 1);
            bool aval = gen(mt) % 2;
            bv.insert(aloc, aval);
        }

        loc.clear();
        for (uint64_t i = target; i > target - ops; i--) {
            loc.push_back(gen(mt) % i);
        }

        for (size_t i = 0; i < ops; i++) {
            bv.remove(loc[i]);
        }

        loc.clear();
        val.clear();
        for (uint64_t i = bv.size(); i < target; i++) {
            loc.push_back(gen(mt) % (i + 1));
            val.push_back(gen(mt) % 2);
        }

        for (size_t i = 0; i < loc.size(); i++) {
            bv.insert(loc[i], val[i]);
        }

        auto t1 = high_resolution_clock::now();
        bv.flush();
        auto t2 = high_resolution_clock::now();
        flush[0] = (double)duration_cast<microseconds>(t2 - t1).count();

        t1 = high_resolution_clock::now();
        auto* ss = bv.generate_query_structure();
        t2 = high_resolution_clock::now();
        sup[0] = (double)duration_cast<microseconds>(t2 - t1).count();

        loc.clear();
        for (size_t i = 0; i < ops; i++) {
            loc.push_back(gen(mt) % target);
        }

        t1 = high_resolution_clock::now();
        for (size_t i = 0; i < ops; i++) {
            checksum += bv.at(loc[i]);
        }
        t2 = high_resolution_clock::now();
        flush[1] = (double)duration_cast<microseconds>(t2 - t1).count() / ops;

        t1 = high_resolution_clock::now();
        for (size_t i = 0; i < ops; i++) {
            checksum -= ss->at(loc[i]);
        }
        t2 = high_resolution_clock::now();
        sup[1] = (double)duration_cast<microseconds>(t2 - t1).count() / ops;

        loc.clear();
        for (size_t i = 0; i < ops; i++) {
            loc.push_back(gen(mt) % target);
        }

        t1 = high_resolution_clock::now();
        for (size_t i = 0; i < ops; i++) {
            checksum += bv.rank(loc[i]);
        }
        t2 = high_resolution_clock::now();
        flush[2] = (double)duration_cast<microseconds>(t2 - t1).count() / ops;

        t1 = high_resolution_clock::now();
        for (size_t i = 0; i < ops; i++) {
            checksum -= ss->rank(loc[i]);
        }
        t2 = high_resolution_clock::now();
        sup[2] = (double)duration_cast<microseconds>(t2 - t1).count() / ops;

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
        flush[3] = (double)duration_cast<microseconds>(t2 - t1).count() / ops;

        t1 = high_resolution_clock::now();
        for (size_t i = 0; i < ops; i++) {
            checksum -= ss->select(loc[i] + select_offset);
        }
        t2 = high_resolution_clock::now();
        sup[3] = (double)duration_cast<microseconds>(t2 - t1).count() / ops;

        if (checksum != 0) {
            std::cerr << "Invalid checksum " << checksum << std::endl;
            ss->print(true);
            bv.print(true);
            exit(1);
        }

        std::cout << "flush\t" << bv.size();
        for (size_t i = 0; i < 4; i++) {
            std::cout << "\t" << flush[i];
        }
        std::cout << "\nsupport\t" << ss->size();
        for (size_t i = 0; i < 4; i++) {
            std::cout << "\t" << sup[i];
        }
        std::cout << std::endl;

        delete (ss);
    }
}

typedef bv::malloc_alloc alloc;
typedef bv::leaf<8> leaf;
typedef bv::node<leaf, uint64_t, 16384, 64> node;
typedef bv::query_support<uint64_t, leaf, 2048> qs;

int main(int argc, char const* argv[]) {
    uint64_t size = 16384;
    alloc* a = new alloc();
    node* n = a->template allocate_node<node>();
    n->has_leaves(true);
    leaf* l = a->template allocate_leaf<leaf>(1 + size / 64);
    for (size_t i = 0; i < size / 2; i++) {
        l->insert(i, i % 2);
    }
    n->append_child(l);
    l = a->template allocate_leaf<leaf>(1 + size / 64);
    for (size_t i = 0; i < size / 2; i++) {
        l->insert(i, (i + size / 2) % 2);
    }
    n->append_child(l);

    qs* q = new qs();
    n->generate_query_structure(q);
    q->finalize();
    q->print(true);
    assert(n->select(4096) == q->select(4096));
    n->deallocate(a);
    a->deallocate_node(n);
    delete(a);
    delete(q);
    /*if (argc < 2) {
        std::cerr << "Seed is required" << std::endl;
        return 1;
    }
    uint64_t seed;
    uint64_t size = 10000000;
    uint64_t steps = 100;

    std::sscanf(argv[1], "%lu", &seed);

    if (argc > 2) {
        std::sscanf(argv[3], "%lu", &size);
        if (size < 10000000) {
            std::cerr << "Invalid size argument" << std::endl;
            return 1;
        }
    }

    if (argc > 3) {
        std::sscanf(argv[4], "%lu", &steps);
    }

    test<bv::simple_bv<16, 16384, 64>, 1>(size, steps, seed);
    return 0;*/
}
