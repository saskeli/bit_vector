all: bv_debug

bv_debug: bv_debug.cpp bit_vector/simple_node.hpp bit_vector/allocator.hpp bit_vector/simple_leaf.hpp
	g++ -std=c++17 -Wall -Wextra -Ofast -march=native -o bv_debug bv_debug.cpp

debug: bv_debug.cpp bit_vector/simple_node.hpp bit_vector/allocator.hpp bit_vector/simple_leaf.hpp
	g++ -std=c++17 -Wextra -Wall -O0 -g -march=native -o bv_debug bv_debug.cpp

bit_vector/simple_node.hpp:

bit_vector/allocator.hpp:

bit_vector/simple_leaf.hpp:

clean:
	rm bv_debug