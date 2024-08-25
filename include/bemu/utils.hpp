#pragma once
#include <concepts>
#include <utility>

#include "types.hpp"

namespace bemu {
inline u16 combine_bytes(const u8 hi, const u8 lo) { return static_cast<u16>(hi) << 8 | lo; }

/// Splits a 16-bit value into its high and low bytes
inline std::pair<u8, u8> split_bytes(const u16 v) {
    return std::make_pair(static_cast<u8>(v >> 8), static_cast<u8>(v));
}

template <std::integral T>
void set_bit(T &v, const u8 bit, const bool value = true) {
    if (value) {
        v |= 1 << bit;
    } else {
        v &= ~(1 << bit);
    }
}

template <std::integral T>
bool get_bit(const T v, const u8 bit) {
    return (v & 1 << bit) != 0;
}
}  // namespace bemu