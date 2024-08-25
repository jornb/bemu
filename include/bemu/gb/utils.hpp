#pragma once

#include <utility>

#include "types.hpp"

namespace bemu::gb {
inline u16 combine_bytes(const u8 hi, const u8 lo) { return static_cast<u16>(hi) << 8 | lo; }
inline std::pair<u8, u8> split_bytes(const u16 v) { return std::make_pair(v >> 8, v); }
}  // namespace bemu::gb