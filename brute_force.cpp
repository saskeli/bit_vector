#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>

#include "bit_vector/bv.hpp"

typedef bv::bv bit_vector;

void check(bit_vector* bv, uint64_t size) {
    auto* q = bv->generate_query_structure();
    assert(q->size() == size);
    assert(bv->size() == size);

    for (uint64_t i = 0; i < size; i++) {
        assert(q->at(i) == bv->at(i));
        assert(q->rank(i) == bv->rank(i));
    }

    uint64_t ones = bv->rank(size);
    assert(q->rank(size) == ones);

    for (uint64_t i = 1; i <= ones; i++) {
        assert(q->select(i) == bv->select(i));
    }
    delete(q);
}

void insert(bit_vector* bv, uint64_t loc, bool val) {
    std::cout << loc << ", " << val << ", " << std::flush;
    bv->insert(loc, val);
}

void remove(bit_vector* bv, uint64_t loc) {
    std::cout << loc << ", " << std::flush;
    bv->remove(loc);
}

void bv_set(bit_vector* bv, uint64_t loc, bool val) {
    std::cout << loc << ", " << val << ", " << std::flush;
    bv->set(loc, val);
}

int main() {

    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<unsigned long long> gen(
        std::numeric_limits<std::uint64_t>::min(),
        std::numeric_limits<std::uint64_t>::max());

    uint64_t counter = 1;
    uint64_t ops = 100;
    while (true) {
        bit_vector bv;
        uint64_t size = ops + (gen(mt) % 100000);

        std::cout << counter++ << ": " << size << ", " << std::flush;

        for (uint64_t i = 0; i < size; i++) {
            bv.insert(0, i % 2);
        }

        check(&bv, size);

        for (uint64_t i = 0; i < ops; i++) {
            uint64_t op = gen(mt) % 3;
            std::cout << op << ", " << std::flush;
            switch (op)
            {
            case 0:
                insert(&bv, gen(mt) % (size + 1), gen(mt) % 2);
                check(&bv, ++size);
                break;
            case 1:
                remove(&bv, gen(mt) % size);
                check(&bv, --size);
                break;
            default:
                bv_set(&bv, gen(mt) % size, gen(mt) % 2);
                check(&bv, size);
            }
        }
        std::cout << std::endl;
    }
}