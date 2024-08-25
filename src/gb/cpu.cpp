#include <spdlog/fmt/fmt.h>
#include <spdlog/fmt/ranges.h>
#include <spdlog/spdlog.h>

#include <array>
#include <bemu/gb/bus.hpp>
#include <bemu/gb/cpu.hpp>
#include <bemu/gb/cpu/alu.hpp>
#include <bemu/gb/cpu/cb.hpp>
#include <bemu/gb/cpu/dec.hpp>
#include <bemu/gb/cpu/inc.hpp>
#include <bemu/gb/cpu/jump.hpp>
#include <bemu/gb/cpu/ld.hpp>
#include <bemu/gb/cpu/misc.hpp>
#include <bemu/gb/cpu/opcodes.hpp>
#include <bemu/gb/cpu/stack.hpp>
#include <bemu/gb/external.hpp>
#include <bemu/gb/lcd.hpp>
#include <bemu/gb/timer.hpp>
#include <bemu/utils.hpp>
#include <magic_enum/magic_enum.hpp>
#include <source_location>
#include <stdexcept>

using namespace bemu;
using namespace bemu::gb;

static std::array<u16, 5> g_interrupt_jump_addresses = {
    0x40,  // VBlank
    0x48,  // LCD
    0x50,  // Timer
    0x58,  // Serial
    0x60   // Joypad
};

bool CpuRegisters::get_z() const { return get_bit(f, 7); }
bool CpuRegisters::get_n() const { return get_bit(f, 6); }
bool CpuRegisters::get_h() const { return get_bit(f, 5); }
bool CpuRegisters::get_c() const { return get_bit(f, 4); }
void CpuRegisters::set_z(const bool z) { set_bit(f, 7, z); }
void CpuRegisters::set_n(const bool n) { set_bit(f, 6, n); }
void CpuRegisters::set_h(const bool h) { set_bit(f, 5, h); }
void CpuRegisters::set_c(const bool c) { set_bit(f, 4, c); }

void CpuRegisters::set_flags(const bool z, const bool n, const bool h, const bool c) {
    set_z(z);
    set_n(n);
    set_h(h);
    set_c(c);
}

bool CpuRegisters::check_flags(const Condition condition) const {
    switch (condition) {
        case Condition::Z: return get_z();
        case Condition::NZ: return !get_z();
        case Condition::C: return get_c();
        case Condition::NC: return !get_c();
        default: return true;
    }
}

u8 CpuRegisters::get_u8(const Register type) const {
    if (is_16bit(type)) {
        throw std::runtime_error(fmt::format("Tried to get CPU register {} as 8 bit", magic_enum::enum_name(type)));
    }

    switch (type) {
        case Register::A: return a;
        case Register::F: return f & 0xF0;
        case Register::B: return b;
        case Register::C: return c;
        case Register::D: return d;
        case Register::E: return e;
        case Register::H: return h;
        case Register::L: return l;
        default: throw std::runtime_error(fmt::format("Unknown CPU register {}", magic_enum::enum_name(type)));
    }
}

void CpuRegisters::set_u8(const Register type, const u8 value) {
    if (is_16bit(type)) {
        throw std::runtime_error(fmt::format("Tried to set CPU register {} as 8 bit", magic_enum::enum_name(type)));
    }

    switch (type) {
        case Register::A: a = value; break;
        case Register::F: f = value & 0xF0; break;
        case Register::B: b = value; break;
        case Register::C: c = value; break;
        case Register::D: d = value; break;
        case Register::E: e = value; break;
        case Register::H: h = value; break;
        case Register::L: l = value; break;
        default: throw std::runtime_error(fmt::format("Unknown CPU register {}", magic_enum::enum_name(type)));
    }
}

u16 CpuRegisters::get_u16(Register type) const {
    if (!is_16bit(type)) {
        throw std::runtime_error(fmt::format("Tried to get CPU register {} as 16 bit", magic_enum::enum_name(type)));
    }
    switch (type) {
        case Register::AF: return combine_bytes(a, f & 0xF0);
        case Register::BC: return combine_bytes(b, c);
        case Register::DE: return combine_bytes(d, e);
        case Register::HL: return combine_bytes(h, l);
        case Register::PC: return pc;
        case Register::SP: return sp;
        default: throw std::runtime_error(fmt::format("Unknown CPU register {}", magic_enum::enum_name(type)));
    }
}

void CpuRegisters::set_u16(const Register type, const u16 value) {
    if (!is_16bit(type)) {
        throw std::runtime_error(fmt::format("Tried to get CPU register {} as 16 bit", magic_enum::enum_name(type)));
    }
    auto [hi, lo] = split_bytes(value);

    switch (type) {
        case Register::AF: {
            a = hi;
            f = lo & 0xF0;
            break;
        }
        case Register::BC: {
            b = hi;
            c = lo;
            break;
        }
        case Register::DE: {
            d = hi;
            e = lo;
            break;
        }
        case Register::HL: {
            h = hi;
            l = lo;
            break;
        }
        case Register::PC: {
            pc = value;
            break;
        }
        case Register::SP: sp = value; break;

        default: throw std::runtime_error(fmt::format("Unknown CPU register {}", magic_enum::enum_name(type)));
    }
}

u8 CpuRegisters::read(const Register8 type) const {
    switch (type) {
        case Register8::A: return a;
        case Register8::F: return f & 0xF0;
        case Register8::B: return b;
        case Register8::C: return c;
        case Register8::D: return d;
        case Register8::E: return e;
        case Register8::H: return h;
        case Register8::L: return l;
        default: throw std::runtime_error(fmt::format("Unknown CPU register {}", magic_enum::enum_name(type)));
    }
}

