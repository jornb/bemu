#pragma once
#include "../types.hpp"
#include "bus.hpp"
#include "enums.hpp"

namespace bemu::gb {

struct CpuRegisters {
  u8 a;  ///< Accumulator

  /// Flags
  ///
  /// Bit 7: Z - Zero flag
  /// Bit 6: N - Subtract flag
  /// Bit 5: H - Half carry flag
  /// Bit 4: C - Carry flag
  /// Bits 3..0: 0
  u8 f;

  u8 b;
  u8 c;
  u8 d;
  u8 e;
  u8 h;
  u8 l;
  u16 pc;  ///< Program counter
  u16 sp;  ///< Stack pointer

  u8 get_u8(RegisterType type);
  u16 get_u16(RegisterType type);
  void set_u8(RegisterType type, u8 value);
  void set_u16(RegisterType type, u16 value);
};
}  // namespace bemu::gb
