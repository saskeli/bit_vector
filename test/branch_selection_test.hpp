#ifndef TEST_BRANCH_SELECTION_HPP
#define TEST_BRANCH_SELECTION_HPP

#include <cstdint>

#include "../deps/googletest/googletest/include/gtest/gtest.h"

template<class branch, uint8_t n_branches>
void branching_set_access_test() {
    branch b = branch();
    for (uint32_t i = 1; i <= n_branches; i++) {
        b[i - 1] = i;
    }
    for (uint32_t i = 0; i < n_branches; i++) {
        ASSERT_EQ(b[i], i + 1);
    }
}

template<class branch, uint8_t n_branches>
void branching_increment_test() {
    branch b = branch();
    for (uint32_t i = 1; i <= n_branches; i++) {
        b[i - 1] = i;
    }
    for (uint32_t i = n_branches / 2; i < n_branches; i++) {
        b[i] += 1;
    }
    for (uint32_t i = 0; i < n_branches / 2; i++) {
        ASSERT_EQ(b[i], i + 1);
    }
    for (uint32_t i = n_branches / 2; i < n_branches; i++) {
        ASSERT_EQ(b[i], i + 2);
    }
}

template<class branch, uint8_t n_branches>
void branching_delete_first_n_test() {
    branch b = branch();
    for (uint32_t i = 1; i <= n_branches; i++) {
        b[i - 1] = i;
    }
    uint64_t offset = b[n_branches / 2 - 1];
    for (uint8_t i = 0; i < n_branches / 2; i++) {
        b[i] = b[i + n_branches / 2] - offset;
    }
    for (uint8_t i = n_branches / 2; i < n_branches; i++) {
        b[i] = (~uint64_t(0)) >> 1;
    }
    for (uint8_t i = 0; i < n_branches / 2; i ++) {
        ASSERT_EQ(b[i], uint64_t(i + 1));
    }
    for (uint8_t i = n_branches / 2; i < n_branches; i++) {
        ASSERT_EQ(b[i], (~uint64_t(0)) >> 1);
    }
}

template<class branch, uint8_t n_branches>
void branching_append_n_test() {
    branch a = branch();
    branch b = branch();
    for (uint32_t i = 1; i <= n_branches / 2; i++) {
        a[i - 1] = i;
        b[i - 1] = i;
    }
    for (uint8_t i = 0; i < n_branches / 2; i++) {
        a[i + n_branches / 2] = a[n_branches / 2 - 1] + b[i];
    }
    for (uint8_t i = 0; i < n_branches; i++) {
        ASSERT_EQ(a[i], uint64_t(i + 1));
    }
}

template<class branch, uint8_t n_branches>
void branching_delete_last_n_test() {
    branch b = branch();
    for (uint32_t i = 1; i <= n_branches; i++) {
        b[i - 1] = i;
    }
    for (uint8_t i = n_branches / 2; i < n_branches; i++) {
        b[i] = (~uint64_t(0)) >> 1;
    }
    for (uint8_t i = 0; i < n_branches / 2; i ++) {
        ASSERT_EQ(b[i], uint64_t(i + 1));
    }
    for (uint8_t i = n_branches / 2; i < n_branches; i++) {
        ASSERT_EQ(b[i], (~uint64_t(0)) >> 1);
    }
}

template<class branch, uint8_t n_branches>
void branching_prepend_n_test() {
    branch a = branch();
    branch b = branch();
    for (uint32_t i = 1; i <= n_branches / 2; i++) {
        a[i - 1] = i;
        b[i - 1] = i;
    }
    for (uint8_t i = 0; i < n_branches / 2; i++) {
        a[i + n_branches / 2] = a[i];
    }
    for (uint8_t i = 0; i < n_branches / 2; i++) {
        a[i] = b[i];
    }
    for (uint8_t i = n_branches / 2; i < n_branches; i++) {
        a[i] += a[n_branches / 2 - 1];
    }
    for (uint8_t i = 0; i < n_branches; i++) {
        ASSERT_EQ(a[i], uint64_t(i + 1));
    }
}

#endif