u16 CpuRegisters::read(const Register16 type) const {
    switch (type) {
        case Register16::AF: return combine_bytes(a, f & 0xF0);
        case Register16::BC: return combine_bytes(b, c);
        case Register16::DE: return combine_bytes(d, e);
        case Register16::HL: return combine_bytes(h, l);
        case Register16::PC: return pc;
        case Register16::SP: return sp;
        default: throw std::runtime_error(fmt::format("Unknown CPU register {}", magic_enum::enum_name(type)));
    }
}

void CpuRegisters::write(const Register8 type, const u8 value) {
    switch (type) {
        case Register8::A: a = value; break;
        case Register8::F: f = value & 0xF0; break;
        case Register8::B: b = value; break;
        case Register8::C: c = value; break;
        case Register8::D: d = value; break;
        case Register8::E: e = value; break;
        case Register8::H: h = value; break;
        case Register8::L: l = value; break;
        default: throw std::runtime_error(fmt::format("Unknown CPU register {}", magic_enum::enum_name(type)));
    }
}

void CpuRegisters::write(const Register16 type, const u16 value) {
    auto [hi, lo] = split_bytes(value);

    switch (type) {
        case Register16::AF: {
            a = hi;
            f = lo & 0xF0;
            break;
        }
        case Register16::BC: {
            b = hi;
            c = lo;
            break;
        }
        case Register16::DE: {
            d = hi;
            e = lo;
            break;
        }
        case Register16::HL: {
            h = hi;
            l = lo;
            break;
        }
        case Register16::PC: {
            pc = value;
            break;
        }
        case Register16::SP: sp = value; break;

        default: throw std::runtime_error(fmt::format("Unknown CPU register {}", magic_enum::enum_name(type)));
    }
}

