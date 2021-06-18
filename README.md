# Fast dynamic bit vector headers

## Compiling headers

If you wish to use the headers in your project, it should suffice to add the repository as a submodule (non-recursively) or copy the headers in the `bit_vector` sub folder.

To use. Simply `#include bit_vector/bv.hpp`.

A compiler supporting `c++ 20` is required. All testing has been done with `g++` and there is no guarantee that other compilers will work.

Branching operations do aggressive cache pre-fetching. Due to this, `CACHE_LINE` with the cache line size should be passed to the compiler. In the `Makefile` this is done by running `CL = $(shell getconf LEVEL1_DCACHE_LINESIZE)`, and passing the returned value to the compiler with `-DCACHE_LINE=$``(CL)`.

See the `Makefile` for more.

## Compiling and running benchmarks

Clone the repository recursively (`git clone --recursive https://github.com/saskeli/bit_vector.git`) to ensure availability of [DYNAMIC](https://github.com/xxsds/DYNAMIC).

If a sufficiently new version of g++ is available, you can simply compile the benchmarking binaries with `make bench`.

## Requirements

### Testing and Test coverage

Tests are built using `googletest`. `googletest` is defined as a sub-module of the repository and should be automatically downloaded during a recursive clone.

For generating coverage reports gcov, lcov and genhtml are used. If missing, these should be available on distribution package managers.

If you do not want to run tests, the `googletest` submodule is not needed.

If you do not want to generate coverage reports, gcov, lcov and genhtml are not required.

### API documentation

Installing [Doxygen](https://www.doxygen.nl/index.html) from a distribution package manager should suffice.

If you do not need to generate documentation, you do not need doxygen.