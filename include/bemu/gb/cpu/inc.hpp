#pragma once

#include "../cpu.hpp"

namespace bemu::gb::cpu {
template <Register8 Register>
void inc(Cpu &cpu) {
    // 8-bit direct increment, e.g. INC B. One cycle.
    const u8 old_value = cpu.m_registers.read(Register);
    const u8 new_value = old_value + 1;

    cpu.m_registers.write(Register, new_value);

    cpu.m_registers.set_z(new_value == 0);
    cpu.m_registers.set_n(false);
    cpu.m_registers.set_h((old_value & 0xF) + 1 > 0xF);
}

template <Register16 Register>
void inc(Cpu &cpu) {
    // 16-bit direct increment, e.g. INC DE. Two cycles.
    const u16 old_value = cpu.m_registers.read(Register);
    const u16 new_value = old_value + 1;

    cpu.m_registers.write(Register, new_value);

    // 16-bit operations take 1 extra cycle and don't modify flags
    cpu.add_cycle();
}

inline void inc_HLind(Cpu &cpu) {
    // 16-bit indirect increment, e.g. INC [HL]
    const u16 address = cpu.m_registers.read(Register16::HL);

    // Calculations reading/calc/write happens on the same cycle
    const u8 lo = cpu.m_memory->read_u8(address);
    const u8 hi = cpu.m_memory->peek_u8(address + 1);
    const u16 old_value = combine_bytes(hi, lo);
    const u8 new_value = old_value + 1;
    cpu.m_memory->write_u8(address, new_value);

    cpu.m_registers.set_z(new_value == 0);
    cpu.m_registers.set_n(false);
    cpu.m_registers.set_h((old_value & 0xF) + 1 > 0xF);
}
}  // namespace bemu::gb::cpu