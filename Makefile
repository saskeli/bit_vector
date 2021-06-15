CL = $(shell getconf LEVEL1_DCACHE_LINESIZE)

CFLAGS = -std=c++2a -Wall -Wextra -DCACHE_LINE=$(CL) -march=native

HEADERS = bit_vector/internal/node.hpp bit_vector/internal/allocator.hpp \
          bit_vector/internal/leaf.hpp bit_vector/internal/bit_vector.hpp \
		  bit_vector/bv.hpp

GTEST_DIR = deps/googletest/googletest
GFLAGS = -isystem $(GTEST_DIR)/include -I $(GTEST_DIR)/include -pthread

GTEST_HEADERS = $(GTEST_DIR)/include/gtest/*.h \
                $(GTEST_DIR)/include/gtest/internal/*.h

GTEST_SRCS_ = $(GTEST_DIR)/src/*.cc $(GTEST_DIR)/src/*.h $(GTEST_HEADERS)

TEST_CODE = test/leaf_tests.hpp test/node_tests.hpp test/test.cpp test/bv_tests.hpp

COVERAGE = -g

.PHONY: clean test cover clean_test

.DEFAULT: bv_debug

cover: COVERAGE = --coverage -O0 -g

all: bv_debug bench

bv_debug: bv_debug.cpp $(HEADERS)
	g++ $(CFLAGS) -DNDEBUG -Ofast -o bv_debug bv_debug.cpp

debug: bench.cpp bv_debug.cpp $(HEADERS)
	g++ $(CFLAGS) -DDEBUG -O0 -g -o bv_debug bv_debug.cpp

bench: bench.cpp $(HEADERS)
	g++ $(CFLAGS) -DNDEBUG -Ofast -o bench bench.cpp

brute: brute_force.cpp $(HEADERS)
	g++ $(CFLAGS) -DDEBUG -O0 -g -o brute brute_force.cpp

bit_vector/%.hpp:

bit_vector/internal/%.hpp:

test/%.hpp:

clean: clean_test
	rm -f bv_debug bench brute

clean_test:
	rm -f gtest_main.o gtest-all.o test/test.o test/test test/gtest_main.a \
	      *.gcda *.gcno *.gcov test/*.gcda test/*.gcno test/*.gcov index.info
	rm -rf target

gtest-all.o: $(GTEST_SRCS_)
	g++ $(CFLAGS) $(GFLAGS) -I$(GTEST_DIR) -c $(GTEST_DIR)/src/gtest-all.cc

gtest_main.o: $(GTEST_SRCS_)
	g++ $(CFLAGS) $(GFLAGS) -I$(GTEST_DIR) -c $(GTEST_DIR)/src/gtest_main.cc

gtest.a: gtest-all.o
	$(AR) $(ARFLAGS) $@ $^

test/gtest_main.a: gtest-all.o gtest_main.o
	$(AR) $(ARFLAGS) $@ $^

test/test.o: $(TEST_CODE) $(GTEST_HEADERS) $(HEADERS)
	g++ $(COVERAGE) $(CFLAGS) $(GFLAGS) -c test/test.cpp -o test/test.o

test: clean_test test/test.o test/gtest_main.a
	cd deps/googletest; cmake CMakeLists.txt
	make -C deps/googletest
	g++ $(CFLAGS) $(GFLAGS) -lpthread test/test.o test/gtest_main.a -o test/test
	test/test

cover: clean_test test/test.o test/gtest_main.a
	cd deps/googletest; cmake CMakeLists.txt
	make -C deps/googletest
	g++ $(COVERAGE) $(CFLAGS) $(GFLAGS) -lpthread test/test.o test/gtest_main.a -o test/test
	test/test
	gcov test/test.cpp
	lcov -c -d . -o index.info
	genhtml index.info -o target
