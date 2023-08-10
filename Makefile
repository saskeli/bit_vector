CL = $(shell getconf LEVEL1_DCACHE_LINESIZE)

CFLAGS = -std=c++2a -Wall -Wextra -Wshadow -pedantic -Werror -DCACHE_LINE=$(CL) -march=native

INCLUDE = -I deps/hopscotch-map/include/ -isystem deps/DYNAMIC/include/

HEADERS = bit_vector/internal/node.hpp bit_vector/internal/allocator.hpp \
          bit_vector/internal/leaf.hpp bit_vector/internal/bit_vector.hpp \
		  bit_vector/bv.hpp bit_vector/internal/query_support.hpp \
		  bit_vector/internal/branch_selection.hpp \
		  bit_vector/internal/packed_array.hpp \
		  bit_vector/internal/gap_leaf.hpp

SDSL = -isystem deps/sdsl-lite/include -Ldeps/sdsl-lite/lib

GTEST_DIR = deps/googletest/googletest
GFLAGS = -isystem $(GTEST_DIR)/include -I $(GTEST_DIR)/include -pthread

GTEST_HEADERS = $(GTEST_DIR)/include/gtest/*.h \
                $(GTEST_DIR)/include/gtest/internal/*.h

GTEST_SRCS_ = $(GTEST_DIR)/src/*.cc $(GTEST_DIR)/src/*.h $(GTEST_HEADERS)

TEST_CODE = test/leaf_tests.hpp test/node_tests.hpp test/test.cpp test/bv_tests.hpp \
            test/branch_selection_test.hpp test/query_support_test.hpp test/run_tests.hpp \
			test/packed_array_test.hpp test/gap_leaf_test.hpp

COVERAGE = -g

.PHONY: clean test cover clean_test update_git

.DEFAULT: bv_debug

cover: COVERAGE = --coverage -O0 -g

all: bv_debug bench brute

bv_debug: bv_debug.cpp $(HEADERS) test/run_tests.hpp test/gtest_main.a
	make -C deps/sdsl-lite
	g++ $(CFLAGS) $(INCLUDE) -DDEBUG $(SDSL) $(GFLAGS) -g -o bv_debug bv_debug.cpp -lsdsl

bench: bench.cpp $(HEADERS)
	make -C deps/sdsl-lite
	g++ $(CFLAGS) $(INCLUDE) -DNDEBUG $(SDSL) -Ofast -o bench bench.cpp -lsdsl

comp_bench: comp_bench.cpp $(HEADERS)
	g++ $(CFLAGS) -DNDEBUG -Ofast -o comp_bench comp_bench.cpp

comp_comp: comp_comp.cpp $(HEADERS)
	g++ $(CFLAGS) -DDEBUG -Ofast -o comp_comp comp_comp.cpp

brute: brute_force.cpp $(HEADERS) test/run_tests.hpp
	make -C deps/sdsl-lite
	g++ $(CFLAGS) $(INCLUDE) -DDEBUG $(SDSL) -Ofast -o brute brute_force.cpp -lsdsl

queries: queries.cpp $(HEADERS)
	g++ $(CFLAGS) -g -DDEBUG -O0 -o queries queries.cpp

rem_bench: rem_bench.cpp $(HEADERS)
	g++ $(CFLAGS) $(INCLUDE) -DNDEBUG -Ofast -o rem_bench rem_bench.cpp

leaf_michrobench: leaf_michrobench.cpp $(HEADERS)
	g++ $(CFLAGS) $(INCLUDE) -DNDEBUG -g -Ofast -o leaf_michrobench leaf_michrobench.cpp

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
	rm -f bv_debug bench brute queries

clean_test:
	rm -f gtest_main.o gtest-all.o test/test.o test/test test/gtest_main.a \
	      *.gcda *.gcno *.gcov test/*.gcda test/*.gcno test/*.gcov index.info
	rm -rf target

update_git:
	git submodule update
	(cd deps/sdsl-lite && cmake CMakeLists.txt)


gtest-all.o: $(GTEST_SRCS_)
	g++ $(CFLAGS) $(GFLAGS) -I$(GTEST_DIR) -c $(GTEST_DIR)/src/gtest-all.cc

gtest_main.o: $(GTEST_SRCS_)
	g++ $(CFLAGS) $(GFLAGS) -I$(GTEST_DIR) -c $(GTEST_DIR)/src/gtest_main.cc

gtest.a: gtest-all.o
	$(AR) $(ARFLAGS) $@ $^

test/gtest_main.a: gtest-all.o gtest_main.o
	$(AR) $(ARFLAGS) $@ $^

test/test.o: $(TEST_CODE) $(GTEST_HEADERS) $(HEADERS)
	g++ $(COVERAGE) $(CFLAGS) $(GFLAGS) $(INCLUDE) $(SDSL) -DDEBUG -DGTEST_ON -c test/test.cpp -o test/test.o -lsdsl

test: clean_test test/test.o test/gtest_main.a
	git submodule update
	cd deps/googletest; cmake CMakeLists.txt
	make -C deps/googletest
	g++ $(CFLAGS) $(GFLAGS) $(INCLUDE) $(SDSL) -DDEBUG -DGTEST_ON -lpthread test/test.o test/gtest_main.a -o test/test -lsdsl
	test/test

cover: clean_test test/test.o test/gtest_main.a
	git submodule update
	cd deps/googletest; cmake CMakeLists.txt
	make -C deps/googletest
	g++ $(COVERAGE) $(CFLAGS) $(INCLUDE) $(GFLAGS) $(SDSL) -DDEBUG -DGTEST_ON -lpthread test/test.o test/gtest_main.a -o test/test -lsdsl
	test/test
	gcov test/test.cpp
	lcov -c -d . --ignore-errors mismatch -o index.info
	genhtml index.info -o target
