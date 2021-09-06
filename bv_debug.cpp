#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>

#include "bit_vector/bv.hpp"
//#include "deps/valgrind/callgrind.h"
#include "deps/DYNAMIC/include/dynamic/dynamic.hpp"
//#include "sdsl/bit_vectors.hpp"

typedef bv::malloc_alloc alloc;
typedef bv::leaf<16> leaf;
typedef bv::node<leaf, uint64_t, 16384, 16> node;
typedef bv::simple_bv<8, 16384, 64> bit_vector;
typedef bv::query_support<uint64_t, leaf, 2048> qs;

template <class bit_vector>
void check_sup(bit_vector* bv, uint64_t size) {
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
        uint64_t ex = bv->select(i);
        uint64_t ac = q->select(i);
        if (ex != ac) {
            std::cout << "select(" << i << "): ex = " << ex << ", ac = " << ac
                      << std::endl;
            assert(ac == ex);
        }
    }
    delete (q);
}

template <class bit_vector>
void insert_sup(bit_vector* bv, uint64_t loc, bool val) {
    bv->insert(loc, val);
}

template <class bit_vector>
void remove_sup(bit_vector* bv, uint64_t loc) {
    bv->remove(loc);
}

template <class bit_vector>
void set_sup(bit_vector* bv, uint64_t loc, bool val) {
    bv->set(loc, val);
}

template <class bit_vector>
void run_sup_test(uint64_t* input, uint64_t len) {
    bit_vector bv;
    uint64_t size = input[0];
    for (uint64_t i = 0; i < size; i++) {
        bv.insert(0, i % 2);
    }

    check_sup(&bv, size);

    uint64_t index = 1;
    while (index < len) {
        switch (input[index]) {
            case 0:
                insert_sup(&bv, input[index + 1], input[index + 2]);
                size++;
                index += 3;
                break;
            case 1:
                remove_sup(&bv, input[index + 1]);
                size--;
                index += 2;
                break;
            default:
                set_sup(&bv, input[index + 1], input[index + 2]);
                index += 3;
        }
        check_sup(&bv, size);
    }
}

template <class bva, class bvb>
void check(bva* a, bvb* b, uint64_t size) {
    assert(a->size() == size);
    assert(b->size() == size);

    for (uint64_t i = 0; i < size; i++) {
        if (a->at(i) != b->at(i)) {
            std::cout << "Problem at i = " << i << std::endl;
            std::cout << "at(" << i << ") = " << a->at(i) << ", " << b->at(i)
                      << std::endl;
            assert(a->at(i) == b->at(i));
        }
        assert(a->rank(i) == b->rank(i));
    }

    uint64_t ones = a->rank(size);
    assert(a->rank(size) == b->rank(size));

    for (uint64_t i = 0; i < ones; i++) {
        assert(a->select(i + 1) == b->select(i));
    }
}

template <class bva, class bvb>
void insert(bva* a, bvb* b, uint64_t loc, bool val) {
    val = val ? true : false;
    a->insert(loc, val);
    b->insert(loc, val);
}

template <class bva, class bvb>
void remove(bva* a, bvb* b, uint64_t loc) {
    a->remove(loc);
    b->remove(loc);
}

template <class bva, class bvb>
void bv_set(bva* a, bvb* b, uint64_t loc, bool val) {
    val = val ? true : false;
    a->set(loc, val);
    b->set(loc, val);
}

template <class bit_vector, class c_bv>
void run_test(uint64_t* input, uint64_t len, bool show_index = false) {
    bit_vector bv;
    c_bv cbv;

    uint64_t size = input[0];

    for (uint64_t i = 0; i < size; i++) {
        bv.insert(0, i % 2);
        cbv.insert(0, i % 2);
        check(&bv, &cbv, i + 1);
    }

    // bv.print(false);

    uint64_t index = 1;
    while (index < len) {
        if (show_index) std::cout << index << std::endl;
        //if (index == 11) bv.print(false);
        switch (input[index]) {
            case 0:
                if (show_index) std::cout << "insert(" << input[index + 1] << ", " << input[index + 2] << ")" << std::endl;
                insert(&bv, &cbv, input[index + 1], input[index + 2]);
                size++;
                index += 3;
                break;
            case 1:
                if (show_index) std::cout << "remove(" << input[index + 1] << ")" << std::endl;
                remove(&bv, &cbv, input[index + 1]);
                size--;
                index += 2;
                break;
            default:
                if (show_index) std::cout << "set(" << input[index + 1] << ", " << input[index + 2] << ")" << std::endl;
                bv_set(&bv, &cbv, input[index + 1], input[index + 2]);
                index += 3;
        }
        //if (index == 11) bv.print(false);
        check(&bv, &cbv, size);
    }
}

