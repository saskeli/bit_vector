#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>

#include "bit_vector/bv.hpp"
#include "dynamic/dynamic.hpp"

template <class bva, class bvb>
void check_comp(bva* a, bvb* b, uint64_t size) {
    assert(a->size() == size);
    assert(b->size() == size);

    for (uint64_t i = 0; i < size; i++) {
        assert(a->at(i) == b->at(i));
        assert(a->rank(i) == b->rank(i));
    }

    uint64_t ones = a->rank(size);
    assert(b->rank(size) == ones);

    for (uint64_t i = 0; i < ones; i++) {
        assert(a->select(i + 1) == b->select(i));
    }
}

template <class bva, class bvb>
void insert_comp(bva* a, bvb* b, uint64_t loc, bool val) {
    std::cout << loc << ", " << val << ", " << std::flush;
    a->insert(loc, val);
    b->insert(loc, val);
}

template <class bva, class bvb>
void remove_comp(bva* a, bvb* b, uint64_t loc) {
    std::cout << loc << ", " << std::flush;
    a->remove(loc);
    b->remove(loc);
}

template <class bva, class bvb>
void set_comp(bva* a, bvb* b, uint64_t loc, bool val) {
    std::cout << loc << ", " << val << ", " << std::flush;
    a->set(loc, val);
    b->set(loc, val);
}

template <class bit_vector>
void run_comp(uint64_t ops, uint64_t ds_size) {
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<unsigned long long> gen(
        std::numeric_limits<std::uint64_t>::min(),
        std::numeric_limits<std::uint64_t>::max());

    uint64_t counter = 1;
    while (true) {
        bit_vector bv;
        dyn::suc_bv d_bv;
        uint64_t size = gen(mt) % ds_size;
        if (size < ops) size = ops;

        std::cout << counter++ << ": " << size << ", " << std::flush;

        for (uint64_t i = 0; i < size; i++) {
            bv.insert(0, i % 2);
            d_bv.insert(0, i % 2);
        }

        check_comp(&bv, &d_bv, size);

        for (uint64_t i = 0; i < ops; i++) {
            uint64_t op = gen(mt) % 3;
            std::cout << op << ", " << std::flush;
            switch (op) {
                case 0:
                    insert_comp(&bv, &d_bv, gen(mt) % (size + 1), gen(mt) % 2);
                    check_comp(&bv, &d_bv, ++size);
                    break;
                case 1:
                    remove_comp(&bv, &d_bv, gen(mt) % size);
                    check_comp(&bv, &d_bv, --size);
                    break;
                default:
                    set_comp(&bv, &d_bv, gen(mt) % size, gen(mt) % 2);
                    check_comp(&bv, &d_bv, size);
            }
        }
        std::cout << std::endl;
    }
}

template <class bit_vector>
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
    delete (q);
}

template <class bit_vector>
void insert(bit_vector* bv, uint64_t loc, bool val) {
    std::cout << loc << ", " << val << ", " << std::flush;
    bv->insert(loc, val);
}

template <class bit_vector>
void remove(bit_vector* bv, uint64_t loc) {
    std::cout << loc << ", " << std::flush;
    bv->remove(loc);
}

template <class bit_vector>
void bv_set(bit_vector* bv, uint64_t loc, bool val) {
    std::cout << loc << ", " << val << ", " << std::flush;
    bv->set(loc, val);
}

template <class bit_vector>
void run_sup(uint64_t ops, uint64_t ds_size) {
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<unsigned long long> gen(
        std::numeric_limits<std::uint64_t>::min(),
        std::numeric_limits<std::uint64_t>::max());

    uint64_t counter = 1;
    while (true) {
        bit_vector bv;
        uint64_t size = gen(mt) % ds_size;
        if (size < ops) size = ops;

        std::cout << counter++ << ": " << size << ", " << std::flush;

        for (uint64_t i = 0; i < size; i++) {
            bv.insert(0, i % 2);
        }

        check(&bv, size);

        for (uint64_t i = 0; i < ops; i++) {
            uint64_t op = gen(mt) % 3;
            std::cout << op << ", " << std::flush;
            switch (op) {
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

void help() {
    std::cout << "Brute force testing for dynamic bit vectors.\n\n"
              << "Will run random query sequences on data structures untill "
                 "interrupeted or an error is detected\n\n"
              << "Type is required.\n\n";
    std::cout << "Usage: ./brute <type> <ops> <size>\n";
    std::cout << "   <type>   0 buffered / vs DYNAMIC.\n"
              << "            1 query support structure testing.\n"
              << "            2 aggressive reallocation vs DYNAMIC.\n"
              << "            3 query support with aggressive reallocation.\n"
              << "   <ops>    Number of random operations to run on each iteration.\n"
              << "            Defaults to 100.\n"
              << "   <size>   Maximum intial size of data structures.\n"
              << "            Needs to be greater than ops. Defaults to 1e5.\n\n";
    std::cout << "Example: ./bench 1" << std::endl;
    exit(0);
}

int main(int argc, char const* argv[]) {
    if (argc < 2) {
        help();
    }
    uint64_t type;
    std::sscanf(argv[1], "%lu", &type);
    uint64_t ops = 100;
    uint64_t size = 10000;
    if (argc > 2) {
        std::sscanf(argv[2], "%lu", &ops);
    }
    if (argc > 3) {
        std::sscanf(argv[3], "%lu", &size);
    }

    if (size <= ops) {
        std::cerr << "Invalid combination of size and ops." << std::endl;
        help();
    }

    switch (type)
    {
    case 0:
        run_comp<bv::bv>(ops, size);
        break;
    case 1:
        run_sup<bv::bv>(ops, size);
        break;
    case 2:
        run_comp<bv::simple_bv<16, 16384, 64, true, true>>(ops, size);
        break;
    default:
        run_sup<bv::simple_bv<16, 16384, 64, true, true>>(ops, size);
        break;
    }
    
}