Cpu::Cpu() {
    // Loads - [0, 1, 2, 3]x2
    m_instruction_handlers[0x02] = &cpu::ld_r16ind_r8<Register16::BC, Register8::A>;
    m_instruction_handlers[0x12] = &cpu::ld_r16ind_r8<Register16::DE, Register8::A>;
    m_instruction_handlers[0x22] = &cpu::ld_r16ind_r8<Register16::HL, Register8::A, cpu::IndirectOperation::Increment>;
    m_instruction_handlers[0x32] = &cpu::ld_r16ind_r8<Register16::HL, Register8::A, cpu::IndirectOperation::Decrement>;

    // Loads - [0, 1, 2, 3]x6
    m_instruction_handlers[0x06] = &cpu::ld_r8_n8<Register8::B>;
    m_instruction_handlers[0x16] = &cpu::ld_r8_n8<Register8::D>;
    m_instruction_handlers[0x26] = &cpu::ld_r8_n8<Register8::H>;
    m_instruction_handlers[0x36] = &cpu::ld_r16ind_n8<Register16::HL>;

    // Loads - [0, 1, 2, 3]xA
    m_instruction_handlers[0x0A] = &cpu::ld_r8_r16ind<Register8::A, Register16::BC>;
    m_instruction_handlers[0x1A] = &cpu::ld_r8_r16ind<Register8::A, Register16::DE>;
    m_instruction_handlers[0x2A] = &cpu::ld_r8_r16ind<Register8::A, Register16::HL, cpu::IndirectOperation::Increment>;
    m_instruction_handlers[0x3A] = &cpu::ld_r8_r16ind<Register8::A, Register16::HL, cpu::IndirectOperation::Decrement>;

    // Loads - [0, 1, 2, 3]xE
    m_instruction_handlers[0x0E] = &cpu::ld_r8_n8<Register8::C>;
    m_instruction_handlers[0x1E] = &cpu::ld_r8_n8<Register8::E>;
    m_instruction_handlers[0x2E] = &cpu::ld_r8_n8<Register8::L>;
    m_instruction_handlers[0x3E] = &cpu::ld_r8_n8<Register8::A>;

    // Loads - 4x (LD B * | LD C * )
    m_instruction_handlers[0x40] = &cpu::ld_r8_r8<Register8::B, Register8::B>;
    m_instruction_handlers[0x41] = &cpu::ld_r8_r8<Register8::B, Register8::C>;
    m_instruction_handlers[0x42] = &cpu::ld_r8_r8<Register8::B, Register8::D>;
    m_instruction_handlers[0x43] = &cpu::ld_r8_r8<Register8::B, Register8::E>;
    m_instruction_handlers[0x44] = &cpu::ld_r8_r8<Register8::B, Register8::H>;
    m_instruction_handlers[0x45] = &cpu::ld_r8_r8<Register8::B, Register8::L>;
    m_instruction_handlers[0x46] = &cpu::ld_r8_r16ind<Register8::B, Register16::HL>;
    m_instruction_handlers[0x47] = &cpu::ld_r8_r8<Register8::B, Register8::A>;
    m_instruction_handlers[0x48] = &cpu::ld_r8_r8<Register8::C, Register8::B>;
    m_instruction_handlers[0x49] = &cpu::ld_r8_r8<Register8::C, Register8::C>;
    m_instruction_handlers[0x4A] = &cpu::ld_r8_r8<Register8::C, Register8::D>;
    m_instruction_handlers[0x4B] = &cpu::ld_r8_r8<Register8::C, Register8::E>;
    m_instruction_handlers[0x4C] = &cpu::ld_r8_r8<Register8::C, Register8::H>;
    m_instruction_handlers[0x4D] = &cpu::ld_r8_r8<Register8::C, Register8::L>;
    m_instruction_handlers[0x4E] = &cpu::ld_r8_r16ind<Register8::C, Register16::HL>;
    m_instruction_handlers[0x4F] = &cpu::ld_r8_r8<Register8::C, Register8::A>;

    // Loads - 5x (LD D * | LD E * )
    m_instruction_handlers[0x50] = &cpu::ld_r8_r8<Register8::D, Register8::B>;
    m_instruction_handlers[0x51] = &cpu::ld_r8_r8<Register8::D, Register8::C>;
    m_instruction_handlers[0x52] = &cpu::ld_r8_r8<Register8::D, Register8::D>;
    m_instruction_handlers[0x53] = &cpu::ld_r8_r8<Register8::D, Register8::E>;
    m_instruction_handlers[0x54] = &cpu::ld_r8_r8<Register8::D, Register8::H>;
    m_instruction_handlers[0x55] = &cpu::ld_r8_r8<Register8::D, Register8::L>;
    m_instruction_handlers[0x56] = &cpu::ld_r8_r16ind<Register8::D, Register16::HL>;
    m_instruction_handlers[0x57] = &cpu::ld_r8_r8<Register8::D, Register8::A>;
    m_instruction_handlers[0x58] = &cpu::ld_r8_r8<Register8::E, Register8::B>;
    m_instruction_handlers[0x59] = &cpu::ld_r8_r8<Register8::E, Register8::C>;
    m_instruction_handlers[0x5A] = &cpu::ld_r8_r8<Register8::E, Register8::D>;
    m_instruction_handlers[0x5B] = &cpu::ld_r8_r8<Register8::E, Register8::E>;
    m_instruction_handlers[0x5C] = &cpu::ld_r8_r8<Register8::E, Register8::H>;
    m_instruction_handlers[0x5D] = &cpu::ld_r8_r8<Register8::E, Register8::L>;
    m_instruction_handlers[0x5E] = &cpu::ld_r8_r16ind<Register8::E, Register16::HL>;
    m_instruction_handlers[0x5F] = &cpu::ld_r8_r8<Register8::E, Register8::A>;

    // Loads - 6x (LD H * | LD L * )
    m_instruction_handlers[0x60] = &cpu::ld_r8_r8<Register8::H, Register8::B>;
    m_instruction_handlers[0x61] = &cpu::ld_r8_r8<Register8::H, Register8::C>;
    m_instruction_handlers[0x62] = &cpu::ld_r8_r8<Register8::H, Register8::D>;
    m_instruction_handlers[0x63] = &cpu::ld_r8_r8<Register8::H, Register8::E>;
    m_instruction_handlers[0x64] = &cpu::ld_r8_r8<Register8::H, Register8::H>;
    m_instruction_handlers[0x65] = &cpu::ld_r8_r8<Register8::H, Register8::L>;
    m_instruction_handlers[0x66] = &cpu::ld_r8_r16ind<Register8::H, Register16::HL>;
    m_instruction_handlers[0x67] = &cpu::ld_r8_r8<Register8::H, Register8::A>;
    m_instruction_handlers[0x68] = &cpu::ld_r8_r8<Register8::L, Register8::B>;
    m_instruction_handlers[0x69] = &cpu::ld_r8_r8<Register8::L, Register8::C>;
    m_instruction_handlers[0x6A] = &cpu::ld_r8_r8<Register8::L, Register8::D>;
    m_instruction_handlers[0x6B] = &cpu::ld_r8_r8<Register8::L, Register8::E>;
    m_instruction_handlers[0x6C] = &cpu::ld_r8_r8<Register8::L, Register8::H>;
    m_instruction_handlers[0x6D] = &cpu::ld_r8_r8<Register8::L, Register8::L>;
    m_instruction_handlers[0x6E] = &cpu::ld_r8_r16ind<Register8::L, Register16::HL>;
    m_instruction_handlers[0x6F] = &cpu::ld_r8_r8<Register8::L, Register8::A>;

    // Loads - 7x
    m_instruction_handlers[0x70] = &cpu::ld_r16ind_r8<Register16::HL, Register8::B>;
    m_instruction_handlers[0x71] = &cpu::ld_r16ind_r8<Register16::HL, Register8::C>;
    m_instruction_handlers[0x72] = &cpu::ld_r16ind_r8<Register16::HL, Register8::D>;
    m_instruction_handlers[0x73] = &cpu::ld_r16ind_r8<Register16::HL, Register8::E>;
    m_instruction_handlers[0x74] = &cpu::ld_r16ind_r8<Register16::HL, Register8::H>;
    m_instruction_handlers[0x75] = &cpu::ld_r16ind_r8<Register16::HL, Register8::L>;
    m_instruction_handlers[0x77] = &cpu::ld_r16ind_r8<Register16::HL, Register8::A>;
    m_instruction_handlers[0x78] = &cpu::ld_r8_r8<Register8::A, Register8::B>;
    m_instruction_handlers[0x79] = &cpu::ld_r8_r8<Register8::A, Register8::C>;
    m_instruction_handlers[0x7A] = &cpu::ld_r8_r8<Register8::A, Register8::D>;
    m_instruction_handlers[0x7B] = &cpu::ld_r8_r8<Register8::A, Register8::E>;
    m_instruction_handlers[0x7C] = &cpu::ld_r8_r8<Register8::A, Register8::H>;
    m_instruction_handlers[0x7D] = &cpu::ld_r8_r8<Register8::A, Register8::L>;
    m_instruction_handlers[0x7E] = &cpu::ld_r8_r16ind<Register8::A, Register16::HL>;
    m_instruction_handlers[0x7F] = &cpu::ld_r8_r8<Register8::A, Register8::A>;

    // Loads - 16-bit
    m_instruction_handlers[0x01] = &cpu::ld_r16_n16<Register16::BC>;
    m_instruction_handlers[0x11] = &cpu::ld_r16_n16<Register16::DE>;
    m_instruction_handlers[0x21] = &cpu::ld_r16_n16<Register16::HL>;
    m_instruction_handlers[0x31] = &cpu::ld_r16_n16<Register16::SP>;
    m_instruction_handlers[0xF8] = &cpu::ld_HL_SP_e8;
    m_instruction_handlers[0xF9] = &cpu::ld_r16_r16<Register16::SP, Register16::HL>;

    // Loads - Special
    m_instruction_handlers[0xE0] = &cpu::ld_a8_r8<Register8::A>;
    m_instruction_handlers[0xF0] = &cpu::ld_r8_a8<Register8::A>;
    m_instruction_handlers[0xE2] = &cpu::ld_r8ind_r8<Register8::C, Register8::A>;  // XXX
    m_instruction_handlers[0xF2] = &cpu::ld_r8_r8ind<Register8::A, Register8::C>;
    m_instruction_handlers[0xEA] = &cpu::ld_a16_r8<Register8::A>;
    m_instruction_handlers[0xFA] = &cpu::ld_r8_a16<Register8::A>;
    m_instruction_handlers[0x08] = &cpu::ld_a16_SP;

    // ALU - 8x
    m_instruction_handlers[0x80] = &cpu::add<Register::B, false>;
    m_instruction_handlers[0x81] = &cpu::add<Register::C, false>;
    m_instruction_handlers[0x82] = &cpu::add<Register::D, false>;
    m_instruction_handlers[0x83] = &cpu::add<Register::E, false>;
    m_instruction_handlers[0x84] = &cpu::add<Register::H, false>;
    m_instruction_handlers[0x85] = &cpu::add<Register::L, false>;
    m_instruction_handlers[0x86] = &cpu::add<Register::HL, false>;
    m_instruction_handlers[0x87] = &cpu::add<Register::A, false>;
    m_instruction_handlers[0x88] = &cpu::add<Register::B, true>;
    m_instruction_handlers[0x89] = &cpu::add<Register::C, true>;
    m_instruction_handlers[0x8A] = &cpu::add<Register::D, true>;
    m_instruction_handlers[0x8B] = &cpu::add<Register::E, true>;
    m_instruction_handlers[0x8C] = &cpu::add<Register::H, true>;
    m_instruction_handlers[0x8D] = &cpu::add<Register::L, true>;
    m_instruction_handlers[0x8E] = &cpu::add<Register::HL, true>;
    m_instruction_handlers[0x8F] = &cpu::add<Register::A, true>;

    // ALU - 9x
    m_instruction_handlers[0x90] = &cpu::sub<Register::B, false>;
    m_instruction_handlers[0x91] = &cpu::sub<Register::C, false>;
    m_instruction_handlers[0x92] = &cpu::sub<Register::D, false>;
    m_instruction_handlers[0x93] = &cpu::sub<Register::E, false>;
    m_instruction_handlers[0x94] = &cpu::sub<Register::H, false>;
    m_instruction_handlers[0x95] = &cpu::sub<Register::L, false>;
    m_instruction_handlers[0x96] = &cpu::sub<Register::HL, false>;
    m_instruction_handlers[0x97] = &cpu::sub<Register::A, false>;
    m_instruction_handlers[0x98] = &cpu::sub<Register::B, true>;
    m_instruction_handlers[0x99] = &cpu::sub<Register::C, true>;
    m_instruction_handlers[0x9A] = &cpu::sub<Register::D, true>;
    m_instruction_handlers[0x9B] = &cpu::sub<Register::E, true>;
    m_instruction_handlers[0x9C] = &cpu::sub<Register::H, true>;
    m_instruction_handlers[0x9D] = &cpu::sub<Register::L, true>;
    m_instruction_handlers[0x9E] = &cpu::sub<Register::HL, true>;
    m_instruction_handlers[0x9F] = &cpu::sub<Register::A, true>;

    // ALU - Ax
    m_instruction_handlers[0xA0] = &cpu::logical_and<Register::B>;
    m_instruction_handlers[0xA1] = &cpu::logical_and<Register::C>;
    m_instruction_handlers[0xA2] = &cpu::logical_and<Register::D>;
    m_instruction_handlers[0xA3] = &cpu::logical_and<Register::E>;
    m_instruction_handlers[0xA4] = &cpu::logical_and<Register::H>;
    m_instruction_handlers[0xA5] = &cpu::logical_and<Register::L>;
    m_instruction_handlers[0xA6] = &cpu::logical_and<Register::HL>;
    m_instruction_handlers[0xA7] = &cpu::logical_and<Register::A>;
    m_instruction_handlers[0xA8] = &cpu::logical_xor<Register::B>;
    m_instruction_handlers[0xA9] = &cpu::logical_xor<Register::C>;
    m_instruction_handlers[0xAA] = &cpu::logical_xor<Register::D>;
    m_instruction_handlers[0xAB] = &cpu::logical_xor<Register::E>;
    m_instruction_handlers[0xAC] = &cpu::logical_xor<Register::H>;
    m_instruction_handlers[0xAD] = &cpu::logical_xor<Register::L>;
    m_instruction_handlers[0xAE] = &cpu::logical_xor<Register::HL>;
    m_instruction_handlers[0xAF] = &cpu::logical_xor<Register::A>;

    // ALU - Bx
    m_instruction_handlers[0xB0] = &cpu::logical_or<Register::B>;
    m_instruction_handlers[0xB1] = &cpu::logical_or<Register::C>;
    m_instruction_handlers[0xB2] = &cpu::logical_or<Register::D>;
    m_instruction_handlers[0xB3] = &cpu::logical_or<Register::E>;
    m_instruction_handlers[0xB4] = &cpu::logical_or<Register::H>;
    m_instruction_handlers[0xB5] = &cpu::logical_or<Register::L>;
    m_instruction_handlers[0xB6] = &cpu::logical_or<Register::HL>;
    m_instruction_handlers[0xB7] = &cpu::logical_or<Register::A>;
    m_instruction_handlers[0xB8] = &cpu::logical_cp<Register::B>;
    m_instruction_handlers[0xB9] = &cpu::logical_cp<Register::C>;
    m_instruction_handlers[0xBA] = &cpu::logical_cp<Register::D>;
    m_instruction_handlers[0xBB] = &cpu::logical_cp<Register::E>;
    m_instruction_handlers[0xBC] = &cpu::logical_cp<Register::H>;
    m_instruction_handlers[0xBD] = &cpu::logical_cp<Register::L>;
    m_instruction_handlers[0xBE] = &cpu::logical_cp<Register::HL>;
    m_instruction_handlers[0xBF] = &cpu::logical_cp<Register::A>;

    // ALU - special
    m_instruction_handlers[0xC6] = &cpu::add_n8<false>;
    m_instruction_handlers[0xD6] = &cpu::sub_n8<false>;
    m_instruction_handlers[0xE6] = &cpu::logical_and_n8;
    m_instruction_handlers[0xF6] = &cpu::logical_or_n8;
    m_instruction_handlers[0xCE] = &cpu::add_n8<true>;
    m_instruction_handlers[0xDE] = &cpu::sub_n8<true>;
    m_instruction_handlers[0xEE] = &cpu::logical_xor_n8;
    m_instruction_handlers[0xFE] = &cpu::logical_cp_n8;
    m_instruction_handlers[0xE8] = &cpu::add_SP_e8;

    // ALU - ADD 16-bit
    m_instruction_handlers[0x09] = &cpu::add<Register16::HL, Register16::BC>;
    m_instruction_handlers[0x19] = &cpu::add<Register16::HL, Register16::DE>;
    m_instruction_handlers[0x29] = &cpu::add<Register16::HL, Register16::HL>;
    m_instruction_handlers[0x39] = &cpu::add<Register16::HL, Register16::SP>;

    // INC + DEC
    m_instruction_handlers[0x03] = &cpu::inc<Register16::BC>;
    m_instruction_handlers[0x13] = &cpu::inc<Register16::DE>;
    m_instruction_handlers[0x23] = &cpu::inc<Register16::HL>;
    m_instruction_handlers[0x33] = &cpu::inc<Register16::SP>;

    m_instruction_handlers[0x04] = &cpu::inc<Register8::B>;
    m_instruction_handlers[0x14] = &cpu::inc<Register8::D>;
    m_instruction_handlers[0x24] = &cpu::inc<Register8::H>;
    m_instruction_handlers[0x34] = &cpu::inc_HLind;

    m_instruction_handlers[0x05] = &cpu::dec<Register8::B>;
    m_instruction_handlers[0x15] = &cpu::dec<Register8::D>;
    m_instruction_handlers[0x25] = &cpu::dec<Register8::H>;
    m_instruction_handlers[0x35] = &cpu::dec_HLind;

    m_instruction_handlers[0x0B] = &cpu::dec<Register16::BC>;
    m_instruction_handlers[0x1B] = &cpu::dec<Register16::DE>;
    m_instruction_handlers[0x2B] = &cpu::dec<Register16::HL>;
    m_instruction_handlers[0x3B] = &cpu::dec<Register16::SP>;

    m_instruction_handlers[0x0C] = &cpu::inc<Register8::C>;
    m_instruction_handlers[0x1C] = &cpu::inc<Register8::E>;
    m_instruction_handlers[0x2C] = &cpu::inc<Register8::L>;
    m_instruction_handlers[0x3C] = &cpu::inc<Register8::A>;

    m_instruction_handlers[0x0D] = &cpu::dec<Register8::C>;
    m_instruction_handlers[0x1D] = &cpu::dec<Register8::E>;
    m_instruction_handlers[0x2D] = &cpu::dec<Register8::L>;
    m_instruction_handlers[0x3D] = &cpu::dec<Register8::A>;

    // Jumps
    m_instruction_handlers[0x20] = &cpu::jr<Condition::NZ>;
    m_instruction_handlers[0x30] = &cpu::jr<Condition::NC>;
    m_instruction_handlers[0x18] = &cpu::jr<Condition::NoCondition>;
    m_instruction_handlers[0x28] = &cpu::jr<Condition::Z>;
    m_instruction_handlers[0x38] = &cpu::jr<Condition::C>;
    m_instruction_handlers[0xC2] = &cpu::jp<Condition::NZ>;
    m_instruction_handlers[0xD2] = &cpu::jp<Condition::NC>;
    m_instruction_handlers[0xC3] = &cpu::jp<Condition::NoCondition>;
    m_instruction_handlers[0xCA] = &cpu::jp<Condition::Z>;
    m_instruction_handlers[0xDA] = &cpu::jp<Condition::C>;
    m_instruction_handlers[0xE9] = &cpu::jp_HL;

    // RSTs
    m_instruction_handlers[0xC7] = &cpu::rst<0x00>;
    m_instruction_handlers[0xD7] = &cpu::rst<0x10>;
    m_instruction_handlers[0xE7] = &cpu::rst<0x20>;
    m_instruction_handlers[0xF7] = &cpu::rst<0x30>;
    m_instruction_handlers[0xCF] = &cpu::rst<0x08>;
    m_instruction_handlers[0xDF] = &cpu::rst<0x18>;
    m_instruction_handlers[0xEF] = &cpu::rst<0x28>;
    m_instruction_handlers[0xFF] = &cpu::rst<0x38>;

    // Calls
    m_instruction_handlers[0xC4] = &cpu::call<Condition::NZ>;
    m_instruction_handlers[0xD4] = &cpu::call<Condition::NC>;
    m_instruction_handlers[0xCC] = &cpu::call<Condition::Z>;
    m_instruction_handlers[0xDC] = &cpu::call<Condition::C>;
    m_instruction_handlers[0xCD] = &cpu::call<Condition::NoCondition>;

    // Returns
    m_instruction_handlers[0xC0] = &cpu::ret<Condition::NZ>;
    m_instruction_handlers[0xD0] = &cpu::ret<Condition::NC>;
    m_instruction_handlers[0xC8] = &cpu::ret<Condition::Z>;
    m_instruction_handlers[0xD8] = &cpu::ret<Condition::C>;
    m_instruction_handlers[0xC9] = &cpu::ret<Condition::NoCondition>;
    m_instruction_handlers[0xD9] = &cpu::ret<Condition::NoCondition, true>;  // RETI

    // Stack
    m_instruction_handlers[0xC1] = &cpu::pop<Register16::BC>;
    m_instruction_handlers[0xD1] = &cpu::pop<Register16::DE>;
    m_instruction_handlers[0xE1] = &cpu::pop<Register16::HL>;
    m_instruction_handlers[0xF1] = &cpu::pop<Register16::AF>;
    m_instruction_handlers[0xC5] = &cpu::push<Register16::BC>;
    m_instruction_handlers[0xD5] = &cpu::push<Register16::DE>;
    m_instruction_handlers[0xE5] = &cpu::push<Register16::HL>;
    m_instruction_handlers[0xF5] = &cpu::push<Register16::AF>;

    // Special
    m_instruction_handlers[0x00] = &cpu::nop;
    m_instruction_handlers[0x10] = &cpu::stop;
    m_instruction_handlers[0x76] = &cpu::halt;
    m_instruction_handlers[0x07] = &cpu::rlca;
    m_instruction_handlers[0x17] = &cpu::rla;
    m_instruction_handlers[0x27] = &cpu::daa;
    m_instruction_handlers[0x37] = &cpu::scf;
    m_instruction_handlers[0x0F] = &cpu::rrca;
    m_instruction_handlers[0x1F] = &cpu::rra;
    m_instruction_handlers[0x2F] = &cpu::cpl;
    m_instruction_handlers[0x3F] = &cpu::ccf;
    m_instruction_handlers[0xF3] = &cpu::di;
    m_instruction_handlers[0xFB] = &cpu::ei;

    // CB - RLC
    u8 i = 0;
    m_instruction_handlers_cb[i++] = &cpu::rlc<Register::B>;
    m_instruction_handlers_cb[i++] = &cpu::rlc<Register::C>;
    m_instruction_handlers_cb[i++] = &cpu::rlc<Register::D>;
    m_instruction_handlers_cb[i++] = &cpu::rlc<Register::E>;
    m_instruction_handlers_cb[i++] = &cpu::rlc<Register::H>;
    m_instruction_handlers_cb[i++] = &cpu::rlc<Register::L>;
    m_instruction_handlers_cb[i++] = &cpu::rlc<Register::HL>;
    m_instruction_handlers_cb[i++] = &cpu::rlc<Register::A>;

    // CB - RRC
    m_instruction_handlers_cb[i++] = &cpu::rrc<Register::B>;
    m_instruction_handlers_cb[i++] = &cpu::rrc<Register::C>;
    m_instruction_handlers_cb[i++] = &cpu::rrc<Register::D>;
    m_instruction_handlers_cb[i++] = &cpu::rrc<Register::E>;
    m_instruction_handlers_cb[i++] = &cpu::rrc<Register::H>;
    m_instruction_handlers_cb[i++] = &cpu::rrc<Register::L>;
    m_instruction_handlers_cb[i++] = &cpu::rrc<Register::HL>;
    m_instruction_handlers_cb[i++] = &cpu::rrc<Register::A>;

    // CB - RL
    m_instruction_handlers_cb[i++] = &cpu::rl<Register::B>;
    m_instruction_handlers_cb[i++] = &cpu::rl<Register::C>;
    m_instruction_handlers_cb[i++] = &cpu::rl<Register::D>;
    m_instruction_handlers_cb[i++] = &cpu::rl<Register::E>;
    m_instruction_handlers_cb[i++] = &cpu::rl<Register::H>;
    m_instruction_handlers_cb[i++] = &cpu::rl<Register::L>;
    m_instruction_handlers_cb[i++] = &cpu::rl<Register::HL>;
    m_instruction_handlers_cb[i++] = &cpu::rl<Register::A>;

    // CB - RR
    m_instruction_handlers_cb[i++] = &cpu::rr<Register::B>;
    m_instruction_handlers_cb[i++] = &cpu::rr<Register::C>;
    m_instruction_handlers_cb[i++] = &cpu::rr<Register::D>;
    m_instruction_handlers_cb[i++] = &cpu::rr<Register::E>;
    m_instruction_handlers_cb[i++] = &cpu::rr<Register::H>;
    m_instruction_handlers_cb[i++] = &cpu::rr<Register::L>;
    m_instruction_handlers_cb[i++] = &cpu::rr<Register::HL>;
    m_instruction_handlers_cb[i++] = &cpu::rr<Register::A>;

    // CB - SLA
    m_instruction_handlers_cb[i++] = &cpu::sla<Register::B>;
    m_instruction_handlers_cb[i++] = &cpu::sla<Register::C>;
    m_instruction_handlers_cb[i++] = &cpu::sla<Register::D>;
    m_instruction_handlers_cb[i++] = &cpu::sla<Register::E>;
    m_instruction_handlers_cb[i++] = &cpu::sla<Register::H>;
    m_instruction_handlers_cb[i++] = &cpu::sla<Register::L>;
    m_instruction_handlers_cb[i++] = &cpu::sla<Register::HL>;
    m_instruction_handlers_cb[i++] = &cpu::sla<Register::A>;

    // CB - SRA
    m_instruction_handlers_cb[i++] = &cpu::sra<Register::B>;
    m_instruction_handlers_cb[i++] = &cpu::sra<Register::C>;
    m_instruction_handlers_cb[i++] = &cpu::sra<Register::D>;
    m_instruction_handlers_cb[i++] = &cpu::sra<Register::E>;
    m_instruction_handlers_cb[i++] = &cpu::sra<Register::H>;
    m_instruction_handlers_cb[i++] = &cpu::sra<Register::L>;
    m_instruction_handlers_cb[i++] = &cpu::sra<Register::HL>;
    m_instruction_handlers_cb[i++] = &cpu::sra<Register::A>;

    // CB - SWAP
    m_instruction_handlers_cb[i++] = &cpu::swap<Register::B>;
    m_instruction_handlers_cb[i++] = &cpu::swap<Register::C>;
    m_instruction_handlers_cb[i++] = &cpu::swap<Register::D>;
    m_instruction_handlers_cb[i++] = &cpu::swap<Register::E>;
    m_instruction_handlers_cb[i++] = &cpu::swap<Register::H>;
    m_instruction_handlers_cb[i++] = &cpu::swap<Register::L>;
    m_instruction_handlers_cb[i++] = &cpu::swap<Register::HL>;
    m_instruction_handlers_cb[i++] = &cpu::swap<Register::A>;

    // CB - SRL
    m_instruction_handlers_cb[i++] = &cpu::srl<Register::B>;
    m_instruction_handlers_cb[i++] = &cpu::srl<Register::C>;
    m_instruction_handlers_cb[i++] = &cpu::srl<Register::D>;
    m_instruction_handlers_cb[i++] = &cpu::srl<Register::E>;
    m_instruction_handlers_cb[i++] = &cpu::srl<Register::H>;
    m_instruction_handlers_cb[i++] = &cpu::srl<Register::L>;
    m_instruction_handlers_cb[i++] = &cpu::srl<Register::HL>;
    m_instruction_handlers_cb[i++] = &cpu::srl<Register::A>;

    // CB - BIT
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::B, 0>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::C, 0>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::D, 0>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::E, 0>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::H, 0>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::L, 0>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::HL, 0>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::A, 0>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::B, 1>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::C, 1>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::D, 1>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::E, 1>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::H, 1>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::L, 1>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::HL, 1>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::A, 1>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::B, 2>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::C, 2>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::D, 2>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::E, 2>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::H, 2>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::L, 2>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::HL, 2>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::A, 2>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::B, 3>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::C, 3>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::D, 3>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::E, 3>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::H, 3>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::L, 3>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::HL, 3>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::A, 3>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::B, 4>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::C, 4>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::D, 4>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::E, 4>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::H, 4>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::L, 4>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::HL, 4>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::A, 4>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::B, 5>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::C, 5>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::D, 5>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::E, 5>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::H, 5>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::L, 5>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::HL, 5>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::A, 5>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::B, 6>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::C, 6>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::D, 6>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::E, 6>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::H, 6>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::L, 6>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::HL, 6>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::A, 6>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::B, 7>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::C, 7>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::D, 7>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::E, 7>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::H, 7>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::L, 7>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::HL, 7>;
    m_instruction_handlers_cb[i++] = &cpu::bit<Register::A, 7>;

    // CB - RES
    m_instruction_handlers_cb[i++] = &cpu::res<Register::B, 0>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::C, 0>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::D, 0>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::E, 0>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::H, 0>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::L, 0>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::HL, 0>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::A, 0>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::B, 1>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::C, 1>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::D, 1>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::E, 1>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::H, 1>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::L, 1>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::HL, 1>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::A, 1>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::B, 2>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::C, 2>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::D, 2>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::E, 2>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::H, 2>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::L, 2>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::HL, 2>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::A, 2>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::B, 3>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::C, 3>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::D, 3>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::E, 3>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::H, 3>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::L, 3>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::HL, 3>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::A, 3>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::B, 4>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::C, 4>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::D, 4>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::E, 4>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::H, 4>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::L, 4>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::HL, 4>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::A, 4>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::B, 5>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::C, 5>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::D, 5>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::E, 5>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::H, 5>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::L, 5>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::HL, 5>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::A, 5>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::B, 6>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::C, 6>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::D, 6>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::E, 6>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::H, 6>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::L, 6>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::HL, 6>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::A, 6>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::B, 7>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::C, 7>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::D, 7>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::E, 7>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::H, 7>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::L, 7>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::HL, 7>;
    m_instruction_handlers_cb[i++] = &cpu::res<Register::A, 7>;

    // CB - SET
    m_instruction_handlers_cb[i++] = &cpu::set<Register::B, 0>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::C, 0>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::D, 0>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::E, 0>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::H, 0>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::L, 0>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::HL, 0>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::A, 0>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::B, 1>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::C, 1>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::D, 1>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::E, 1>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::H, 1>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::L, 1>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::HL, 1>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::A, 1>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::B, 2>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::C, 2>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::D, 2>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::E, 2>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::H, 2>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::L, 2>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::HL, 2>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::A, 2>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::B, 3>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::C, 3>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::D, 3>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::E, 3>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::H, 3>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::L, 3>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::HL, 3>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::A, 3>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::B, 4>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::C, 4>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::D, 4>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::E, 4>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::H, 4>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::L, 4>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::HL, 4>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::A, 4>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::B, 5>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::C, 5>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::D, 5>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::E, 5>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::H, 5>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::L, 5>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::HL, 5>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::A, 5>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::B, 6>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::C, 6>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::D, 6>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::E, 6>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::H, 6>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::L, 6>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::HL, 6>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::A, 6>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::B, 7>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::C, 7>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::D, 7>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::E, 7>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::H, 7>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::L, 7>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::HL, 7>;
    m_instruction_handlers_cb[i++] = &cpu::set<Register::A, 7>;
}

