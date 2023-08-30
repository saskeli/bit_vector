#pragma once

#include <array>
#include <cstdint>

#include "bv.hpp"

namespace dbv {

template <std::array mapping, std::array repr, std::array code_lengths,
          std::array r_map, class bv>
class Wavelet_tree {
   private:
    static const constexpr uint16_t CODE_LENGTH = []() constexpr {
        uint16_t m_v = 0;
        for (auto v : code_lengths) {
            m_v = std::max(m_v, v);
        }
        return m_v;
    }();
    static const constexpr uint16_t WT_NODES = uint16_t(1) << CODE_LENGTH;
    static_assert(WT_NODES >= 2);

    struct WT_node {
        uint64_t size = 0;
        uint64_t left = 0;
    };

    WT_node nodes[WT_NODES];
    bv bit_vector;

   public:
    Wavelet_tree() : nodes(), bit_vector() { }

    void insert(uint64_t idx, uint8_t e) {
        e = mapping[e];
        uint8_t code = repr[e];
        uint8_t c_len = code_lengths[e];
        uint16_t node_id = 1;
        uint64_t inherited_offset = 0;
        for (uint16_t i = 0; i < c_len; ++i) {
            bool v = (code >> i) & uint8_t(1);
            bit_vector.insert(nodes[node_id].left + inherited_offset + idx, v);
            ++nodes[node_id].size;
            if (v) {
                idx = bit_vector.rank(nodes[node_id].left + inherited_offset + idx);
                idx -= bit_vector.rank(nodes[node_id].left + inherited_offset);
                inherited_offset += nodes[node_id].left + nodes[node_id].size;
                node_id = node_id * 2 + 1;
            } else {
                idx = bit_vector.rank0(nodes[node_id].left + inherited_offset + idx);
                idx -= bit_vector.rank0(nodes[node_id].left + inherited_offset);
                nodes[node_id].left += CODE_LENGTH - i - 1;
                node_id *= 2;
            }
        }
    }

    uint8_t access(uint64_t idx) const {
        uint8_t e = 0;

        uint16_t node_id = 1;
        uint64_t inherited_offset = 0;
        for (uint16_t i = 0; i < CODE_LENGTH; ++i) {
            if (nodes[node_id].size == 0) [[unlikely]] {
                break;
            } 
            uint64_t s_offset = nodes[node_id].left + inherited_offset;
            if (bit_vector.at(s_offset + idx);) {
                idx = bit_vector.rank(s_offset + idx);
                idx -= bit_vector.rank(s_offset);
                inherited_offset += nodes[node_id].left + nodes[node_id].size;
                e |= uint8_t(1) << i;
                node_id = node_id * 2 + 1;
            } else {
                idx = bit_vector.rank0(s_offset + idx);
                idx -= bit_vector.rank0(s_offset);
                node_id *= 2;
            }

        }
        return r_map[r];
    }
};

typedef Wavelet_tree<[]() constexpr {
        std::array<uint8_t, 256> ret;
        for (uint16_t i = 0; i < 256; ++i) {
            ret[i] = i;
        }
        return ret;
    }(),
    []() constexpr {
        std::array<uint8_t, 256> ret;
        for (uint16_t i = 0; i < 256; ++i) {
            ret[i] = i;
        }
        return ret;
    }(),
    []() constexpr {
        std::array<uint8_t, 256> ret;
        for (uint16_t i = 0; i < 256; ++i) {
            ret[i] = 8;
        }
        return ret;
    }(),
    []() constexpr {
        std::array<uint8_t, 256> ret;
        for (uint16_t i = 0; i < 256; ++i) {
            ret[i] = i;
        }
        return ret;
    }(),
    bv> Byte_wt;
}  // namespace dbv
