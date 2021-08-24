#ifndef TEST_BRANCH_SELECTION_HPP
#define TEST_BRANCH_SELECTION_HPP

#include <cstdint>

#include "../deps/googletest/googletest/include/gtest/gtest.h"

template<class branch, uint8_t n_branches>
void branching_set_access_test() {
    branch b = branch();
    for (uint32_t i = 1; i <= n_branches; i++) {
        b.set(i - 1, i);
    }
    for (uint32_t i = 0; i < n_branches; i++) {
        ASSERT_EQ(b.get(i), i + 1);
    }
}

template<class branch, uint8_t n_branches>
void branching_increment_test() {
    branch b = branch();
    for (uint32_t i = 1; i <= n_branches; i++) {
        b.set(i - 1, i);
    }
    b.increment(n_branches / 2, n_branches, 1);
    for (uint32_t i = 0; i < n_branches / 2; i++) {
        ASSERT_EQ(b.get(i), i + 1);
    }
    for (uint32_t i = n_branches / 2; i < n_branches; i++) {
        ASSERT_EQ(b.get(i), i + 2);
    }
}

template<class branch, uint8_t n_branches>
void branching_delete_first_n_test() {
    branch b = branch();
    for (uint32_t i = 1; i <= n_branches; i++) {
        b.set(i - 1, i);
    }
    b.clear_first(n_branches / 2, n_branches);
    for (uint8_t i = 0; i < n_branches / 2; i ++) {
        ASSERT_EQ(b.get(i), uint64_t(i + 1));
    }
}

template<class branch, uint8_t n_branches>
void branching_append_n_test() {
    branch a = branch();
    branch b = branch();
    for (uint32_t i = 1; i <= n_branches / 2; i++) {
        a.set(i - 1, i);
        b.set(i - 1, i);
    }
    a.append(n_branches / 2, n_branches / 2, &b);
    for (uint8_t i = 0; i < n_branches; i++) {
        ASSERT_EQ(a.get(i), uint64_t(i + 1));
    }
}

template<class branch, uint8_t n_branches>
void branching_delete_last_n_test() {
    branch b = branch();
    for (uint32_t i = 1; i <= n_branches; i++) {
        b.set(i - 1, i);
    }
    b.clear_last(n_branches / 2, n_branches);
    for (uint8_t i = 0; i < n_branches / 2; i ++) {
        ASSERT_EQ(b.get(i), uint64_t(i + 1));
    }
}

template<class branch, uint8_t n_branches>
void branching_prepend_n_test() {
    branch a = branch();
    branch b = branch();
    for (uint32_t i = 1; i <= n_branches / 2; i++) {
        a.set(i - 1, i);
        b.set(i - 1, i);
    }
    a.prepend(n_branches / 2, n_branches / 2, n_branches / 2, &b);
    for (uint8_t i = 0; i < n_branches; i++) {
        ASSERT_EQ(a.get(i), uint64_t(i + 1)) << "i = " << int(i);
    }
}

template<class branch, uint8_t n_branches>
void branching_transfer_prepend_n_test() {
    branch a = branch();
    branch b = branch();
    for (uint32_t i = 1; i <= n_branches / 2; i++) {
        a.set(i - 1, i);
        b.set(i - 1, i * 2);
    }
    for (uint32_t i = 1; i <= n_branches / 2; i++) {
        ASSERT_EQ(a.get(i - 1), i);
        ASSERT_EQ(b.get(i - 1) , i * 2) << "i = " << i;
    }
    a.prepend(3, n_branches / 2, n_branches / 2, &b);
    b.clear_last(3, n_branches);
    for (uint32_t i = 1; i < 3 + n_branches / 2; i++) {
        ASSERT_EQ(a.get(i - 1), min(i, 3u) * 2 + (i > 3 ? i - 3 : 0)) << "i = " << i;
    }
    for (uint32_t i = 1; i <= n_branches / 2 - 3; i++ ) {
        ASSERT_EQ(b.get(i - 1), i * 2) << "i = " << i;
    }
}

template<class branch, uint8_t n_branches>
void branching_insert_test() {
    branch b = branch();
    for (uint32_t i = 1; i <= n_branches / 2; i++) {
        b.set(i - 1, i * 100);
    }
    b.insert(2, n_branches / 2, 50, 50);
    ASSERT_EQ(b.get(0), 100u);
    ASSERT_EQ(b.get(1), 150u);
    for (uint8_t i = 2; i < 1 + n_branches / 2; i++) {
        ASSERT_EQ(b.get(i), i * 100u);
    }
}

template<class branch, uint8_t n_branches>
void branch_remove_test() {
    branch b = branch();
    for (uint32_t i = 1; i <= n_branches / 2; i++) {
        b.set(i - 1, i);
    }
    for (uint32_t i = 0; i < n_branches / 2; i++) {
        ASSERT_EQ(b.get(i), uint32_t(i + 1));
    }
    b.remove(2, n_branches / 2);
    for (uint32_t i = 0; i < 2; i++) {
        ASSERT_EQ(b.get(i), uint32_t(i + 1));
    }
    for (uint32_t i = 2; i < n_branches / 2 - 1; i++) {
        ASSERT_EQ(b.get(i), uint32_t(i + 2));
    }
}

template<class branch, uint8_t n_branches>
void branch_append_elem_test() {
    branch b = branch();
    for (uint32_t i = 1; i <= n_branches / 2; i++) {
        b.append(i - 1, 1u);
    }
    /*for (uint8_t i = 0; i < n_branches; i++) {
        std::cout << b.get(i) << " ";
    }
    std::cout << std::endl;*/
    for (uint32_t i = 0; i < n_branches / 2; i++) {
        ASSERT_EQ(b.get(i), uint32_t(i + 1));
    }
}

#endif