void Cpu::connect(ICycler *cycler, MemoryBus *memory) {
    m_cycler = cycler;
    m_memory = memory;
}

void Cpu::add_cycle() {
    if (m_cycler) m_cycler->add_cycles();
}

bool Cpu::contains(const u16 address) const { return address == 0xFFFF || address == 0xFF0F; }

u8 Cpu::read(const u16 address) const {
    if (address == 0xFF0F) {
        return m_interrupt_request_flags;
    }

    if (address == 0xFFFF) {
        return m_interrupt_enable_flags;
    }

    return 0xFF;
}

void Cpu::write(const u16 address, const u8 value) {
    if (address == 0xFF0F) {
        m_interrupt_request_flags = value;
        return;
    }

    if (address == 0xFFFF) {
        m_interrupt_enable_flags = value;
        return;
    }
}

void Cpu::set_pending_interrupt(const InterruptType type, const bool pending_interrupt) {
    set_bit(m_interrupt_request_flags, static_cast<u8>(type), pending_interrupt);
}

bool Cpu::has_pending_interrupt() const {
    return (m_interrupt_request_flags & m_interrupt_enable_flags & 0b11111) != 0;
}

u8 Cpu::peek_u8() const { return m_memory->peek_u8(m_registers.pc); }

u16 Cpu::peek_u16() const {
    const auto lo = m_memory->peek_u8(m_registers.pc);
    const auto hi = m_memory->peek_u8(m_registers.pc + 1);
    return combine_bytes(hi, lo);
}

