CL = $(shell getconf LEVEL1_DCACHE_LINESIZE)

CFLAGS = -std=c++2a -Wall -Wextra -pedantic -DCACHE_LINE=$(CL) -march=native

INCLUDE = -I deps/hopscotch-map/include/ -I deps/DYNAMIC/include/

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

.PHONY: clean test cover clean_test update_git

.DEFAULT: bv_debug

cover: COVERAGE = --coverage -O0 -g

all: bv_debug bench brute

bv_debug: bv_debug.cpp $(HEADERS) update_git
	g++ $(CFLAGS) $(INCLUDE) -DDEBUG -Ofast -g -o bv_debug bv_debug.cpp

bench: bench.cpp $(HEADERS) update_git
	(cd deps/sdsl-lite && cmake CMakeLists.txt)
	make -C deps/sdsl-lite
	g++ $(CFLAGS) $(INCLUDE) -DNDEBUG -Ideps/sdsl-lite/include -Ldeps/sdsl-lite/lib -Ofast -o bench bench.cpp -lsdsl

brute: brute_force.cpp $(HEADERS) update_git
	g++ $(CFLAGS) $(INCLUDE) -DDEBUG -O0 -g -o brute brute_force.cpp

rem_bench: rem_bench.cpp $(HEADERS) update_git
	g++ $(CFLAGS) $(INCLUDE) -DNDEBUG -Ofast -o rem_bench rem_bench.cpp

bit_vector/%.hpp:

bit_vector/internal/%.hpp:

test/%.hpp:

benchmarking/b%.cpp: benchmarking/gen_code.py
	python $< $* $@

benchmarking/b%: benchmarking/b%.cpp
	g++ $(CFLAGS) $(INCLUDE) -DNDEBUG -Ofast -o $@ $<

run%: benchmarking/b%
	benchmarking/b$* 1337 1000000000 100
	rm -f benchmarking/b$*

clean: clean_test
	rm -f bv_debug bench brute

clean_test:
	rm -f gtest_main.o gtest-all.o test/test.o test/test test/gtest_main.a \
	      *.gcda *.gcno *.gcov test/*.gcda test/*.gcno test/*.gcov index.info
	rm -rf target

update_git:
	git submodule update

gtest-all.o: $(GTEST_SRCS_)
	g++ $(CFLAGS) $(GFLAGS) -I$(GTEST_DIR) -c $(GTEST_DIR)/src/gtest-all.cc

gtest_main.o: $(GTEST_SRCS_)
	g++ $(CFLAGS) $(GFLAGS) -I$(GTEST_DIR) -c $(GTEST_DIR)/src/gtest_main.cc

gtest.a: gtest-all.o
	$(AR) $(ARFLAGS) $@ $^

test/gtest_main.a: gtest-all.o gtest_main.o
	$(AR) $(ARFLAGS) $@ $^

test/test.o: $(TEST_CODE) $(GTEST_HEADERS) $(HEADERS)
	g++ $(COVERAGE) $(CFLAGS) $(GFLAGS) $(INCLUDE) -c test/test.cpp -o test/test.o

test: clean_test test/test.o test/gtest_main.a
	cd deps/googletest; cmake CMakeLists.txt
	make -C deps/googletest
	g++ $(CFLAGS) $(GFLAGS) $(INCLUDE) -lpthread test/test.o test/gtest_main.a -o test/test
	test/test

cover: clean_test test/test.o test/gtest_main.a
	cd deps/googletest; cmake CMakeLists.txt
	make -C deps/googletest
	g++ $(COVERAGE) $(CFLAGS) $(INCLUDE) $(GFLAGS) -lpthread test/test.o test/gtest_main.a -o test/test
	test/test
	gcov test/test.cpp
	lcov -c -d . -o index.info
	genhtml index.info -o target
