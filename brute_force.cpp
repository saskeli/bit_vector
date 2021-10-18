#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>

#include "bit_vector/bv.hpp"
#include "dynamic/dynamic.hpp"
#include "sdsl/bit_vectors.hpp"
#include "test/run_tests.hpp"

template <class bit_vector>
void brute_comp(uint64_t ops, uint64_t ds_size) {
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

        check(&bv, &d_bv, size);
        uint64_t loc = 0;
        uint64_t val = 0;

        for (uint64_t i = 0; i < ops; i++) {
            uint64_t op = gen(mt) % 3;
            std::cout << op << ", " << std::flush;
            switch (op) {
                case 0:
                    loc = gen(mt) % (size + 1);
                    val = gen(mt) % 2;
                    std::cout << loc << ", " << val << ", " << std::flush;
                    insert(&bv, &d_bv, loc, val);
                    check(&bv, &d_bv, ++size);
                    break;
                case 1:
                    loc = gen(mt) % size;
                    std::cout << loc << ", " << std::flush;
                    remove(&bv, &d_bv, loc);
                    check(&bv, &d_bv, --size);
                    break;
                default:
                    loc = gen(mt) % size;
                    val = gen(mt) % 2;
                    std::cout << loc << ", " << val << ", " << std::flush;
                    bv_set(&bv, &d_bv, loc, val);
                    check(&bv, &d_bv, size);
            }
        }
        std::cout << std::endl;
    }
}

template <class bit_vector, class control>
void brute_comp(uint64_t ops, uint64_t ds_size, uint32_t prob) {
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<unsigned long long> gen(
        std::numeric_limits<std::uint64_t>::min(),
        std::numeric_limits<std::uint64_t>::max());

    uint64_t counter = 1;
    while (true) {
        bit_vector bv;
        control d_bv;
        uint64_t size = gen(mt) % ds_size;
        if (size < ops) size = ops;

        std::cout << counter++ << ": " << size << ", " << std::flush;

        for (uint64_t i = 0; i < size; i++) {
            bv.insert(0, 0);
            d_bv.insert(0, 0);
        }

        check(&bv, &d_bv, size, 1);
        uint64_t loc = 0;
        uint64_t val = 0;

        for (uint64_t i = 0; i < ops; i++) {
            uint64_t op = gen(mt) % 3;
            std::cout << op << ", " << std::flush;
            switch (op) {
                case 0:
                    loc = gen(mt) % (size + 1);
                    val = (gen(mt) % prob) == 0;
                    std::cout << loc << ", " << val << ", " << std::flush;
                    insert(&bv, &d_bv, loc, val);
                    check(&bv, &d_bv, ++size, 1);
                    break;
                case 1:
                    loc = gen(mt) % size;
                    std::cout << loc << ", " << std::flush;
                    remove(&bv, &d_bv, loc);
                    check(&bv, &d_bv, --size, 1);
                    break;
                default:
                    loc = gen(mt) % size;
                    val = (gen(mt) % prob) == 0;
                    std::cout << loc << ", " << val << ", " << std::flush;
                    bv_set(&bv, &d_bv, loc, val);
                    check(&bv, &d_bv, size, 1);
            }
        }
        std::cout << std::endl;
    }
}

template <class bit_vector>
void brute_sup(uint64_t ops, uint64_t ds_size) {
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

        check_sup(&bv, size);
        uint64_t loc = 0;
        uint64_t val = 0;

        for (uint64_t i = 0; i < ops; i++) {
            uint64_t op = gen(mt) % 3;
            std::cout << op << ", " << std::flush;
            switch (op) {
                case 0:
                    loc = gen(mt) % (size + 1);
                    val = gen(mt) % 2;
                    std::cout << loc << ", " << val << ", " << std::flush;
                    insert_sup(&bv, loc, val);
                    check_sup(&bv, ++size);
                    break;
                case 1:
                    loc = gen(mt) % size;
                    std::cout << loc << ", " << std::flush;
                    remove_sup(&bv, loc);
                    check_sup(&bv, --size);
                    break;
                default:
                    loc = gen(mt) % size;
                    val = gen(mt) % 2;
                    std::cout << loc << ", " << val << ", " << std::flush;
                    set_sup(&bv, loc, val);
                    check_sup(&bv, size);
            }
        }
        std::cout << std::endl;
    }
}

template <class bit_vector>
void brute_sdsl(uint64_t ops, uint64_t ds_size) {
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

        check_sdsl(&bv, size);
        uint64_t loc = 0;
        uint64_t val = 0;

        for (uint64_t i = 0; i < ops; i++) {
            uint64_t op = gen(mt) % 3;
            std::cout << op << ", " << std::flush;
            switch (op) {
                case 0:
                    loc = gen(mt) % (size + 1);
                    val = gen(mt) % 2;
                    std::cout << loc << ", " << val << ", " << std::flush;
                    insert_sup(&bv, loc, val);
                    check_sdsl(&bv, ++size);
                    break;
                case 1:
                    loc = gen(mt) % size;
                    std::cout << loc << ", " << std::flush;
                    remove_sup(&bv, loc);
                    check_sdsl(&bv, --size);
                    break;
                default:
                    loc = gen(mt) % size;
                    val = gen(mt) % 2;
                    std::cout << loc << ", " << val << ", " << std::flush;
                    set_sup(&bv, loc, val);
                    check_sdsl(&bv, size);
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
    std::cout
        << "   <type>   0 buffered vs DYNAMIC.\n"
        << "            1 query support structure testing.\n"
        << "            2 aggressive reallocation vs DYNAMIC.\n"
        << "            3 query support with aggressive reallocation.\n"
        << "            4 buffered vs SDSL dump.\n"
        << "            5 hybrid_rle vs buffered\n"
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

    switch (type) {
        case 0:
            brute_comp<bv::bv>(ops, size);
            break;
        case 1:
            brute_sup<bv::bv>(ops, size);
            break;
        case 2:
            brute_comp<bv::simple_bv<16, 16384, 64, true, true>>(ops, size);
            break;
        case 3:
            brute_sup<bv::simple_bv<16, 16384, 64, true, true>>(ops, size);
            break;
        case 4:
            brute_sdsl<bv::bv>(ops, size);
            break;
        default:
            brute_comp<bv::simple_bv<16, 16384, 64, true, true, true>,
                       bv::simple_bv<16, 16384, 64, true, true>>(ops, size, 50);
    }
}