u8 Cpu::fetch_u8() { return m_memory->read_u8(m_registers.pc++); }

u16 Cpu::fetch_u16() {
    const auto lo = fetch_u8();
    const auto hi = fetch_u8();
    return combine_bytes(hi, lo);
}

void Cpu::stack_push8(const u8 value) { m_memory->write_u8(--m_registers.sp, value); }

u8 Cpu::stack_pop8() { return m_memory->read_u8(m_registers.sp++); }

void Cpu::stack_push16(const u16 value) {
    auto [hi, lo] = split_bytes(value);
    stack_push8(hi);
    stack_push8(lo);
}

u16 Cpu::stack_pop16() {
    const auto lo = stack_pop8();
    const auto hi = stack_pop8();
    return combine_bytes(hi, lo);
}

bool Cpu::step() {
    if (!m_halted) {
        // Handle interrupts
        if (m_interrupt_master_enable && has_pending_interrupt()) {
            execute_interrupts();
            m_set_interrupt_master_enable_next_cycle = false;
        } else {
            // Normal instruction
            execute_next_instruction();

            // EI (Enable interrupts) delayed.
            // Takes effect after an instruction completes
            if (m_set_interrupt_master_enable_next_cycle) {
                m_interrupt_master_enable = true;
                m_set_interrupt_master_enable_next_cycle = false;
            }
        }
    } else {
        // Halt for 1 cycle
        add_cycle();

        // Exit halt status on any interrupt, even if not handled
        if (has_pending_interrupt()) {
            m_halted = false;
        }
    }

    return true;
}

