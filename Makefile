CFLAGS = -std=c++17 -Wall -Wextra -march=native

HEADERS = bit_vector/simple_node.hpp bit_vector/allocator.hpp bit_vector/simple_leaf.hpp

all: bv_debug

bv_debug: bv_debug.cpp $(HEADERS)
	g++ $(CFLAGS) -Ofast -o bv_debug bv_debug.cpp

debug: bv_debug.cpp $(HEADERS)
	g++ $(CFLAGS) -DDEBUG -O0 -g -o bv_debug bv_debug.cpp

bit_vector/%.hpp:

clean:
	rm bv_debug