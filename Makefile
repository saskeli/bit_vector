CFLAGS = -std=c++20 -Wall -Wextra -march=native

HEADERS = bit_vector/simple_node.hpp bit_vector/allocator.hpp bit_vector/simple_leaf.hpp

GTEST_DIR = deps/googletest/googletest
GFLAGS = -isystem $(GTEST_DIR)/include -I $(GTEST_DIR)/include -pthread

GTEST_HEADERS = $(GTEST_DIR)/include/gtest/*.h \
                $(GTEST_DIR)/include/gtest/internal/*.h

GTEST_SRCS_ = $(GTEST_DIR)/src/*.cc $(GTEST_DIR)/src/*.h $(GTEST_HEADERS)

all: bv_debug

bv_debug: bv_debug.cpp $(HEADERS)
	g++ $(CFLAGS) -DNDEBUG -Ofast -o bv_debug bv_debug.cpp

debug: bv_debug.cpp $(HEADERS)
	g++ $(CFLAGS) -DDEBUG -O0 -g -o bv_debug bv_debug.cpp

bit_vector/%.hpp:

test/helpers.hpp:

clean:
	rm bv_debug

gtest-all.o : $(GTEST_SRCS_)
	g++ $(CFLAGS) $(GFLAGS) -I$(GTEST_DIR) -c $(GTEST_DIR)/src/gtest-all.cc

gtest_main.o : $(GTEST_SRCS_)
	g++ $(CFLAGS) $(GFLAGS) -I$(GTEST_DIR) -c $(GTEST_DIR)/src/gtest_main.cc

gtest.a : gtest-all.o
	$(AR) $(ARFLAGS) $@ $^

test/gtest_main.a : gtest-all.o gtest_main.o
	$(AR) $(ARFLAGS) $@ $^

test/test.o : test/test.cpp $(GTEST_HEADERS) $(HEADERS) test/helpers.hpp
	g++ $(CFLAGS) $(GFLAGS) -c test/test.cpp -o test/test.o

test : test/test.o test/gtest_main.a
	git submodule update
	cd deps/googletest; cmake CMakeLists.txt
	make -C deps/googletest
	g++ $(CFLAGS) $(GFLAGS) -lpthread $^ -o test/test
	test/test