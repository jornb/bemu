#pragma once
#include "../cpu.hpp"

namespace bemu::gb::cpu {
template <Condition Cond>
void jr(Cpu &cpu) {
    const auto offset = static_cast<s8>(cpu.fetch_u8());
    const auto address = cpu.m_registers.pc + offset;

    if (cpu.m_registers.check_flags(Cond)) {
        cpu.m_registers.pc = address;
        cpu.add_cycle();
    }
}

template <Condition Cond>
void jp(Cpu &cpu) {
    const auto address = cpu.fetch_u16();

    if (cpu.m_registers.check_flags(Cond)) {
        cpu.m_registers.pc = address;
        cpu.add_cycle();
    }
}

inline void jp_HL(Cpu &cpu) {
    // Special case for jumping straight to HL in 1 M-cycle
    cpu.m_registers.pc = cpu.m_registers.read(Register16::HL);
}

template <Condition Cond, bool enable_interrupt = false>
void ret(Cpu &cpu) {
    if constexpr (enable_interrupt) {
        cpu.m_interrupt_master_enable = true;
    }

    if constexpr (Cond != Condition::NoCondition) {
        // First cycle checks flags
        const auto check = cpu.m_registers.check_flags(Cond);
        cpu.add_cycle();

        if (check) {
            cpu.m_registers.pc = cpu.stack_pop16();
            cpu.add_cycle();
        }
    } else {
        cpu.m_registers.pc = cpu.stack_pop16();
        cpu.add_cycle();
    }
}

template <Condition Cond>
void call(Cpu &cpu) {
    const auto address = cpu.fetch_u16();

    if (cpu.m_registers.check_flags(Cond)) {
        cpu.stack_push16(cpu.m_registers.pc);
        cpu.m_registers.pc = address;
        cpu.add_cycle();
    }
}

template <u16 Address>
void rst(Cpu &cpu) {
    cpu.stack_push16(cpu.m_registers.pc);
    cpu.m_registers.pc = Address;
    cpu.add_cycle();
}
}  // namespace bemu::gb::cpu