int main() {
    uint64_t a[] = {
        876, 1,   27,  2,   668, 1,   1,   594, 0,   26,  0,   2,   320, 1,
        2,   679, 1,   0,   82,  1,   0,   673, 0,   2,   201, 0,   1,   875,
        1,   157, 2,   219, 1,   0,   761, 1,   0,   531, 0,   1,   579, 1,
        738, 1,   636, 1,   571, 2,   152, 0,   0,   176, 1,   2,   453, 1,
        1,   93,  2,   795, 0,   1,   275, 2,   580, 1,   0,   406, 0,   2,
        290, 0,   1,   619, 0,   14,  0,   0,   817, 1,   0,   128, 1,   1,
        0,   2,   73,  0,   1,   333, 1,   799, 1,   352, 0,   678, 1,   0,
        667, 1,   1,   786, 0,   643, 0,   1,   295, 0,   340, 1,   0,   53,
        1,   2,   811, 1,   0,   416, 0,   2,   638, 0,   1,   36,  2,   700,
        1,   0,   278, 1,   2,   551, 0,   0,   101, 1,   1,   739, 2,   362,
        0,   0,   392, 0,   0,   608, 0,   0,   800, 1,   2,   4,   0,   1,
        798, 1,   173, 2,   417, 1,   0,   538, 1,   1,   876, 1,   111, 1,
        493, 0,   467, 1,   0,   765, 1,   1,   660, 0,   254, 1,   0,   603,
        0,   0,   499, 0,   1,   483, 1,   415, 1,   522, 2,   132, 1,   1,
        210, 0,   570, 0,   1,   613, 2,   741, 1,   0,   383, 0,   0,   0,
        1,   2,   205, 1,   1,   350, 1,   327, 1,   671, 2,   466, 0,   2,
        562, 0,   2,   728, 0,   0,   599, 1,   1,   514, 2,   762, 1,   2,
        555, 1,   1,   368, 1,   172, 2,   468, 1,   2,   762, 1,   2,   290,
        0,   2,   5,   1,   0,   242, 0,   1,   775, 0,   8,   0,   1,   799,
        0,   281, 1,   2,   242, 1,   0,   660, 1,   2,   401, 1,   0,   798,
        0,   2,   370, 1,   2,   20,  0,   2,   379, 1,   1,   436, 0,   154,
        1};
    run_test<bv::bv, dyn::suc_bv>(a, 148, true);
    /*uint64_t a[] = {16387, 2,     7184, 1,    1,    9890,  1,    10662,
                    1,     11795, 1,    7332, 0,    14649, 1,    2,
                    14480, 1,     2,    6282, 0,    2,     4190, 1,
                    2,     16170, 0,    1,    12969};
    run_sup_test<bv::bv>(a, 29);*/

    /*uint64_t seed = 2586862946;
    uint64_t size = 1000000000;
    uint64_t steps = 100;
    bit_vector bv;
    dyn::suc_bv cbv;

    std::mt19937 mt(seed);
    std::uniform_int_distribution<unsigned long long> gen(
        std::numeric_limits<std::uint64_t>::min(),
        std::numeric_limits<std::uint64_t>::max());

    uint64_t start_size = 1000000;

    double startexp = log2(double(start_size));
    double delta = (log2(double(size)) - log2(double(start_size))) / steps;
    uint64_t ops = 100000;
    std::cerr << "startexp: " << startexp << ". delta: " << delta;
    std::cerr << "\nWith seed: " << seed << std::endl;
    std::vector<uint64_t> loc, val;

    std::cout << "Filling to 900000..." << std::flush;
    for (uint64_t i = 0; i < 900000; i++) {
        uint64_t aloc = gen(mt) % (i + 1);
        bool aval = gen(mt) % 2;
        bv.insert(aloc, aval);
        cbv.insert(aloc, aval);
    }
    std::cout << "OK" << std::endl;
    for (uint64_t step = 1; step <= steps; step++) {
        uint64_t start = bv.size();
        uint64_t target = uint64_t(pow(2.0, startexp + delta * step));

        uint64_t checksum = 0;

        std::cout << start << " -> " << target << "..." << std::flush;

        for (size_t i = start; i < target; i++) {
            uint64_t aloc = gen(mt) % (i + 1);
            bool aval = gen(mt) % 2;
            bv.insert(aloc, aval);
            cbv.insert(aloc, aval);
            if (target == 7413102 && (i == 6954489 || i == 6954488)) {
                std::cout << " " << i << " ";
                bv.validate();
                check<bit_vector, dyn::suc_bv>(&bv, &cbv, bv.size());
            }
        }

        if (target == 7413102) {
            std::cout << " B ";
            check<bit_vector, dyn::suc_bv>(&bv, &cbv, bv.size());
        }

        loc.clear();
        for (uint64_t i = target; i > target - ops; i--) {
            loc.push_back(gen(mt) % i);
        }

        for (size_t i = 0; i < ops; i++) {
            bv.remove(loc[i]);
            cbv.remove(loc[i]);
        }

        loc.clear();
        val.clear();
        for (uint64_t i = bv.size(); i < target; i++) {
            loc.push_back(gen(mt) % (i + 1));
            val.push_back(gen(mt) % 2);
        }

        for (size_t i = 0; i < loc.size(); i++) {
            bv.insert(loc[i], val[i]);
            cbv.insert(loc[i], val[i]);
        }

        loc.clear();
        for (size_t i = 0; i < ops; i++) {
            loc.push_back(gen(mt) % target);
        }

        for (size_t i = 0; i < ops; i++) {
            checksum += bv.at(loc[i]);
            checksum -= cbv.at(loc[i]);
        }
        assert(checksum == 0);

        loc.clear();
        for (size_t i = 0; i < ops; i++) {
            loc.push_back(gen(mt) % target);
        }

        for (size_t i = 0; i < ops; i++) {
            checksum += bv.rank(loc[i]);
            checksum -= cbv.rank(loc[i]);
        }
        assert(checksum == 0);

        uint64_t limit = bv.rank(target - 1);
        assert(limit == cbv.rank(target - 1));
        loc.clear();
        for (size_t i = 0; i < ops; i++) {
            loc.push_back(gen(mt) % limit);
        }

        for (size_t i = 0; i < ops; i++) {
            checksum += bv.select(loc[i] + 1);
            checksum -= cbv.select(loc[i]);
        }
        assert(checksum == 0);
        std::cout << "OK" << std::endl;
    }

    uint64_t tot_size = bv.size();
    check<bit_vector, dyn::suc_bv>(&bv, &cbv, tot_size);
    std::cout << "PASSED!" << std::endl;*/
}