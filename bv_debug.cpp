#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>

#include "bit_vector/bv.hpp"
//#include "deps/valgrind/callgrind.h"
#include "deps/DYNAMIC/include/dynamic/dynamic.hpp"
#include "sdsl/bit_vectors.hpp"

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

template <class bit_vector>
void check_sdsl(bit_vector* bv, uint64_t size) {
    sdsl::bit_vector sdsl_bv(size);
    bv->dump(sdsl_bv.data());
    sdsl::bit_vector::rank_1_type sdsl_rank(&sdsl_bv);
    sdsl::bit_vector::select_1_type sdsl_select(&sdsl_bv);
    assert(sdsl_bv.size() == bv->size());

    for (uint64_t i = 0; i < size; i++) {
        uint64_t ex = bv->at(i);
        uint64_t ac = sdsl_bv[i];
        if (ex != ac) {
            std::cerr << "Problem at access(" << i << "): " << ex << " != " << ac << std::endl;
            /*bv->print(false);
            for (size_t i = 0; i < sdsl_bv.size(); i++) {
                if (i % 8 == 0) std::cout << " ";
                if (i % 64 == 0) std::cout << std::endl;
                std::cout << (sdsl_bv[i] ? "1" : "0");
            }
            std::cout << std::endl;*/
            std::cout << "data[15] = " << sdsl_bv.data()[15] << std::endl;
            assert(ex == ac);
        }
        assert(sdsl_rank(i) == bv->rank(i));
    }

    uint64_t ones = bv->rank(size);
    for (uint64_t i = 1; i <= ones; i++) {
        assert(sdsl_select(i) == bv->select(i));
    }
}

template <class bit_vector>
void run_sdsl_test(uint64_t* input, uint64_t len) {
    bit_vector bv;
    uint64_t size = input[0];
    for (uint64_t i = 0; i < size; i++) {
        bv.insert(0, i % 2);
    }

    check_sdsl(&bv, size);

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
        check_sdsl(&bv, size);
    }
}

int main() {
    uint64_t a[] = {
        5216, 1, 2447, 0, 1110, 0, 2, 579, 0, 2, 3487, 0, 0, 130, 0, 2, 3531, 
        1, 2, 4570, 0, 1, 397, 1, 3680, 1, 1738, 2, 2418, 0, 0, 4711, 0, 1, 
        2344, 2, 3801, 1, 2, 1335, 1, 2, 1946, 1, 0, 1124, 0, 2, 3259, 1, 2, 
        4391, 0, 1, 3194, 0, 1329, 0, 2, 3592, 0, 1, 4492, 2, 2694, 0, 0, 
        3454, 1, 1, 392, 0, 2466, 1, 1, 356, 1, 1631, 2, 1375, 0, 1, 161, 2, 
        5201, 1, 2, 2525, 1, 2, 2977, 1, 1, 4409, 1, 1163, 2, 1810, 0, 2, 
        2429, 0, 1, 2347, 0, 1555, 0, 2, 4344, 1, 1, 3129, 1, 2863, 0, 72, 0, 
        1, 557, 1, 2308, 0, 4407, 1, 1, 2581, 2, 1314, 0, 0, 5069, 1, 2, 3754, 
        0, 2, 2580, 1, 0, 4535, 1, 0, 3777, 1, 1, 4069, 1, 4509, 0, 1614, 1, 
        0, 5139, 0, 2, 2872, 0, 0, 3201, 0, 2, 3827, 0, 1, 1647, 2, 2394, 1, 
        0, 3371, 0, 0, 111, 1, 1, 4438, 0, 4862, 0, 2, 3131, 1, 1, 127, 2, 
        2480, 1, 0, 515, 0, 1, 211, 0, 3083, 1, 1, 3100, 1, 4473, 1, 4593, 0, 
        3961, 0, 1, 1319, 0, 4386, 1, 1, 8, 2, 715, 0, 2, 2062, 0, 2, 1678, 0, 
        0, 1052, 1, 0, 4265, 0, 0, 31, 0, 2, 3963, 0, 2, 1106, 0, 1, 3563, 2, 
        1006, 1, 2, 3817, 1, 1, 30, 1, 4290, 2, 4978, 0, 0, 2984, 1, 2, 4219, 
        0, 2, 4478, 0, 2, 4786, 0, 1, 3850, 1, 1170
    };
    run_sdsl_test<bv::bv>(a, 266);

    /*uint64_t a[] = {
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
    run_test<bv::bv, dyn::suc_bv>(a, 148, true);*/
    /*uint64_t a[] = {16387, 2,     7184, 1,    1,    9890,  1,    10662,
                    1,     11795, 1,    7332, 0,    14649, 1,    2,
                    14480, 1,     2,    6282, 0,    2,     4190, 1,
                    2,     16170, 0,    1,    12969};
    run_sup_test<bv::bv>(a, 29);*/
}