void Cpu::execute_next_instruction() {
    static u64 last_ticks = 0;

    // Get the original state debugging
    u16 pc = m_registers.pc;

    // Read the next opcode from the program counter
    auto ticks = m_cycler->get_tick_count();
    auto opcode = fetch_u8();

    // spdlog::set_level(spdlog::level::trace);
    spdlog::set_level(spdlog::level::info);
    // spdlog::set_level(spdlog::level::warn);

    std::string prefix;
    if (spdlog::should_log(spdlog::level::trace)) {
        const auto cpu_state_str = fmt::format(
            "{:010d} (+{:>2}) {:04x} [AF={:04x} BC={:04x} DE={:04x} HL={:04x} SP={:04x}, FLAGS={}{}{}{}]", ticks,
            ticks - last_ticks, pc, m_registers.get_u16(Register::AF), m_registers.get_u16(Register::BC),
            m_registers.get_u16(Register::DE), m_registers.get_u16(Register::HL), m_registers.sp,
            m_registers.get_z() ? "Z" : "-", m_registers.get_n() ? "N" : "-", m_registers.get_h() ? "H" : "-",
            m_registers.get_c() ? "C" : "-");

        // Get bytes spanning operation
        const auto &op = opcode == 0xCB ? opcodes_cb[peek_u8()] : opcodes[opcode];
        std::vector<u8> bytes;
        bytes.push_back(opcode);
        for (u8 i = 1; i < op.length; i++) {
            bytes.emplace_back(m_memory->read_u8(m_registers.pc + i));
        }
        const auto bytes_str = fmt::format("{:02x}", fmt::join(bytes, " "));

        prefix = fmt::format("{} ({:<8}) {}", cpu_state_str, bytes_str, op.mnemonic);

        spdlog::trace("{}", prefix);
    }

    last_ticks = ticks;

    if (opcode == 0xCB) {
        opcode = fetch_u8();

        if (m_instruction_handlers_cb[opcode]) {
            m_instruction_handlers_cb[opcode](*this);
        } else {
            throw std::runtime_error(fmt::format("{} Unknown opcode CB {:02x}", prefix, opcode));
        }
    } else {
        if (m_instruction_handlers[opcode]) {
            m_instruction_handlers[opcode](*this);
        } else {
            throw std::runtime_error(fmt::format("{} Unknown opcode {:02x}", prefix, opcode));
        }
    }
}

void Cpu::execute_interrupts() {
    for (u8 bit = 0; bit < 5; ++bit) {
        if (get_bit(m_interrupt_request_flags, bit) && get_bit(m_interrupt_enable_flags, bit)) {
            // Clear the interrupt request flag
            set_bit(m_interrupt_request_flags, bit, false);

            // Disable interrupts immediately
            m_interrupt_master_enable = false;

            // 2 Wait states (NOPs)
            add_cycle();
            add_cycle();

            // Push current stack pointer onto the stack (2 M-cycles)
            stack_push16(m_registers.pc);

            // Call the interrupt handler
            // Loading the PC takes 1 M-cycle
            m_registers.pc = g_interrupt_jump_addresses[bit];
            add_cycle();

            // Only one interrupt can be handled each cycle
            return;
        }
    }
}
