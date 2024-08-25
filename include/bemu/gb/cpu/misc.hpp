#pragma once
#include "../cpu.hpp"

namespace bemu::gb::cpu {

inline void nop(Cpu &) {}

inline void stop(Cpu &) { throw std::runtime_error("Stopped"); }

inline void halt(Cpu &cpu) { cpu.m_halted = true; }

inline void rlca(Cpu &cpu) {
    auto value = cpu.m_registers.a;
    const bool rotated_bit = get_bit(value, 7);
    value <<= 1;
    set_bit(value, 0, rotated_bit);

    cpu.m_registers.a = value;
    cpu.m_registers.set_flags(false, false, false, rotated_bit);
}

inline void rla(Cpu &cpu) {
    auto value = cpu.m_registers.a;
    const bool rotated_bit = get_bit(value, 7);
    value <<= 1;
    set_bit(value, 0, cpu.m_registers.get_c());

    cpu.m_registers.a = value;
    cpu.m_registers.set_flags(false, false, false, rotated_bit);
}

inline void rrca(Cpu &cpu) {
    auto value = cpu.m_registers.a;
    const bool rotated_bit = get_bit(value, 0);
    value >>= 1;
    set_bit(value, 7, rotated_bit);

    cpu.m_registers.a = value;
    cpu.m_registers.set_flags(false, false, false, rotated_bit);
}

inline void rra(Cpu &cpu) {
    auto value = cpu.m_registers.a;
    const bool rotated_bit = get_bit(value, 0);
    value >>= 1;
    set_bit(value, 7, cpu.m_registers.get_c());

    cpu.m_registers.a = value;
    cpu.m_registers.set_flags(false, false, false, rotated_bit);
}

inline void scf(Cpu &cpu) {
    cpu.m_registers.set_n(false);
    cpu.m_registers.set_h(false);
    cpu.m_registers.set_c(true);
}

inline void ccf(Cpu &cpu) {
    cpu.m_registers.set_n(false);
    cpu.m_registers.set_h(false);
    cpu.m_registers.set_c(!cpu.m_registers.get_c());
}

inline void cpl(Cpu &cpu) {
    cpu.m_registers.a = ~cpu.m_registers.a;
    cpu.m_registers.set_n(true);
    cpu.m_registers.set_h(true);
}

inline void daa(Cpu &cpu) {
    auto &reg = cpu.m_registers;

    if (reg.get_n()) {
        // Last instruction was a subtraction
        if (reg.get_c()) {
            reg.a -= 0x60;
        }
        if (reg.get_h()) {
            reg.a -= 0x6;
        }
    } else {
        // Last instruction was an addition
        if (reg.get_c() || reg.a > 0x99) {
            reg.a += 0x60;
            reg.set_c(true);
        }

        if (reg.get_h() || (reg.a & 0xF) > 0x9) {
            reg.a += 0x6;
        }
    }

    reg.set_z(reg.a == 0);
    reg.set_h(false);
}

inline void di(Cpu &cpu) { cpu.m_interrupt_master_enable = false; }

inline void ei(Cpu &cpu) { cpu.m_set_interrupt_master_enable_next_cycle = true; }

}  // namespace bemu::gb::cpu