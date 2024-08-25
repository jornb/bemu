#pragma once

#include "types.hpp"

namespace bemu::gb {
/// CPU regsiter type
enum class RegisterType : u8 { None, A, F, B, C, D, E, H, L, AF, BC, DE, HL, SP, PC };

constexpr bool is_16bit(const RegisterType reg) { return reg >= RegisterType::AF; }

enum class OpcodeArgumentType {
    None,
    RegisterName,     ///< Name of a register, e.g. RegisterType
    Data8,            ///< 8-bit data
    Data16,           ///< 16-bit data
    Address8,         ///< 8-bit (unsigned) address, sometimes added with 0xFF00
    Address16,        ///< 16-bit address
    RelativeAddress8  ///< Signed 8-bit address, relative to program counter
};

/// Bus addressing Mode
enum addr_mode {
    None,
    R_D16,
    R_R,
    MR_R,
    R,
    R_D8,
    R_MR,
    R_HLI,
    R_HLD,
    HLI_R,
    HLD_R,
    R_A8,
    A8_R,
    HL_SPR,
    D16,
    D8,
    D16_R,
    MR_D8,
    MR,
    A16_R,
    R_A16
};
}  // namespace bemu::gb
