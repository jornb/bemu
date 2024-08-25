#pragma once
#include "bus.hpp"
#include "enums.hpp"
#include "types.hpp"
#include "utils.hpp"

namespace bemu::gb {

struct CpuRegisters {
    u8 a = 0x01;  ///< Accumulator

    /// Flags
    ///
    /// Bit 7: Z - Zero flag
    /// Bit 6: N - Subtract flag
    /// Bit 5: H - Half carry flag
    /// Bit 4: C - Carry flag
    /// Bits 3..0: 0
    u8 f = 0xB0;

    u8 b = 0x00;
    u8 c = 0x13;
    u8 d = 0x00;
    u8 e = 0xD8;
    u8 h = 0x01;
    u8 l = 0x4D;
    u16 pc = 0x0100;  ///< Program counter
    u16 sp = 0xFFFE;  ///< Stack pointer

    [[nodiscard]] u8 get_u8(RegisterType type) const;
    void set_u8(RegisterType type, u8 value);

    [[nodiscard]] u16 get_u16(RegisterType type) const;
    void set_u16(RegisterType type, u16 value);
};

struct Emulator;

struct Cpu {
    Emulator &m_emulator;

    CpuRegisters m_registers;

    // current fetch...
    u16 m_fetched_data = 0;
    u16 m_memory_destination = 0;
    bool m_destination_is_memory;
    u8 m_current_opcode;
    // instruction *cur_inst;

    bool m_halted = false;
    bool m_stepping = false;

    bool m_int_master_enabled = false;
    bool m_enabling_ime = false;
    u8 m_ie_register = 0x00;
    u8 m_int_flags;

    u8 fetch_u8();

    bool step();
    void execute_next_instruction();
    void execute_jp(u16 address);
    void execute_ld(RegisterType reg, u8 value);
    void execute_ld(u16 address, RegisterType reg);
    void execute_inc(RegisterType reg);
};
}  // namespace bemu::gb
