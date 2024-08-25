#pragma once

#include <utility>

#include "types.hpp"

namespace bemu::gb {
inline u16 combine_bytes(const u8 hi, const u8 lo) { return static_cast<u16>(hi) << 8 | lo; }
inline std::pair<u8, u8> split_bytes(const u16 v) { return std::make_pair(v >> 8, v); }

inline void set_bit(u8 &v, const u8 &bit, const bool value = true) {
    if (value) {
        v |= 1 << bit;
    } else {
        v &= ~(1 << bit);
    }
}
}  // namespace bemu::gb