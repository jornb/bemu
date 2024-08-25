#pragma once
#include "../cpu.hpp"
#include "../memory.hpp"

namespace bemu::gb::cpu {

enum class IndirectOperation : u8 { None, Increment, Decrement };

template <Register8 Destination, Register8 Source>
void ld_r8_r8(Cpu &cpu) {
    const auto value = cpu.m_registers.read(Source);
    cpu.m_registers.write(Destination, value);
}

template <Register8 Destination>
void ld_r8_n8(Cpu &cpu) {
    const auto value = cpu.fetch_u8();
    cpu.m_registers.write(Destination, value);
}

template <Register8 Destination, Register16 Source, IndirectOperation Operation = IndirectOperation::None>
void ld_r8_r16ind(Cpu &cpu) {
    const auto address = cpu.m_registers.read(Source);

    if constexpr (Operation == IndirectOperation::Increment) {
        cpu.m_registers.write(Source, address + 1);
    } else if constexpr (Operation == IndirectOperation::Decrement) {
        cpu.m_registers.write(Source, address - 1);
    }

    const auto value = cpu.m_memory->read_u8(address);
    cpu.m_registers.write(Destination, value);
}

template <Register16 Destination, Register8 Source, IndirectOperation Operation = IndirectOperation::None>
void ld_r16ind_r8(Cpu &cpu) {
    const auto address = cpu.m_registers.read(Destination);

    if constexpr (Operation == IndirectOperation::Increment) {
        cpu.m_registers.write(Destination, address + 1);
    } else if constexpr (Operation == IndirectOperation::Decrement) {
        cpu.m_registers.write(Destination, address - 1);
    }

    const auto value = cpu.m_registers.read(Source);
    cpu.m_memory->write_u8(address, value);
}

template <Register16 Destination>
void ld_r16ind_n8(Cpu &cpu) {
    const auto address = cpu.m_registers.read(Destination);
    const auto value = cpu.fetch_u8();
    cpu.m_memory->write_u8(address, value);
}

template <Register8 Source>
void ld_a16_r8(Cpu &cpu) {
    const auto address = cpu.fetch_u16();
    const auto value = cpu.m_registers.read(Source);
    cpu.m_memory->write_u8(address, value);
}

template <Register8 Destination>
void ld_r8_a16(Cpu &cpu) {
    const auto address = cpu.fetch_u16();
    const auto value = cpu.m_memory->read_u8(address);
    cpu.m_registers.write(Destination, value);
}

template <Register8 Source>
void ld_a8_r8(Cpu &cpu) {
    const u16 address = 0xFF00 + cpu.fetch_u8();
    const auto value = cpu.m_registers.read(Source);
    cpu.m_memory->write_u8(address, value);
}

template <Register8 Destination>
void ld_r8_a8(Cpu &cpu) {
    const u16 address = 0xFF00 + cpu.fetch_u8();
    const auto value = cpu.m_memory->read_u8(address);
    cpu.m_registers.write(Destination, value);
}

template <Register16 Destination>
void ld_r16_n16(Cpu &cpu) {
    const auto value = cpu.fetch_u16();
    cpu.m_registers.write(Destination, value);
}

template <Register16 Destination, Register16 Source>
void ld_r16_r16(Cpu &cpu) {
    const auto value = cpu.m_registers.read(Source);
    cpu.m_registers.write(Destination, value);
    cpu.add_cycle();
}

template <Register8 Destination, Register8 Source>
void ld_r8ind_r8(Cpu &cpu) {
    const u16 address = 0xFF00 + cpu.m_registers.read(Destination);
    const auto value = cpu.m_registers.read(Source);
    cpu.m_memory->write_u8(address, value);
}

template <Register8 Destination, Register8 Source>
void ld_r8_r8ind(Cpu &cpu) {
    const u16 address = 0xFF00 + cpu.m_registers.read(Source);
    const auto value = cpu.m_memory->read_u8(address);
    cpu.m_registers.write(Destination, value);
}

inline void ld_HL_SP_e8(Cpu &cpu) {
    const int old_value = cpu.m_registers.sp;
    const int added_value = static_cast<s8>(cpu.fetch_u8());
    const int new_value_int = static_cast<int>(old_value) + added_value;
    const auto new_value = static_cast<u16>(new_value_int);

    cpu.m_registers.write(Register16::HL, new_value);

    cpu.m_registers.set_z(false);
    cpu.m_registers.set_n(false);
    cpu.m_registers.set_h((old_value & 0xF) + (added_value & 0xF) > 0xF);
    cpu.m_registers.set_c((old_value & 0xFF) + (added_value & 0xFF) > 0xFF);

    // 3 cycles in total
    //      * Read opcode
    //      * Read operand
    //      * This one
    cpu.add_cycle();
}

inline void ld_a16_SP(Cpu &cpu) {
    const auto address = cpu.fetch_u16();
    const auto value = cpu.m_registers.sp;
    cpu.m_memory->write_u16(address, value);
}
}  // namespace bemu::gb::cpu