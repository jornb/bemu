#pragma once
#include "../cpu.hpp"

namespace bemu::gb::cpu {
template <Register16 Register>
void push(Cpu &cpu) {
    cpu.add_cycle();
    cpu.stack_push16(cpu.m_registers.read(Register));
}

template <Register16 Register>
void pop(Cpu &cpu) {
    const auto data = cpu.stack_pop16();
    cpu.m_registers.write(Register, data);
}
}  // namespace bemu::gb::cpu