
import sys
from itertools import product

types = {
#  id   Type   avx     branch leaf buffer
    0: ("dyn", "false", 8, 8192, 0),
    1: ("leaf", "false", 8, 8192, 0),
    2: ("leaf", "false", 8, 8192, 8)
}

branches = [8, 16, 32, 64, 128]
leaves = [2**12, 2**13, 2**14]
buffer = [8, 16, 32]
avx = ["true", "false"]

for br, l, bu, a in product(branches, leaves, buffer, avx):
    types[len(types)] = ("uint64_t", a, br, l, bu)

def write_header(f, name, is_avx, branch, leaf, buf):
    f.write("""#include <sys/resource.h>
#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
""")
    if (name == "dyn" or name == "leaf"):
        f.write('\n#include "dynamic/dynamic.hpp"\n')
        dyntype = "dyn::suc_bv"
        if (name == "leaf") :
            dyntype = "dyn::b_suc_bv" if buf > 0 else "dyn::ub_suc_bv"
        f.write(f"\ntypedef {dyntype} bit_vector;\n\n")
    else:
        f.write('\n#include "../bit_vector/bv.hpp"\n')
        f.write(f"\ntypedef bv::simple_bv<{buf}, {leaf}, {branch}, {is_avx}> bit_vector;\n\n")

def write_tester(f, name, is_avx, branch, leaf, buf):
    f.write(f"""
void test(uint64_t size, uint64_t steps, uint64_t seed) {{
    bit_vector bv;

    std::mt19937 mt(seed);
    std::uniform_int_distribution<unsigned long long> gen(
        std::numeric_limits<std::uint64_t>::min(),
        std::numeric_limits<std::uint64_t>::max());

    using std::chrono::duration_cast;
    using std::chrono::high_resolution_clock;
    using std::chrono::microseconds;

    struct rusage ru;

    uint64_t start_size = 1000000;

    double startexp = log2(double(start_size));
    double delta = (log2(double(size)) - log2(double(start_size))) / steps;
    uint64_t ops = 100000;
    std::cerr << "startexp: " << startexp << ". delta: " << delta << std::endl;
    std::vector<uint64_t> loc, val;

    std::cout
        << "type\\tavx\\tbuffer\\tbranch\\tleaf_size\\tseed\\tsize\\tremove\\tinsert\\t"
        << "set\\taccess\\trank\\tselect\\tsize(bits)\\trss\\tchecksum" << std::endl;

    for (uint64_t i = 0; i < 900000; i++) {{
        uint64_t aloc = gen(mt) % (i + 1);
        bool aval = gen(mt) % 2;
        bv.insert(aloc, aval);
    }}
    for (uint64_t step = 1; step <= steps; step++) {{
        uint64_t start = bv.size();
        uint64_t target = uint64_t(pow(2.0, startexp + delta * step));

        uint64_t checksum = 0;
        std::cout << "{name}\\t{is_avx}\\t{buf}\\t{branch}\\t{leaf}\\t" 
                  << seed << "\\t" << target << "\\t";

        for (size_t i = start; i < target; i++) {{
            uint64_t aloc = gen(mt) % (i + 1);
            bool aval = gen(mt) % 2;
            bv.insert(aloc, aval);
        }}

        loc.clear();
        for (uint64_t i = target; i > target - ops; i--) {{
            loc.push_back(gen(mt) % i);
        }}

        auto t1 = high_resolution_clock::now();
        for (size_t i = 0; i < ops; i++) {{
            bv.remove(loc[i]);
        }}
        auto t2 = high_resolution_clock::now();
        std::cout << (double)duration_cast<microseconds>(t2 - t1).count() / ops
                  << "\\t";

        loc.clear();
        val.clear();
        for (uint64_t i = bv.size(); i < target; i++) {{
            loc.push_back(gen(mt) % (i + 1));
            val.push_back(gen(mt) % 2);
        }}

        t1 = high_resolution_clock::now();
        for (size_t i = 0; i < loc.size(); i++) {{
            bv.insert(loc[i], val[i]);
        }}
        t2 = high_resolution_clock::now();
        std::cout << (double)duration_cast<microseconds>(t2 - t1).count() / ops
                  << "\\t";

        loc.clear();
        val.clear();
        for (uint64_t i = 0; i < ops; i++) {{
            loc.push_back(gen(mt) % target);
            val.push_back(gen(mt) % 2);
        }}

        t1 = high_resolution_clock::now();
        for (size_t i = 0; i < loc.size(); i++) {{
            bv.set(loc[i], val[i]);
        }}
        t2 = high_resolution_clock::now();
        std::cout << (double)duration_cast<microseconds>(t2 - t1).count() / ops
                  << "\\t";

        loc.clear();
        for (size_t i = 0; i < ops; i++) {{
            loc.push_back(gen(mt) % target);
        }}

        t1 = high_resolution_clock::now();
        for (size_t i = 0; i < ops; i++) {{
            checksum += bv.at(loc[i]);
        }}
        t2 = high_resolution_clock::now();
        std::cout << (double)duration_cast<microseconds>(t2 - t1).count() / ops
                  << "\\t";

        loc.clear();
        for (size_t i = 0; i < ops; i++) {{
            loc.push_back(gen(mt) % target);
        }}

        t1 = high_resolution_clock::now();
        for (size_t i = 0; i < ops; i++) {{
            checksum += bv.rank(loc[i]);
        }}
        t2 = high_resolution_clock::now();
        std::cout << (double)duration_cast<microseconds>(t2 - t1).count() / ops
                  << "\\t";

        uint64_t limit = bv.rank(target - 1);
        loc.clear();
        for (size_t i = 0; i < ops; i++) {{
            loc.push_back(gen(mt) % limit);
        }}

        t1 = high_resolution_clock::now();
        for (size_t i = 0; i < ops; i++) {{
            checksum += bv.select(loc[i]{"" if name == "dyn" or name == "leaf" else " + 1"});
        }}
        t2 = high_resolution_clock::now();
        std::cout << (double)duration_cast<microseconds>(t2 - t1).count() / ops
                  << "\\t";

        std::cout << bv.bit_size() << "\\t";
        getrusage(RUSAGE_SELF, &ru);
        std::cout << ru.ru_maxrss * 8 * 1024 << "\\t";

        std::cout << checksum << std::endl;
    }}
}}
""")

def write_main(f):
    f.write("""
int main(int argc, char const *argv[]) {
    if (argc < 2) {
        return 0;
    }
    uint64_t seed;
    uint64_t size = 10000000;
    uint64_t steps = 100;
    std::sscanf(argv[1], "%lu", &seed);
    if (argc > 2) {
        std::sscanf(argv[2], "%lu", &size);
        if (size < 10000000) {
            std::cerr << "Invalid size argument" << std::endl;
            return 1;
        }
    }
    if (argc > 3) {
        std::sscanf(argv[3], "%lu", &steps);
    }
    test(size, steps, seed);
}""")

def main(t_id, f_path):
    if t_id < 0 or t_id >= len(types):
        print(f"Invalid type {t_id} (should be in range [0..{len(types) - 1}]).", file=sys.stderr)
        exit(1)
    print(f"Generating code for test case {t_id} (in range [0..{len(types) - 1}]).", file=sys.stderr)
    name, is_avx, branch, leaf, buf = types[t_id]
    with open(f_path, 'w') as out_file:
        write_header(out_file, name, is_avx, branch, leaf, buf)
        write_tester(out_file, name, is_avx, branch, leaf, buf)
        write_main(out_file)


if __name__ == "__main__":
    if len(sys.argv) > 2:
        main(int(sys.argv[1]), sys.argv[2])
