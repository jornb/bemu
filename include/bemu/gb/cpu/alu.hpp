#pragma once
#include <tuple>

#include "../cpu.hpp"

namespace bemu::gb::cpu {

namespace details {
inline std::tuple<u8, bool, bool, bool, bool> add8(const int old_value, const int added_value,
                                                   const int old_carry = 0) {
    const int new_value = old_value + added_value + old_carry;
    const int new_value_half = (old_value & 0xF) + (added_value & 0xF) + old_carry;
    const u8 new_value_u8 = static_cast<u8>(new_value);
    return std::make_tuple(new_value_u8, new_value_u8 == 0, false, new_value_half > 0xF, new_value > 0xFF);
}

inline std::tuple<u8, bool, bool, bool, bool> sub8(const int old_value, const int subtracted_value,
                                                   const int old_carry = 0) {
    const int new_value = old_value - subtracted_value - old_carry;
    const int new_value_half = (old_value & 0xF) - (subtracted_value & 0xF) - old_carry;
    const u8 new_value_u8 = static_cast<u8>(new_value);
    return std::make_tuple(new_value_u8, new_value_u8 == 0, true, new_value_half < 0, new_value < 0);
}

template <Register Source>
u8 get_operand(Cpu &cpu) {
    if constexpr (is_16bit(Source)) {
        const auto address = cpu.m_registers.get_u16(Source);
        return cpu.m_memory->read_u8(address);
    } else {
        return cpu.m_registers.get_u8(Source);
    }
}
}  // namespace details

template <Register Source, bool Carry>
void add(Cpu &cpu) {
    const auto operand = details::get_operand<Source>(cpu);

    auto [result, z, n, h, c] = details::add8(cpu.m_registers.a, operand, cpu.m_registers.get_c() && Carry ? 1 : 0);

    cpu.m_registers.a = result;
    cpu.m_registers.set_flags(z, n, h, c);
}

template <Register16 Destination, Register16 Source>
void add(Cpu &cpu) {
    // 16-bit add
    const auto old_value = cpu.m_registers.read(Destination);
    const auto added_value = cpu.m_registers.read(Source);

    const auto new_value_int = static_cast<int>(old_value) + added_value;
    const auto new_value = static_cast<u16>(new_value_int);

    cpu.add_cycle();

    cpu.m_registers.write(Destination, new_value);

    cpu.m_registers.set_n(false);
    cpu.m_registers.set_h((old_value & 0xFFF) + (added_value & 0xFFF) > 0xFFF);
    cpu.m_registers.set_c((old_value & 0xFFFF) + (added_value & 0xFFFF) > 0xFFFF);
}

template <bool Carry>
void add_n8(Cpu &cpu) {
    const auto operand = cpu.fetch_u8();

    auto [result, z, n, h, c] = details::add8(cpu.m_registers.a, operand, cpu.m_registers.get_c() && Carry ? 1 : 0);

    cpu.m_registers.a = result;
    cpu.m_registers.set_flags(z, n, h, c);
}

inline void add_SP_e8(Cpu &cpu) {
    const auto old_value = cpu.m_registers.sp;
    const auto added_value = static_cast<s8>(cpu.fetch_u8());
    const auto new_value_int = static_cast<int>(old_value) + added_value;
    const auto new_value = static_cast<u16>(new_value_int);

    cpu.m_registers.sp = new_value;

    cpu.add_cycle();

    cpu.m_registers.set_z(false);
    cpu.m_registers.set_n(false);
    cpu.m_registers.set_h((old_value & 0xF) + (added_value & 0xF) > 0xF);
    cpu.m_registers.set_c((old_value & 0xFF) + (added_value & 0xFF) > 0xFF);

    cpu.add_cycle();
}

template <Register Source, bool Carry>
void sub(Cpu &cpu) {
    const auto operand = details::get_operand<Source>(cpu);

    auto [result, z, n, h, c] = details::sub8(cpu.m_registers.a, operand, cpu.m_registers.get_c() && Carry ? 1 : 0);

    cpu.m_registers.a = result;
    cpu.m_registers.set_flags(z, n, h, c);
}

template <bool Carry>
void sub_n8(Cpu &cpu) {
    const auto operand = cpu.fetch_u8();

    auto [result, z, n, h, c] = details::sub8(cpu.m_registers.a, operand, cpu.m_registers.get_c() && Carry ? 1 : 0);

    cpu.m_registers.a = result;
    cpu.m_registers.set_flags(z, n, h, c);
}

template <Register Source>
void logical_and(Cpu &cpu) {
    const auto operand = details::get_operand<Source>(cpu);

    cpu.m_registers.a &= operand;
    cpu.m_registers.set_flags(cpu.m_registers.a == 0, false, true, false);
}

inline void logical_and_n8(Cpu &cpu) {
    const auto operand = cpu.fetch_u8();

    cpu.m_registers.a &= operand;
    cpu.m_registers.set_flags(cpu.m_registers.a == 0, false, true, false);
}

template <Register Source>
void logical_or(Cpu &cpu) {
    const auto operand = details::get_operand<Source>(cpu);

    cpu.m_registers.a |= operand;
    cpu.m_registers.set_flags(cpu.m_registers.a == 0, false, false, false);
}

inline void logical_or_n8(Cpu &cpu) {
    const auto operand = cpu.fetch_u8();

    cpu.m_registers.a |= operand;
    cpu.m_registers.set_flags(cpu.m_registers.a == 0, false, false, false);
}

template <Register Source>
void logical_xor(Cpu &cpu) {
    const auto operand = details::get_operand<Source>(cpu);

    cpu.m_registers.a ^= operand;
    cpu.m_registers.set_flags(cpu.m_registers.a == 0, false, false, false);
}

inline void logical_xor_n8(Cpu &cpu) {
    const auto operand = cpu.fetch_u8();

    cpu.m_registers.a ^= operand;
    cpu.m_registers.set_flags(cpu.m_registers.a == 0, false, false, false);
}

template <Register Source>
void logical_cp(Cpu &cpu) {
    const int operand = details::get_operand<Source>(cpu);

    const auto result = cpu.m_registers.a - operand;
    const auto half_result = (cpu.m_registers.a & 0x0F) - (operand & 0x0F);

    cpu.m_registers.set_flags(result == 0, true, half_result < 0, result < 0);
}

inline void logical_cp_n8(Cpu &cpu) {
    const int operand = cpu.fetch_u8();

    const auto result = cpu.m_registers.a - operand;
    const auto half_result = (cpu.m_registers.a & 0x0F) - (operand & 0x0F);

    cpu.m_registers.set_flags(result == 0, true, half_result < 0, result < 0);
}
}  // namespace bemu::gb::cpu