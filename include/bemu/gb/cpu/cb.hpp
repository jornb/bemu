#pragma once
#include "../cpu.hpp"
#include "../memory.hpp"

namespace bemu::gb::cpu {
namespace details::cb {
template <Register Register>
u8 get_operand(Cpu &cpu) {
    if constexpr (is_16bit(Register)) {
        const auto address = cpu.m_registers.get_u16(Register);
        return cpu.m_memory->read_u8(address);
    } else {
        return cpu.m_registers.get_u8(Register);
    }
}

template <Register Register>
void write_result(Cpu &cpu, const u8 value) {
    if constexpr (Register == Register::HL) {
        const auto address = cpu.m_registers.read(Register16::HL);
        cpu.m_memory->write_u8(address, value);
    } else {
        cpu.m_registers.set_u8(Register, value);
    }
}
}  // namespace details::cb

template <Register Register>
void rlc(Cpu &cpu) {
    auto value = details::cb::get_operand<Register>(cpu);

    const bool rotated_bit = get_bit(value, 7);
    value <<= 1;
    set_bit(value, 0, rotated_bit);

    details::cb::write_result<Register>(cpu, value);
    cpu.m_registers.set_flags(value == 0, false, false, rotated_bit);
}

template <Register Register>
void rrc(Cpu &cpu) {
    auto value = details::cb::get_operand<Register>(cpu);

    const bool rotated_bit = get_bit(value, 0);
    value >>= 1;
    set_bit(value, 7, rotated_bit);

    details::cb::write_result<Register>(cpu, value);
    cpu.m_registers.set_flags(value == 0, false, false, rotated_bit);
}

template <Register Register>
void rl(Cpu &cpu) {
    auto value = details::cb::get_operand<Register>(cpu);

    const bool rotated_bit = get_bit(value, 7);
    value <<= 1;
    set_bit(value, 0, cpu.m_registers.get_c());

    details::cb::write_result<Register>(cpu, value);
    cpu.m_registers.set_flags(value == 0, false, false, rotated_bit);
}

template <Register Register>
void rr(Cpu &cpu) {
    auto value = details::cb::get_operand<Register>(cpu);

    const bool rotated_bit = get_bit(value, 0);
    value >>= 1;
    set_bit(value, 7, cpu.m_registers.get_c());

    details::cb::write_result<Register>(cpu, value);
    cpu.m_registers.set_flags(value == 0, false, false, rotated_bit);
}

template <Register Register>
void sla(Cpu &cpu) {
    auto value = details::cb::get_operand<Register>(cpu);
    const bool shifted_bit = get_bit(value, 7);
    value <<= 1;

    details::cb::write_result<Register>(cpu, value);
    cpu.m_registers.set_flags(value == 0, false, false, shifted_bit);
}

template <Register Register>
void sra(Cpu &cpu) {
    const u8 input = details::cb::get_operand<Register>(cpu);
    s8 value_i = static_cast<s8>(input);
    value_i >>= 1;
    const u8 value = static_cast<u8>(value_i);

    details::cb::write_result<Register>(cpu, value);
    cpu.m_registers.set_flags(value == 0, false, false, get_bit(input, 0));
}

template <Register Register>
void swap(Cpu &cpu) {
    auto value = details::cb::get_operand<Register>(cpu);
    value = (value >> 4) | (value << 4);

    details::cb::write_result<Register>(cpu, value);
    cpu.m_registers.set_flags(value == 0, false, false, false);
}

template <Register Register>
void srl(Cpu &cpu) {
    auto value = details::cb::get_operand<Register>(cpu);
    const bool shifted_bit = get_bit(value, 0);
    value >>= 1;

    details::cb::write_result<Register>(cpu, value);
    cpu.m_registers.set_flags(value == 0, false, false, shifted_bit);
}

template <Register Register, size_t Bit>
void bit(Cpu &cpu) {
    auto value = details::cb::get_operand<Register>(cpu);

    cpu.m_registers.set_z(!get_bit(value, Bit));
    cpu.m_registers.set_n(false);
    cpu.m_registers.set_h(true);
}

template <Register Register, size_t Bit>
void res(Cpu &cpu) {
    auto value = details::cb::get_operand<Register>(cpu);
    set_bit(value, Bit, false);
    details::cb::write_result<Register>(cpu, value);
}

template <Register Register, size_t Bit>
void set(Cpu &cpu) {
    auto value = details::cb::get_operand<Register>(cpu);
    set_bit(value, Bit);
    details::cb::write_result<Register>(cpu, value);
}
}  // namespace bemu::gb::cpu