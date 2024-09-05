#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <array>
#include <bemu/gb/cpu.hpp>
#include <bemu/gb/emulator.hpp>
#include <bemu/gb/utils.hpp>
#include <magic_enum.hpp>
#include <source_location>
#include <stdexcept>

using namespace bemu::gb;

static std::array<u16, 5> g_interrupt_jump_addresses = {
    0x40,  // VBlank
    0x48,  // LCD
    0x50,  // Timer
    0x58,  // Serial
    0x60   // Joypad
};

namespace {
struct UnexpectedError : std::exception {
    explicit UnexpectedError(const std::string &reason = "",
                             const std::source_location sl = std::source_location::current()) {
        m_what = fmt::format("Unexpected opcode error in {}: {}", sl.function_name(), reason);
    }

    [[nodiscard]] const char *what() const noexcept override { return m_what.c_str(); }

   private:
    std::string m_what;
};

struct NotImplementedError : std::exception {
    explicit NotImplementedError(const std::string &reason = "",
                                 const std::source_location sl = std::source_location::current()) {
        m_what = fmt::format("Implementation error in {}: {}", sl.function_name(), reason);
    }

    [[nodiscard]] const char *what() const noexcept override { return m_what.c_str(); }

   private:
    std::string m_what;
};

struct UnexpectedAddressError : std::exception {
    explicit UnexpectedAddressError(const u16 address,
                                    const std::source_location sl = std::source_location::current()) {
        m_what = fmt::format("Unexpected address {:04x} in {}", address, sl.function_name());
    }

    [[nodiscard]] const char *what() const noexcept override { return m_what.c_str(); }

   private:
    std::string m_what;
};

bool is_address(const OperandType t) {
    switch (t) {
        case OperandType::Address8:
        case OperandType::Address16:
        case OperandType::RelativeAddress8pc:
        case OperandType::RelativeAddress8sp:
        case OperandType::HLI:
        case OperandType::HLD: return true;
        default: return false;
    }
}

bool is_address(const OperandDescription t) {
    return is_address(t.m_type) || (t.m_type == OperandType::Register && t.m_register_is_indirect);
}

// s32 load_argument(Cpu &cpu, const OperandDescription &op) {
//     switch (op.m_type) {
//         case OperandType::Data8u: return cpu.fetch_u8();
//         case OperandType::Data8s: return static_cast<s8>(cpu.fetch_u8());
//         case OperandType::Address8: return 0xFF + cpu.fetch_u8();
//         case OperandType::Address16: return cpu.fetch_u16();
//         case OperandType::RelativeAddress8pc: return cpu.fetch_u8() + cpu.fetch_u8();
//     }
// }

// bool is_16_bit(const OperandDescription op) {
//     return op.m_type == OperandType::Data16 || (op.m_type == OperandType::Register &&
//     is_16bit(op.m_register.value()));
// }
}  // namespace

// /// Commonly used order of registers by last 3 bits
// /// See https://gbdev.io/gb-opcodes/optables/octal.
// static std::array g_register_order = {Register::B, Register::C, Register::D,  Register::E,
//                                       Register::H, Register::L, Register::HL, Register::A};

// namespace {
// void execute_ld(Cpu &cpu, Register r1, Register r2, PostLoadOperation p1 = PostLoadOperation::Nothing);
// void execute_ld_n8(Cpu &cpu, Register r1);
//
// bool execute_jp(Cpu &cpu, Condition condition, u16 address);
// bool execute_jp_a16(Cpu &cpu, const std::string &debug_prefix, Condition condition);
// bool execute_jp_e8(Cpu &cpu, const std::string &debug_prefix, Condition condition);
//
// bool execute_jp_a16(Cpu &cpu, const std::string &debug_prefix, const Condition condition) {
//     const auto address = cpu.fetch_u16();
//     if (condition == None) {
//         spdlog::trace("{} JP {:04x}", debug_prefix, address));
//     } else {
//         spdlog::trace("{} JP {}, {:04x}", debug_prefix, magic_enum::enum_name(condition), address));
//     }
//
//     return execute_jp(cpu, condition, address);
// }
//
// bool execute_jp_e8(Cpu &cpu, const std::string &debug_prefix, const Condition condition) {
//     const auto rel = static_cast<s8>(cpu.fetch_u8());
//     const auto address = cpu.m_registers.pc + rel;
//     if (condition == None) {
//         spdlog::trace("{} JR {} = JP {:04x}", debug_prefix, rel, address));
//     } else {
//         spdlog::trace("{} JR {}, {} = JP {}, {:04x}", debug_prefix, magic_enum::enum_name(condition), rel,
//                                  magic_enum::enum_name(condition), address));
//     }
//
//     return execute_jp(cpu, condition, address);
// }
//
// bool execute_jp(Cpu &cpu, const Condition condition, const u16 address) {
//     if (cpu.m_registers.check_flags(condition)) {
//         cpu.m_registers.pc = address;
//         cpu.m_emulator.add_cycles();
//     }
//     return true;
// }
//
// bool execute_jp(Cpu &cpu, const std::string &debug_prefix, const u8 opcode) {
//     switch (opcode) {
//         // Absolute
//         case 0xC2: return execute_jp_a16(cpu, debug_prefix, NZ);
//         case 0xC3: return execute_jp_a16(cpu, debug_prefix, None);
//         case 0xCA: return execute_jp_a16(cpu, debug_prefix, Z);
//         case 0xB2: return execute_jp_a16(cpu, debug_prefix, NC);
//         case 0xBA: return execute_jp_a16(cpu, debug_prefix, C);
//
//         // Relative
//         case 0x18: return execute_jp_e8(cpu, debug_prefix, None);
//         case 0x20: return execute_jp_e8(cpu, debug_prefix, NZ);
//         case 0x28: return execute_jp_e8(cpu, debug_prefix, Z);
//         case 0x30: return execute_jp_e8(cpu, debug_prefix, NC);
//         case 0x38: return execute_jp_e8(cpu, debug_prefix, C);
//
//         case 0xE9: {
//             spdlog::trace("{} JP HL", debug_prefix));
//             return execute_jp(cpu, None, cpu.m_registers.get_u16(Register::HL));
//         }
//
//         default: return false;
//     }
// }

// }  // namespace

bool bemu::gb::is_16bit(const OperandDescription t) {
    return t.m_type == OperandType::Data16 || (t.m_type == OperandType::Register && is_16bit(t.m_register.value()));
}

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
        case Register::F: return f;
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
        case Register::F: f = value; break;
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
        case Register::AF: return combine_bytes(a, f);
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
            f = lo;
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

OperandDescription indirect(const Register r) { return {r, true}; }

Cpu::Cpu(Emulator &emulator) : m_emulator(emulator) {
    // m_instruction_handlers - 0x0X
    m_instruction_handlers[0x00] = {&execute_noop};
    m_instruction_handlers[0x01] = {&execute_ld, Register::BC, OperandType::Data16};
    m_instruction_handlers[0x02] = {&execute_ld, indirect(Register::BC), Register::A};
    m_instruction_handlers[0x03] = {&execute_inc, Register::BC};
    m_instruction_handlers[0x04] = {&execute_inc, Register::B};
    m_instruction_handlers[0x05] = {&execute_dec, Register::B};
    m_instruction_handlers[0x06] = {&execute_ld, Register::B, OperandType::Data8u};
    m_instruction_handlers[0x07] = {&execute_rlca};
    m_instruction_handlers[0x08] = {&execute_ld, OperandType::Address16, Register::SP};
    m_instruction_handlers[0x09] = {&execute_add, Register::HL, Register::BC};
    m_instruction_handlers[0x0A] = {&execute_ld, Register::A, indirect(Register::BC)};
    m_instruction_handlers[0x0B] = {&execute_dec, Register::BC};
    m_instruction_handlers[0x0C] = {&execute_inc, Register::C};
    m_instruction_handlers[0x0D] = {&execute_dec, Register::C};
    m_instruction_handlers[0x0E] = {&execute_ld, Register::C, OperandType::Data8u};
    m_instruction_handlers[0x0F] = {&execute_rrca};

    // m_instruction_handlers - 0x1X
    m_instruction_handlers[0x10] = {&execute_stop, OperandType::Data8u};
    m_instruction_handlers[0x11] = {&execute_ld, Register::DE, OperandType::Data16};
    m_instruction_handlers[0x12] = {&execute_ld, indirect(Register::DE), Register::A};
    m_instruction_handlers[0x13] = {&execute_inc, Register::DE};
    m_instruction_handlers[0x14] = {&execute_inc, Register::D};
    m_instruction_handlers[0x15] = {&execute_dec, Register::D};
    m_instruction_handlers[0x16] = {&execute_ld, Register::D, OperandType::Data8u};
    m_instruction_handlers[0x17] = {&execute_rla};
    m_instruction_handlers[0x18] = {&execute_jp, OperandType::RelativeAddress8pc};
    m_instruction_handlers[0x19] = {&execute_add, Register::HL, Register::DE};
    m_instruction_handlers[0x1A] = {&execute_ld, Register::A, indirect(Register::DE)};
    m_instruction_handlers[0x1B] = {&execute_dec, Register::DE};
    m_instruction_handlers[0x1C] = {&execute_inc, Register::E};
    m_instruction_handlers[0x1D] = {&execute_dec, Register::E};
    m_instruction_handlers[0x1E] = {&execute_ld, Register::E, OperandType::Data8u};
    m_instruction_handlers[0x1F] = {&execute_rra};

    // m_instruction_handlers - 0x2X
    m_instruction_handlers[0x20] = {&execute_jp, OperandType::RelativeAddress8pc, OperandType::None, Condition::NZ};
    m_instruction_handlers[0x21] = {&execute_ld, Register::HL, OperandType::Data16};
    m_instruction_handlers[0x22] = {&execute_ld, OperandType::HLI, Register::A};
    m_instruction_handlers[0x23] = {&execute_inc, Register::HL};
    m_instruction_handlers[0x24] = {&execute_inc, Register::H};
    m_instruction_handlers[0x25] = {&execute_dec, Register::H};
    m_instruction_handlers[0x26] = {&execute_ld, Register::H, OperandType::Data8u};
    m_instruction_handlers[0x27] = {&execute_daa};
    m_instruction_handlers[0x28] = {&execute_jp, OperandType::RelativeAddress8pc, OperandType::None, Condition::Z};
    m_instruction_handlers[0x29] = {&execute_add, Register::HL, Register::HL};
    m_instruction_handlers[0x2A] = {&execute_ld, Register::A, OperandType::HLI};
    m_instruction_handlers[0x2B] = {&execute_dec, Register::HL};
    m_instruction_handlers[0x2C] = {&execute_inc, Register::L};
    m_instruction_handlers[0x2D] = {&execute_dec, Register::L};
    m_instruction_handlers[0x2E] = {&execute_ld, Register::L, OperandType::Data8u};
    m_instruction_handlers[0x2F] = {&execute_cpl};

    // m_instruction_handlers - 0x3X
    m_instruction_handlers[0x30] = {&execute_jp, OperandType::RelativeAddress8pc, OperandType::None, Condition::NC};
    m_instruction_handlers[0x31] = {&execute_ld, Register::SP, OperandType::Data16};
    m_instruction_handlers[0x32] = {&execute_ld, OperandType::HLD, Register::A};
    m_instruction_handlers[0x33] = {&execute_inc, Register::SP};
    m_instruction_handlers[0x34] = {&execute_inc, indirect(Register::HL)};
    m_instruction_handlers[0x35] = {&execute_dec, indirect(Register::HL)};
    m_instruction_handlers[0x36] = {&execute_ld, indirect(Register::HL), OperandType::Data8u};
    m_instruction_handlers[0x37] = {&execute_scf};
    m_instruction_handlers[0x38] = {&execute_jp, OperandType::RelativeAddress8pc, OperandType::None, Condition::C};
    m_instruction_handlers[0x39] = {&execute_add, Register::HL, Register::SP};
    m_instruction_handlers[0x3A] = {&execute_ld, Register::A, OperandType::HLD};
    m_instruction_handlers[0x3B] = {&execute_dec, Register::SP};
    m_instruction_handlers[0x3C] = {&execute_inc, Register::A};
    m_instruction_handlers[0x3D] = {&execute_dec, Register::A};
    m_instruction_handlers[0x3E] = {&execute_ld, Register::A, OperandType::Data8u};
    m_instruction_handlers[0x3F] = {&execute_ccf};

    // m_instruction_handlers - 0x4X
    m_instruction_handlers[0x40] = {&execute_ld, Register::B, Register::B};
    m_instruction_handlers[0x41] = {&execute_ld, Register::B, Register::C};
    m_instruction_handlers[0x42] = {&execute_ld, Register::B, Register::D};
    m_instruction_handlers[0x43] = {&execute_ld, Register::B, Register::E};
    m_instruction_handlers[0x44] = {&execute_ld, Register::B, Register::H};
    m_instruction_handlers[0x45] = {&execute_ld, Register::B, Register::L};
    m_instruction_handlers[0x46] = {&execute_ld, Register::B, indirect(Register::HL)};
    m_instruction_handlers[0x47] = {&execute_ld, Register::B, Register::A};
    m_instruction_handlers[0x48] = {&execute_ld, Register::C, Register::B};
    m_instruction_handlers[0x49] = {&execute_ld, Register::C, Register::C};
    m_instruction_handlers[0x4A] = {&execute_ld, Register::C, Register::D};
    m_instruction_handlers[0x4B] = {&execute_ld, Register::C, Register::E};
    m_instruction_handlers[0x4C] = {&execute_ld, Register::C, Register::H};
    m_instruction_handlers[0x4D] = {&execute_ld, Register::C, Register::L};
    m_instruction_handlers[0x4E] = {&execute_ld, Register::C, indirect(Register::HL)};
    m_instruction_handlers[0x4F] = {&execute_ld, Register::C, Register::A};

    // m_instruction_handlers - 0x5X
    m_instruction_handlers[0x50] = {&execute_ld, Register::D, Register::B};
    m_instruction_handlers[0x51] = {&execute_ld, Register::D, Register::C};
    m_instruction_handlers[0x52] = {&execute_ld, Register::D, Register::D};
    m_instruction_handlers[0x53] = {&execute_ld, Register::D, Register::E};
    m_instruction_handlers[0x54] = {&execute_ld, Register::D, Register::H};
    m_instruction_handlers[0x55] = {&execute_ld, Register::D, Register::L};
    m_instruction_handlers[0x56] = {&execute_ld, Register::D, indirect(Register::HL)};
    m_instruction_handlers[0x57] = {&execute_ld, Register::D, Register::A};
    m_instruction_handlers[0x58] = {&execute_ld, Register::E, Register::B};
    m_instruction_handlers[0x59] = {&execute_ld, Register::E, Register::C};
    m_instruction_handlers[0x5A] = {&execute_ld, Register::E, Register::D};
    m_instruction_handlers[0x5B] = {&execute_ld, Register::E, Register::E};
    m_instruction_handlers[0x5C] = {&execute_ld, Register::E, Register::H};
    m_instruction_handlers[0x5D] = {&execute_ld, Register::E, Register::L};
    m_instruction_handlers[0x5E] = {&execute_ld, Register::E, indirect(Register::HL)};
    m_instruction_handlers[0x5F] = {&execute_ld, Register::E, Register::A};

    // m_instruction_handlers - 0x6X
    m_instruction_handlers[0x60] = {&execute_ld, Register::H, Register::B};
    m_instruction_handlers[0x61] = {&execute_ld, Register::H, Register::C};
    m_instruction_handlers[0x62] = {&execute_ld, Register::H, Register::D};
    m_instruction_handlers[0x63] = {&execute_ld, Register::H, Register::E};
    m_instruction_handlers[0x64] = {&execute_ld, Register::H, Register::H};
    m_instruction_handlers[0x65] = {&execute_ld, Register::H, Register::L};
    m_instruction_handlers[0x66] = {&execute_ld, Register::H, indirect(Register::HL)};
    m_instruction_handlers[0x67] = {&execute_ld, Register::H, Register::A};
    m_instruction_handlers[0x68] = {&execute_ld, Register::L, Register::B};
    m_instruction_handlers[0x69] = {&execute_ld, Register::L, Register::C};
    m_instruction_handlers[0x6A] = {&execute_ld, Register::L, Register::D};
    m_instruction_handlers[0x6B] = {&execute_ld, Register::L, Register::E};
    m_instruction_handlers[0x6C] = {&execute_ld, Register::L, Register::H};
    m_instruction_handlers[0x6D] = {&execute_ld, Register::L, Register::L};
    m_instruction_handlers[0x6E] = {&execute_ld, Register::L, indirect(Register::HL)};
    m_instruction_handlers[0x6F] = {&execute_ld, Register::L, Register::A};

    // m_instruction_handlers - 0x7X
    m_instruction_handlers[0x70] = {&execute_ld, indirect(Register::HL), Register::B};
    m_instruction_handlers[0x71] = {&execute_ld, indirect(Register::HL), Register::C};
    m_instruction_handlers[0x72] = {&execute_ld, indirect(Register::HL), Register::D};
    m_instruction_handlers[0x73] = {&execute_ld, indirect(Register::HL), Register::E};
    m_instruction_handlers[0x74] = {&execute_ld, indirect(Register::HL), Register::H};
    m_instruction_handlers[0x75] = {&execute_ld, indirect(Register::HL), Register::L};
    m_instruction_handlers[0x76] = {&execute_halt};
    m_instruction_handlers[0x77] = {&execute_ld, indirect(Register::HL), Register::A};
    m_instruction_handlers[0x78] = {&execute_ld, Register::A, Register::B};
    m_instruction_handlers[0x79] = {&execute_ld, Register::A, Register::C};
    m_instruction_handlers[0x7A] = {&execute_ld, Register::A, Register::D};
    m_instruction_handlers[0x7B] = {&execute_ld, Register::A, Register::E};
    m_instruction_handlers[0x7C] = {&execute_ld, Register::A, Register::H};
    m_instruction_handlers[0x7D] = {&execute_ld, Register::A, Register::L};
    m_instruction_handlers[0x7E] = {&execute_ld, Register::A, indirect(Register::HL)};
    m_instruction_handlers[0x7F] = {&execute_ld, Register::A, Register::A};

    // m_instruction_handlers - 0x8X
    m_instruction_handlers[0x80] = {&execute_add, Register::A, Register::B};
    m_instruction_handlers[0x81] = {&execute_add, Register::A, Register::C};
    m_instruction_handlers[0x82] = {&execute_add, Register::A, Register::D};
    m_instruction_handlers[0x83] = {&execute_add, Register::A, Register::E};
    m_instruction_handlers[0x84] = {&execute_add, Register::A, Register::H};
    m_instruction_handlers[0x85] = {&execute_add, Register::A, Register::L};
    m_instruction_handlers[0x86] = {&execute_add, Register::A, indirect(Register::HL)};
    m_instruction_handlers[0x87] = {&execute_add, Register::A, Register::A};
    m_instruction_handlers[0x88] = {&execute_adc, Register::B};
    m_instruction_handlers[0x89] = {&execute_adc, Register::C};
    m_instruction_handlers[0x8A] = {&execute_adc, Register::D};
    m_instruction_handlers[0x8B] = {&execute_adc, Register::E};
    m_instruction_handlers[0x8C] = {&execute_adc, Register::H};
    m_instruction_handlers[0x8D] = {&execute_adc, Register::L};
    m_instruction_handlers[0x8E] = {&execute_adc, indirect(Register::HL)};
    m_instruction_handlers[0x8F] = {&execute_adc, Register::A};

    // m_instruction_handlers - 0x9X
    m_instruction_handlers[0x90] = {&execute_sub, Register::B};
    m_instruction_handlers[0x91] = {&execute_sub, Register::C};
    m_instruction_handlers[0x92] = {&execute_sub, Register::D};
    m_instruction_handlers[0x93] = {&execute_sub, Register::E};
    m_instruction_handlers[0x94] = {&execute_sub, Register::H};
    m_instruction_handlers[0x95] = {&execute_sub, Register::L};
    m_instruction_handlers[0x96] = {&execute_sub, indirect(Register::HL)};
    m_instruction_handlers[0x97] = {&execute_sub, Register::A};
    m_instruction_handlers[0x98] = {&execute_sbc, Register::B};
    m_instruction_handlers[0x99] = {&execute_sbc, Register::C};
    m_instruction_handlers[0x9A] = {&execute_sbc, Register::D};
    m_instruction_handlers[0x9B] = {&execute_sbc, Register::E};
    m_instruction_handlers[0x9C] = {&execute_sbc, Register::H};
    m_instruction_handlers[0x9D] = {&execute_sbc, Register::L};
    m_instruction_handlers[0x9E] = {&execute_sbc, indirect(Register::HL)};
    m_instruction_handlers[0x9F] = {&execute_sbc, Register::A};

    // m_instruction_handlers - 0xAX
    m_instruction_handlers[0xA0] = {&execute_and, Register::B};
    m_instruction_handlers[0xA1] = {&execute_and, Register::C};
    m_instruction_handlers[0xA2] = {&execute_and, Register::D};
    m_instruction_handlers[0xA3] = {&execute_and, Register::E};
    m_instruction_handlers[0xA4] = {&execute_and, Register::H};
    m_instruction_handlers[0xA5] = {&execute_and, Register::L};
    m_instruction_handlers[0xA6] = {&execute_and, indirect(Register::HL)};
    m_instruction_handlers[0xA7] = {&execute_and, Register::A};
    m_instruction_handlers[0xA8] = {&execute_xor, Register::B};
    m_instruction_handlers[0xA9] = {&execute_xor, Register::C};
    m_instruction_handlers[0xAA] = {&execute_xor, Register::D};
    m_instruction_handlers[0xAB] = {&execute_xor, Register::E};
    m_instruction_handlers[0xAC] = {&execute_xor, Register::H};
    m_instruction_handlers[0xAD] = {&execute_xor, Register::L};
    m_instruction_handlers[0xAE] = {&execute_xor, indirect(Register::HL)};
    m_instruction_handlers[0xAF] = {&execute_xor, Register::A};

    // m_instruction_handlers - 0xBX
    m_instruction_handlers[0xB0] = {&execute_or, Register::B};
    m_instruction_handlers[0xB1] = {&execute_or, Register::C};
    m_instruction_handlers[0xB2] = {&execute_or, Register::D};
    m_instruction_handlers[0xB3] = {&execute_or, Register::E};
    m_instruction_handlers[0xB4] = {&execute_or, Register::H};
    m_instruction_handlers[0xB5] = {&execute_or, Register::L};
    m_instruction_handlers[0xB6] = {&execute_or, indirect(Register::HL)};
    m_instruction_handlers[0xB7] = {&execute_or, Register::A};
    m_instruction_handlers[0xB8] = {&execute_cp, Register::B};
    m_instruction_handlers[0xB9] = {&execute_cp, Register::C};
    m_instruction_handlers[0xBA] = {&execute_cp, Register::D};
    m_instruction_handlers[0xBB] = {&execute_cp, Register::E};
    m_instruction_handlers[0xBC] = {&execute_cp, Register::H};
    m_instruction_handlers[0xBD] = {&execute_cp, Register::L};
    m_instruction_handlers[0xBE] = {&execute_cp, indirect(Register::HL)};
    m_instruction_handlers[0xBF] = {&execute_cp, Register::A};

    // m_instruction_handlers - 0xCX
    m_instruction_handlers[0xC0] = {&execute_ret, OperandType::None, OperandType::None, Condition::NZ};
    m_instruction_handlers[0xC1] = {&execute_pop, Register::BC};
    m_instruction_handlers[0xC2] = {&execute_jp, OperandType::Address16, OperandType::None, Condition::NZ};
    m_instruction_handlers[0xC3] = {&execute_jp, OperandType::Address16};
    m_instruction_handlers[0xC4] = {&execute_call, OperandType::Address16, OperandType::None, Condition::NZ};
    m_instruction_handlers[0xC5] = {&execute_push, Register::BC};
    m_instruction_handlers[0xC6] = {&execute_add, Register::A, OperandType::Data8u};
    m_instruction_handlers[0xC7] = {&execute_rst, OperandType::None, OperandType::None, Condition::None, 0x00};
    m_instruction_handlers[0xC8] = {&execute_ret, OperandType::None, OperandType::None, Condition::Z};
    m_instruction_handlers[0xC9] = {&execute_ret};
    m_instruction_handlers[0xCA] = {&execute_jp, OperandType::Address16, OperandType::None, Condition::Z};
    m_instruction_handlers[0xCC] = {&execute_call, OperandType::Address16, OperandType::None, Condition::Z};
    m_instruction_handlers[0xCD] = {&execute_call, OperandType::Address16};
    m_instruction_handlers[0xCE] = {&execute_adc, OperandType::Data8u};
    m_instruction_handlers[0xCF] = {&execute_rst, OperandType::None, OperandType::None, Condition::None, 0x08};

    // m_instruction_handlers - 0xDX
    m_instruction_handlers[0xD0] = {&execute_ret, OperandType::None, OperandType::None, Condition::NC};
    m_instruction_handlers[0xD1] = {&execute_pop, Register::DE};
    m_instruction_handlers[0xD2] = {&execute_jp, OperandType::Address16, OperandType::None, Condition::NC};
    m_instruction_handlers[0xD4] = {&execute_call, OperandType::Address16, OperandType::None, Condition::NC};
    m_instruction_handlers[0xD5] = {&execute_push, Register::DE};
    m_instruction_handlers[0xD6] = {&execute_sub, OperandType::Data8u};
    m_instruction_handlers[0xD7] = {&execute_rst, OperandType::None, OperandType::None, Condition::None, 0x10};
    m_instruction_handlers[0xD8] = {&execute_ret, OperandType::None, OperandType::None, Condition::C};
    m_instruction_handlers[0xD9] = {&execute_reti};
    m_instruction_handlers[0xDA] = {&execute_jp, OperandType::Address16, OperandType::None, Condition::C};
    m_instruction_handlers[0xDC] = {&execute_call, OperandType::Address16, OperandType::None, Condition::C};
    m_instruction_handlers[0xDE] = {&execute_sbc, OperandType::Data8u};
    m_instruction_handlers[0xDF] = {&execute_rst, OperandType::None, OperandType::None, Condition::None, 0x18};

    // m_instruction_handlers - 0xEX
    m_instruction_handlers[0xE0] = {&execute_ld, OperandType::Address8, Register::A};
    m_instruction_handlers[0xE1] = {&execute_pop, Register::HL};
    m_instruction_handlers[0xE2] = {&execute_ld, indirect(Register::C), Register::A};
    m_instruction_handlers[0xE5] = {&execute_push, Register::HL};
    m_instruction_handlers[0xE6] = {&execute_and, OperandType::Data8u};
    m_instruction_handlers[0xE7] = {&execute_rst, OperandType::None, OperandType::None, Condition::None, 0x20};
    m_instruction_handlers[0xE8] = {&execute_add, Register::SP, OperandType::Data8u};
    m_instruction_handlers[0xE9] = {&execute_jp, Register::HL};
    m_instruction_handlers[0xEA] = {&execute_ld, OperandType::Address16, Register::A};
    m_instruction_handlers[0xEE] = {&execute_xor, OperandType::Data8u};
    m_instruction_handlers[0xEF] = {&execute_rst, OperandType::None, OperandType::None, Condition::None, 0x28};

    // m_instruction_handlers - 0xFX
    m_instruction_handlers[0xF0] = {&execute_ld, Register::A, OperandType::Address8};
    m_instruction_handlers[0xF1] = {&execute_pop, Register::AF};
    m_instruction_handlers[0xF2] = {&execute_ld, Register::A, indirect(Register::C)};
    m_instruction_handlers[0xF3] = {&execute_di};
    m_instruction_handlers[0xF5] = {&execute_push, Register::AF};
    m_instruction_handlers[0xF6] = {&execute_or, OperandType::Data8u};
    m_instruction_handlers[0xF7] = {&execute_rst, OperandType::None, OperandType::None, Condition::None, 0x30};
    m_instruction_handlers[0xF8] = {&execute_ld, Register::HL, OperandType::RelativeAddress8sp};
    m_instruction_handlers[0xF9] = {&execute_ld, Register::SP, Register::HL};
    m_instruction_handlers[0xFA] = {&execute_ld, Register::A, OperandType::Address16};
    m_instruction_handlers[0xFB] = {&execute_ei};
    m_instruction_handlers[0xFE] = {&execute_cp, OperandType::Data8u};
    m_instruction_handlers[0xFF] = {&execute_rst, OperandType::None, OperandType::None, Condition::None, 0x38};

    // m_instruction_handlers_cb - 0x1X
    auto add_octet = [&](const u8 start, const instruction_function_t function, const u8 param = 0) {
        m_instruction_handlers_cb[start + 0] = {function, Register::B, OperandType::None, Condition::None, param};
        m_instruction_handlers_cb[start + 1] = {function, Register::C, OperandType::None, Condition::None, param};
        m_instruction_handlers_cb[start + 2] = {function, Register::D, OperandType::None, Condition::None, param};
        m_instruction_handlers_cb[start + 3] = {function, Register::E, OperandType::None, Condition::None, param};
        m_instruction_handlers_cb[start + 4] = {function, Register::H, OperandType::None, Condition::None, param};
        m_instruction_handlers_cb[start + 5] = {function, Register::L, OperandType::None, Condition::None, param};
        m_instruction_handlers_cb[start + 6] = {function, indirect(Register::HL), OperandType::None, Condition::None,
                                                param};
        m_instruction_handlers_cb[start + 7] = {function, Register::A, OperandType::None, Condition::None, param};
    };
    add_octet(0000, &execute_rlc);
    add_octet(0010, &execute_rrc);
    add_octet(0020, &execute_rl);
    add_octet(0030, &execute_rr);
    add_octet(0040, &execute_sla);
    add_octet(0050, &execute_sra);
    add_octet(0060, &execute_swap);
    add_octet(0070, &execute_srl);
    add_octet(0100, &execute_bit, 0);
    add_octet(0110, &execute_bit, 1);
    add_octet(0120, &execute_bit, 2);
    add_octet(0130, &execute_bit, 3);
    add_octet(0140, &execute_bit, 4);
    add_octet(0150, &execute_bit, 5);
    add_octet(0160, &execute_bit, 6);
    add_octet(0170, &execute_bit, 7);
    add_octet(0200, &execute_res, 0);
    add_octet(0210, &execute_res, 1);
    add_octet(0220, &execute_res, 2);
    add_octet(0230, &execute_res, 3);
    add_octet(0240, &execute_res, 4);
    add_octet(0250, &execute_res, 5);
    add_octet(0260, &execute_res, 6);
    add_octet(0270, &execute_res, 7);
    add_octet(0300, &execute_set, 0);
    add_octet(0310, &execute_set, 1);
    add_octet(0320, &execute_set, 2);
    add_octet(0330, &execute_set, 3);
    add_octet(0340, &execute_set, 4);
    add_octet(0350, &execute_set, 5);
    add_octet(0360, &execute_set, 6);
    add_octet(0370, &execute_set, 7);
}

bool Cpu::contains(const u16 address) const { return address == 0xFFFF || address == 0xFF0F; }

u8 Cpu::read_memory(const u16 address) const {
    if (address == 0xFF0F) {
        return m_interrupt_request_flags;
    }

    if (address == 0xFFFF) {
        return m_interrupt_enable_flags;
    }

    throw UnexpectedAddressError(address);
}

void Cpu::write_memory(const u16 address, const u8 value) {
    if (address == 0xFF0F) {
        m_interrupt_request_flags = value;
        return;
    }

    if (address == 0xFFFF) {
        m_interrupt_enable_flags = value;
        return;
    }

    throw UnexpectedAddressError(address);
}

void Cpu::set_pending_interrupt(const InterruptType type, const bool pending_interrupt) {
    set_bit(m_interrupt_request_flags, type, pending_interrupt);
}

bool Cpu::has_pending_interrupt() const { return m_interrupt_request_flags & 0b11111 > 0; }

u8 Cpu::peek_u8() const { return m_emulator.m_bus.read_u8(m_registers.pc, false); }

u16 Cpu::peek_u16() const {
    const auto lo = m_emulator.m_bus.read_u8(m_registers.pc, false);
    const auto hi = m_emulator.m_bus.read_u8(m_registers.pc + 1, false);
    return combine_bytes(hi, lo);
}

u8 Cpu::fetch_u8() { return m_emulator.m_bus.read_u8(m_registers.pc++); }

u16 Cpu::fetch_u16() {
    const auto lo = fetch_u8();
    const auto hi = fetch_u8();
    return combine_bytes(hi, lo);
}

u8 Cpu::read_data_u8(const Register reg, const bool add_cycles) {
    if (is_16bit(reg)) {
        const auto address = m_registers.get_u16(reg);
        return m_emulator.m_bus.read_u8(address, add_cycles);
    }
    return m_registers.get_u8(reg);
}

void Cpu::write_data_u8(const Register reg, u8 value, const bool add_cycles) {
    if (is_16bit(reg)) {
        const auto address = m_registers.get_u16(reg);
        m_emulator.m_bus.write_u8(address, value, add_cycles);
        return;
    }
    return m_registers.set_u8(reg, value);
}

void Cpu::stack_push8(const u8 value) { m_emulator.m_bus.write_u8(--m_registers.sp, value); }

u8 Cpu::stack_pop8() { return m_emulator.m_bus.read_u8(m_registers.sp++); }

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
        execute_next_instruction();
    } else {
        // Halt for 1 cycle
        m_emulator.add_cycles();

        // Exit halt status once an interrupt is set
        // If we don't have interrupts enabled at this point... we're stuck... so don't handle that case
        if (has_pending_interrupt()) {
            m_halted = false;
            spdlog::info("Unhalted");
        }
    }

    // Handle interrupts
    if (m_interrupt_master_enable) {
        execute_interrupts();
        m_set_interrupt_master_enable_next_cycle = false;
    }

    // Enable interrupts 1 cycle delayed
    if (m_set_interrupt_master_enable_next_cycle) {
        m_interrupt_master_enable = true;
    }

    return true;
}

void Cpu::execute_next_instruction() {
    static u64 last_ticks = 0;  // FIXME

    // Get the original state debugging
    u16 pc = m_registers.pc;

    // Read the next opcode from the program counter
    auto ticks = m_emulator.m_ticks;
    auto opcode = fetch_u8();

    std::string prefix =
        /*!spdlog::should_log(spdlog::level::trace)
            ? ""
            :*/ fmt::format(
                  "{:08d} (+{:>2})    {:04x}    [A={:02x} F={:02x} B={:02x} C={:02x} D={:02x} E={:02x} H={:02x} "
                  "L={:02x} "
                  "SP={:04x}]    [{}{}{}{}]    ({:02x} {:02x} {:02x})   ",
                  ticks, ticks - last_ticks, pc, m_registers.a, m_registers.f, m_registers.b, m_registers.c,
                  m_registers.d, m_registers.e, m_registers.h, m_registers.l, m_registers.sp,
                  m_registers.get_z() ? "Z" : "-", m_registers.get_n() ? "N" : "-", m_registers.get_h() ? "H" : "-",
                  m_registers.get_c() ? "C" : "-", opcode, m_emulator.m_bus.read_u8(m_registers.pc, false),
                  m_emulator.m_bus.read_u8(m_registers.pc + 1, false));
    last_ticks = ticks;
    spdlog::trace("{}", prefix);

    // This is where the error happens
    if (pc == 0x021c) {
        spdlog::trace("Tracepoint: {}", prefix);
    }

    if (pc == 0x021f) {
        spdlog::trace("Tracepoint: {}", prefix);
    }

    if (pc == 0x0293) {
        spdlog::trace("Tracepoint: {}", prefix);
    }

    if (pc == 0x0296) {
        spdlog::trace("Tracepoint: {}", prefix);

        // Render
        {
            constexpr size_t nx = 16;
            constexpr size_t ny = 8 * 3;
            RenderTarget<8 * nx, 8 * ny> render;
            for (size_t x = 0; x < nx; ++x) {
                for (size_t y = 0; y < ny; ++y) {
                    render.render_tile(m_emulator.m_bus, 0x8000 + x * 0x10 + y * 0x100, 8 * x, 8 * y);
                }
            }
            for (u8 i = 0; i < render.screen_height; ++i) {
                std::string s;
                for (u8 c = 0; c < render.screen_width; ++c) {
                    const auto b = render.m_pixels[i][c];
                    if (b == 0) {
                        s += "  ";
                    } else if (b == 1) {
                        s += "--";
                    } else if (b == 2) {
                        s += "xx";
                    } else if (b == 3) {
                        s += "##";
                    }
                }

                spdlog::trace(s);
            }
        }
    }

    if (pc == 0x47f2) {
        spdlog::trace("Tracepoint: {}", prefix);
    }

    const auto *instruction_set = &m_instruction_handlers;

    if (opcode == 0xCB) {
        opcode = fetch_u8();

        const auto &instruction = m_instruction_handlers_cb[opcode];
        if (instruction.m_handler == nullptr) {
            throw std::runtime_error(fmt::format("{} Unknown opcode CB {:02x}", prefix, opcode));
        }
        (this->*instruction.m_handler)(prefix, instruction);
    } else {
        const auto &instruction = m_instruction_handlers[opcode];
        if (instruction.m_handler == nullptr) {
            throw std::runtime_error(fmt::format("{} Unknown opcode {:02x}", prefix, opcode));
        }
        (this->*instruction.m_handler)(prefix, instruction);
    }
}

void Cpu::execute_interrupts() {
    for (u8 bit = 0; bit < 8; ++bit) {
        if (get_bit(m_interrupt_request_flags, bit)) {
            // Clear the interrupt request flag
            set_bit(m_interrupt_request_flags, bit, false);

            // Disable interrupts immediately
            m_interrupt_master_enable = false;

            if (m_halted)
                spdlog::info("Unhalted");
            m_halted = false;  // TODO: Remove?
            // spdlog::info("Unhalted");

            // Call the interrupt handler
            stack_push16(m_registers.pc);
            m_registers.pc = g_interrupt_jump_addresses[bit];

            // Only one interrupt can be handled each cycle
            return;
        }
    }
}

void Cpu::execute_noop(const std::string &debug_prefix, const CpuInstruction &) {
    spdlog::trace("{} NOP", debug_prefix);
}

void Cpu::execute_stop(const std::string &debug_prefix, const CpuInstruction &) {
    const auto data = fetch_u8();
    spdlog::trace("{} STOP {:02x}", debug_prefix, data);
    throw std::runtime_error("Stopped");
}

void Cpu::execute_halt(const std::string &debug_prefix, const CpuInstruction &) {
    spdlog::trace("{} HALT", debug_prefix);
    m_halted = true;
}

void Cpu::execute_ld(const std::string &dbg, const CpuInstruction &instruction) {
    spdlog::trace("{} LD {}, {}", dbg, to_string(instruction.m_op1), to_string(instruction.m_op2));

    if (is_address(instruction.m_op1)) {
        // Load into address
        if (is_16bit(instruction.m_op2)) {
            const auto address = decode_address(instruction.m_op1);
            const auto value = decode_u16(instruction.m_op2);
            m_emulator.m_bus.write_u16(address, value);
        } else {
            const auto address = decode_address(instruction.m_op1);
            const auto value = decode_u8(instruction.m_op2);
            m_emulator.m_bus.write_u8(address, value);
        }
    } else if (is_16bit(instruction.m_op1.m_register.value())) {
        // Load into 16-bit register
        const auto value = decode_u16(instruction.m_op2);
        m_registers.set_u16(instruction.m_op1.m_register.value(), value);
    } else {
        // Load into 8-bit register
        const auto value = decode_u8(instruction.m_op2);
        m_registers.set_u8(instruction.m_op1.m_register.value(), value);
    }
}

void Cpu::execute_inc(const std::string &dbg, const CpuInstruction &instruction) {
    spdlog::trace("{} INC {}", dbg, to_string(instruction.m_op1));
    const auto reg = instruction.m_op1.m_register.value();

    if (is_16bit(instruction.m_op1.m_register.value())) {
        if (!instruction.m_op1.m_register_is_indirect) {
            // 16-bit direct increment, e.g. INC DE. Two cycles.
            const u16 old_value = decode_u16(instruction.m_op1);
            const u16 new_value = old_value + 1;
            m_registers.set_u16(reg, new_value);

            // 16-bit operations take 1 extra cycle and don't modify flags
            m_emulator.add_cycles(1);
        } else {
            // 16-bit indirect increment, e.g. INC [HL]
            const u16 address = m_registers.get_u16(reg);
            const u8 old_value = m_emulator.m_bus.read_u16(address);
            const int new_value_int = static_cast<int>(old_value) + 1;
            const u8 new_value = new_value_int & 0xFF;

            m_emulator.m_bus.write_u8(address, new_value);

            m_registers.set_z(new_value == 0);
            m_registers.set_n(false);
            m_registers.set_h(new_value_int & 0xF == 0);
        }
    } else {
        // 8-bit direct increment, e.g. INC B. One cycle.
        const u8 old_value = m_registers.get_u8(reg);
        const int new_value_int = static_cast<int>(old_value) + 1;
        const u8 new_value = new_value_int & 0xFF;

        m_registers.set_u8(reg, new_value);

        m_registers.set_z(new_value == 0);
        m_registers.set_n(false);
        m_registers.set_h(new_value_int & 0xF == 0);
    }
}

void Cpu::execute_dec(const std::string &dbg, const CpuInstruction &instruction) {
    spdlog::trace("{} DEC {}", dbg, to_string(instruction.m_op1));
    const auto reg = instruction.m_op1.m_register.value();

    if (is_16bit(instruction.m_op1.m_register.value())) {
        if (!instruction.m_op1.m_register_is_indirect) {
            // 16-bit direct increment, e.g. DEC DE. Two cycles.
            const u16 old_value = decode_u16(instruction.m_op1);
            const u16 new_value = old_value - 1;
            m_registers.set_u16(reg, new_value);

            // 16-bit operations take 1 extra cycle and don't modify flags
            m_emulator.add_cycles(1);
        } else {
            // 16-bit indirect increment, e.g. DEC [HL]
            const u16 address = m_registers.get_u16(reg);
            const u8 old_value = m_emulator.m_bus.read_u16(address);
            const int new_value_int = static_cast<int>(old_value) - 1;
            const u8 new_value = new_value_int & 0xFF;

            m_emulator.m_bus.write_u8(address, new_value);

            // TODO: Check cycles. Expected +3, but I think it may be +4. inc/dec on [HL] is atomic?

            m_registers.set_z(new_value == 0);
            m_registers.set_n(true);
            m_registers.set_h(new_value_int & 0xF == 0);
        }
    } else {
        // 8-bit direct increment, e.g. DEC B. One cycle.
        const u8 old_value = m_registers.get_u8(reg);
        const int new_value_int = static_cast<int>(old_value) - 1;
        const u8 new_value = new_value_int & 0xFF;

        m_registers.set_u8(reg, new_value);

        m_registers.set_z(new_value == 0);
        m_registers.set_n(true);
        m_registers.set_h((new_value_int & 0xF) == 0xF);
    }
}

void Cpu::execute_add(const std::string &dbg, const CpuInstruction &instruction) {
    spdlog::trace("{} ADD {}, {}", dbg, to_string(instruction.m_op1), to_string(instruction.m_op2));

    // op1 is always a register
    const auto reg1 = instruction.m_op1.m_register.value();

    m_registers.set_n(false);

    if (reg1 == Register::A) {
        // 8-bit add to A
        const int old_value = m_registers.get_u8(reg1);
        const int added_value = decode_u8(instruction.m_op2);

        const int new_value = old_value + added_value;
        const int new_value_half = (old_value & 0xF) + (added_value & 0xF);

        const u8 new_value_u8 = static_cast<u8>(new_value);

        m_registers.a = new_value_u8;
        m_registers.set_flags(new_value_u8 == 0, false, new_value_half > 0xF, new_value > 0xFF);
    } else if (reg1 == Register::SP) {
        // Special case
        const auto old_value = m_registers.sp;
        const auto added_value = static_cast<s8>(fetch_u8());
        const auto new_value_int = static_cast<int>(old_value) + added_value;
        const auto new_value = static_cast<u8>(new_value_int & 0xFF);

        m_registers.sp = new_value;

        m_registers.set_z(false);
        m_registers.set_h((old_value & 0xF) + (added_value & 0xF) > 0xF);
        m_registers.set_c((old_value & 0xFF) + (added_value & 0xFF) > 0xFF);
    } else {
        // 16-bit add
        const auto old_value = m_registers.get_u16(reg1);
        const auto added_value = decode_u16(instruction.m_op2);

        const auto new_value_int = static_cast<int>(old_value) + added_value;
        const auto new_value = static_cast<u16>(new_value_int & 0xFFFF);

        m_registers.set_u16(reg1, new_value);

        m_registers.set_h((old_value & 0xF) + (added_value & 0xF) > 0xF);
        m_registers.set_c((old_value & 0xFF) + (added_value & 0xFF) > 0xFF);
    }
}

void Cpu::execute_adc(const std::string &dbg, const CpuInstruction &instruction) {
    spdlog::trace("{} ADC A, {}", dbg, to_string(instruction.m_op1));

    m_registers.set_n(false);

    // 8-bit add to A, with carry
    const int old_value = m_registers.a;
    const int added_value = decode_u8(instruction.m_op1);
    const u8 old_c = m_registers.get_c() ? 1 : 0;

    const int new_value = old_value + added_value + old_c;
    const int new_value_half = (old_value & 0xF) + (added_value & 0xF) + old_c;
    const u8 new_value_u8 = static_cast<u8>(new_value);

    m_registers.a = new_value_u8;
    m_registers.set_flags(new_value_u8 == 0, false, new_value_half > 0xF, new_value > 0xFF);
}

void Cpu::execute_sub(const std::string &dbg, const CpuInstruction &instruction) {
    spdlog::trace("{} SUB A, {}", dbg, to_string(instruction.m_op1));

    const int old_value = m_registers.a;
    const int subtracted_value = decode_u8(instruction.m_op1);

    const int new_value = old_value - subtracted_value;
    const int new_value_half = (old_value & 0xF) - (subtracted_value & 0xF);
    const u8 new_value_u8 = static_cast<u8>(new_value);

    m_registers.a = new_value_u8;
    m_registers.set_flags(new_value_u8 == 0, true, new_value_half < 0, new_value < 0);
}

void Cpu::execute_sbc(const std::string &dbg, const CpuInstruction &instruction) {
    spdlog::trace("{} SBC A, {}", dbg, to_string(instruction.m_op1));

    const auto c = m_registers.get_c() ? 1 : 0;
    const int old_value = m_registers.a;
    const int subtracted_value = decode_u8(instruction.m_op1);

    const int new_value = old_value - subtracted_value - c;
    const int new_value_half = (old_value & 0xF) - (subtracted_value & 0xF) - c;
    const u8 new_value_u8 = static_cast<u8>(new_value);

    m_registers.a = new_value_u8;
    m_registers.set_flags(new_value_u8 == 0, true, new_value_half < 0, new_value < 0);
}

void Cpu::execute_cp(const std::string &dbg, const CpuInstruction &instruction) {
    spdlog::trace("{} CP A, {}", dbg, to_string(instruction.m_op1));

    const auto a = static_cast<int>(m_registers.a);
    const auto r = static_cast<int>(decode_u8(instruction.m_op1));
    const auto result = a - r;
    const auto half_result = (a & 0x0F) - (r & 0x0F);

    m_registers.set_z(result == 0);
    m_registers.set_n(true);
    m_registers.set_h(half_result < 0);
    m_registers.set_c(result < 0);
}

void Cpu::execute_and(const std::string &dbg, const CpuInstruction &instruction) {
    spdlog::trace("{} AND A, {}", dbg, to_string(instruction.m_op1));

    m_registers.a &= decode_u8(instruction.m_op1);
    m_registers.set_z(m_registers.a == 0);
    m_registers.set_n(false);
    m_registers.set_h(true);
    m_registers.set_c(false);
}

void Cpu::execute_or(const std::string &dbg, const CpuInstruction &instruction) {
    spdlog::trace("{} OR A, {}", dbg, to_string(instruction.m_op1));

    const auto data = decode_u8(instruction.m_op1);
    m_registers.a |= data;
    m_registers.set_z(m_registers.a == 0);
    m_registers.set_n(false);
    m_registers.set_h(false);
    m_registers.set_c(false);
}

void Cpu::execute_xor(const std::string &dbg, const CpuInstruction &instruction) {
    spdlog::trace("{} XOR A, {}", dbg, to_string(instruction.m_op1));

    const auto data = decode_u8(instruction.m_op1);
    m_registers.a ^= data;
    m_registers.set_z(m_registers.a == 0);
    m_registers.set_n(false);
    m_registers.set_h(false);
    m_registers.set_c(false);
}

void Cpu::execute_jp(const std::string &dbg, const CpuInstruction &instruction) {
    auto jump_type_str = instruction.m_op1.m_type == OperandType::RelativeAddress8pc ? "JR" : "JP";
    if (instruction.m_condition != Condition::None) {
        spdlog::trace("{} {} {}, {}", dbg, jump_type_str, magic_enum::enum_name(instruction.m_condition),
                     to_string(instruction.m_op1));
    } else {
        spdlog::trace("{} {} {}", dbg, jump_type_str, to_string(instruction.m_op1));
    }

    const auto address = decode_address(instruction.m_op1);
    if (m_registers.check_flags(instruction.m_condition)) {
        m_registers.pc = address;
        m_emulator.add_cycles();
    }
}

void Cpu::execute_call(const std::string &dbg, const CpuInstruction &instruction) {
    if (instruction.m_condition == Condition::None) {
        spdlog::trace("{} CALL {}", dbg, to_string(instruction.m_op1));
    } else {
        spdlog::trace("{} CALL {}, {}", dbg, magic_enum::enum_name(instruction.m_condition),
                     to_string(instruction.m_op1));
    }

    const auto address = decode_address(instruction.m_op1);
    if (m_registers.check_flags(instruction.m_condition)) {
        stack_push16(m_registers.pc);
        m_registers.pc = address;
        m_emulator.add_cycles();
    }
}

void Cpu::execute_rst(const std::string &dbg, const CpuInstruction &instruction) {
    spdlog::trace("{} RST {:02x}", dbg, instruction.m_parameter);

    stack_push16(m_registers.pc);
    const auto address = instruction.m_parameter;
    m_registers.pc = address;
    m_emulator.add_cycles();
}

void Cpu::execute_ret(const std::string &dbg, const CpuInstruction &instruction) {
    if (instruction.m_condition != Condition::None) {
        spdlog::trace("{} RET {}", dbg, magic_enum::enum_name(instruction.m_condition));
    } else {
        spdlog::trace("{} RET", dbg);
    }

    if (m_registers.check_flags(instruction.m_condition)) {
        m_registers.pc = stack_pop16();
        m_emulator.add_cycles();
    }
}

void Cpu::execute_reti(const std::string &dbg, const CpuInstruction &) {
    spdlog::trace("{} RETI", dbg);
    m_interrupt_master_enable = true;
    m_registers.pc = stack_pop16();
    m_emulator.add_cycles();
}

void Cpu::execute_sra(const std::string &dbg, const CpuInstruction &instruction) {
    const auto reg = instruction.m_op1.m_register.value();
    spdlog::trace("{} SRA {}", dbg, to_string(instruction.m_op1));

    const u8 input = read_register_u8(reg);
    s8 value_i = static_cast<s8>(input);
    value_i >>= 1;
    const u8 value = static_cast<u8>(value_i);

    write_register_u8(reg, value);
    m_registers.set_flags(value == 0, false, false, get_bit(input, 0));
}

void Cpu::execute_srl(const std::string &dbg, const CpuInstruction &instruction) {
    const auto reg = instruction.m_op1.m_register.value();
    spdlog::trace("{} SRL {}", dbg, to_string(instruction.m_op1));

    auto value = read_register_u8(reg);
    const bool shifted_bit = get_bit(value, 0);
    value >>= 1;

    write_register_u8(reg, value);
    m_registers.set_flags(value == 0, false, false, shifted_bit);
}

void Cpu::execute_rrc(const std::string &dbg, const CpuInstruction &instruction) {
    const auto reg = instruction.m_op1.m_register.value();
    spdlog::trace("{} RRC {}", dbg, to_string(instruction.m_op1));

    auto value = read_register_u8(reg);
    const bool rotated_bit = get_bit(value, 0);
    value >>= 1;
    set_bit(value, 7, rotated_bit);

    write_register_u8(reg, value);
    m_registers.set_flags(value == 0, false, false, rotated_bit);
}

void Cpu::execute_rrca(const std::string &dbg, const CpuInstruction &) {
    spdlog::trace("{} RRCA", dbg);

    auto value = m_registers.a;
    const bool rotated_bit = get_bit(value, 0);
    value >>= 1;
    set_bit(value, 7, rotated_bit);

    m_registers.a = value;
    m_registers.set_flags(false, false, false, rotated_bit);
}

void Cpu::execute_rr(const std::string &dbg, const CpuInstruction &instruction) {
    const auto reg = instruction.m_op1.m_register.value();
    spdlog::trace("{} RR", dbg, to_string(instruction.m_op1));

    auto value = read_register_u8(reg);
    const bool rotated_bit = get_bit(value, 0);
    value >>= 1;
    set_bit(value, 7, m_registers.get_c());

    write_register_u8(instruction.m_op1.m_register.value(), value);
    m_registers.set_flags(value == 0, false, false, rotated_bit);
}

void Cpu::execute_rra(const std::string &dbg, const CpuInstruction &) {
    spdlog::trace("{} RRA", dbg);

    auto value = m_registers.a;
    const bool rotated_bit = get_bit(value, 0);
    value >>= 1;
    set_bit(value, 7, m_registers.get_c());

    m_registers.a = value;
    m_registers.set_flags(false, false, false, rotated_bit);
}

void Cpu::execute_sla(const std::string &dbg, const CpuInstruction &instruction) {
    const auto reg = instruction.m_op1.m_register.value();
    spdlog::trace("{} SLA {}", dbg, to_string(instruction.m_op1));

    auto value = read_register_u8(reg);
    const bool shifted_bit = get_bit(value, 7);
    value <<= 1;

    write_register_u8(reg, value);
    m_registers.set_flags(value == 0, false, false, shifted_bit);
}

void Cpu::execute_rlc(const std::string &dbg, const CpuInstruction &instruction) {
    const auto reg = instruction.m_op1.m_register.value();
    spdlog::trace("{} RLC", dbg, to_string(instruction.m_op1));

    auto value = read_register_u8(reg);
    const bool rotated_bit = get_bit(value, 7);
    value <<= 1;
    set_bit(value, 0, rotated_bit);

    write_register_u8(reg, value);
    m_registers.set_flags(value == 0, false, false, rotated_bit);
}

void Cpu::execute_rlca(const std::string &dbg, const CpuInstruction &) {
    spdlog::trace("{} RLCA", dbg);

    auto value = m_registers.a;
    const bool rotated_bit = get_bit(value, 7);
    value <<= 1;
    set_bit(value, 0, rotated_bit);

    m_registers.a = value;
    m_registers.set_flags(false, false, false, rotated_bit);
}

void Cpu::execute_rl(const std::string &dbg, const CpuInstruction &instruction) {
    const auto reg = instruction.m_op1.m_register.value();
    spdlog::trace("{} RL {}", dbg, to_string(instruction.m_op1));

    auto value = read_register_u8(reg);
    const bool rotated_bit = get_bit(value, 7);
    value <<= 1;
    set_bit(value, 0, m_registers.get_z());

    write_register_u8(reg, value);
    m_registers.set_flags(value == 0, false, false, rotated_bit);
}

void Cpu::execute_rla(const std::string &dbg, const CpuInstruction &) {
    spdlog::trace("{} RLA", dbg);

    auto value = m_registers.a;
    const bool rotated_bit = get_bit(value, 7);
    value <<= 1;
    set_bit(value, 0, m_registers.get_z());

    m_registers.a = value;
    m_registers.set_flags(false, false, false, rotated_bit);
}

void Cpu::execute_push(const std::string &dbg, const CpuInstruction &instruction) {
    spdlog::trace("{} PUSH {}", dbg, to_string(instruction.m_op1));
    stack_push16(decode_u16(instruction.m_op1));
}

void Cpu::execute_pop(const std::string &dbg, const CpuInstruction &instruction) {
    spdlog::trace("{} POP {}", dbg, to_string(instruction.m_op1));
    const auto data = stack_pop16();
    m_registers.set_u16(instruction.m_op1.m_register.value(), data);
}

void Cpu::execute_scf(const std::string &dbg, const CpuInstruction &) {
    spdlog::trace("{} SCF", dbg);
    m_registers.set_n(false);
    m_registers.set_h(false);
    m_registers.set_c(true);
}

void Cpu::execute_ccf(const std::string &dbg, const CpuInstruction &) {
    spdlog::trace("{} CCF", dbg);
    m_registers.set_n(false);
    m_registers.set_h(false);
    m_registers.set_c(!m_registers.get_c());
}

void Cpu::execute_cpl(const std::string &dbg, const CpuInstruction &) {
    spdlog::trace("{} CPL", dbg);
    m_registers.a = ~m_registers.a;
    m_registers.set_n(true);
}

void Cpu::execute_daa(const std::string &dbg, const CpuInstruction &) {
    spdlog::trace("{} DAA", dbg);

    if (m_registers.get_n()) {
        // Last instruction was a subtraction
        if (m_registers.get_c()) {
            m_registers.a -= 0x60;
        }
        if (m_registers.get_h()) {
            m_registers.a -= 0x6;
        }
    } else {
        // Last instruction was an addition
        if (m_registers.get_c() || m_registers.a > 0x99) {
            m_registers.a += 0x60;
            m_registers.set_c(true);
        }

        if (m_registers.get_h() || (m_registers.a & 0xF) > 0x9) {
            m_registers.a += 0x6;
        }
    }

    m_registers.set_z(m_registers.a == 0);
    m_registers.set_h(false);
}

void Cpu::execute_bit(const std::string &dbg, const CpuInstruction &instruction) {
    spdlog::trace("{} BIT {}, {}", dbg, instruction.m_parameter, to_string(instruction.m_op1));
    const auto value = decode_u8(instruction.m_op1);
    write_register_u8(instruction.m_op1.m_register.value(), value);
    m_registers.set_z(get_bit(value, instruction.m_parameter));
    m_registers.set_n(false);
    m_registers.set_h(true);
}

void Cpu::execute_set(const std::string &dbg, const CpuInstruction &instruction) {
    spdlog::trace("{} SET {}, {}", dbg, instruction.m_parameter, to_string(instruction.m_op1));
    auto value = decode_u8(instruction.m_op1);
    set_bit(value, instruction.m_parameter);
    write_register_u8(instruction.m_op1.m_register.value(), value);
}

void Cpu::execute_res(const std::string &dbg, const CpuInstruction &instruction) {
    spdlog::trace("{} RES {}, {}", dbg, instruction.m_parameter, to_string(instruction.m_op1));
    auto value = decode_u8(instruction.m_op1);
    set_bit(value, instruction.m_parameter, false);
    write_register_u8(instruction.m_op1.m_register.value(), value);
}

void Cpu::execute_swap(const std::string &dbg, const CpuInstruction &instruction) {
    const auto reg = instruction.m_op1.m_register.value();
    spdlog::trace("{} SWAP {}", dbg, to_string(instruction.m_op1));

    auto value = read_register_u8(reg);
    value = (value >> 4) | (value << 4);

    write_register_u8(reg, value);
    m_registers.set_flags(value == 0, false, false, false);
}

void Cpu::execute_di(const std::string &dbg, const CpuInstruction &) {
    spdlog::trace("{} DI", dbg);
    m_interrupt_master_enable = false;
}

void Cpu::execute_ei(const std::string &dbg, const CpuInstruction &) {
    spdlog::trace("{} EI", dbg);
    m_set_interrupt_master_enable_next_cycle = true;
}

std::string Cpu::to_string(const OperandDescription &op) const {
    if (op.m_type == OperandType::Register) {
        if (op.m_register_is_indirect) {
            return fmt::format("[{}]", magic_enum::enum_name(op.m_register.value()));
        }
        return std::string{magic_enum::enum_name(op.m_register.value())};
    }

    switch (op.m_type) {
        case OperandType::None: return "-";
        case OperandType::Data8u: return fmt::format("{:02x}", peek_u8());
        case OperandType::Data8s: return fmt::format("{:+d}", static_cast<s8>(peek_u8()));
        case OperandType::Data16: return fmt::format("{:04x}", peek_u16());
        case OperandType::Address8: return fmt::format("({:04x})", 0xFF00 + peek_u8());
        case OperandType::Address16: return fmt::format("({:04x})", peek_u16());
        case OperandType::RelativeAddress8pc: return fmt::format("(IP{:+d})", static_cast<s8>(peek_u8()));
        case OperandType::RelativeAddress8sp: return fmt::format("(SP{:+d})", static_cast<s8>(peek_u8()));
        case OperandType::HLI: return "[HL+]";
        case OperandType::HLD: return "[HL-]";
        default: throw NotImplementedError();
    }
}

u16 Cpu::decode_address(const OperandDescription &op) {
    switch (op.m_type) {
        case OperandType::Register: {
            const auto reg = op.m_register.value();
            if (is_16bit(op.m_register.value())) {
                return m_registers.get_u16(reg);
            }

            // e.g. LD [C], A
            return 0xFF00 + m_registers.get_u8(reg);
        }
        case OperandType::Address8: return 0xFF00 + fetch_u8();
        case OperandType::Address16: return fetch_u16();
        case OperandType::RelativeAddress8pc: {
            const auto offset = static_cast<s8>(fetch_u8());
            return m_registers.pc + offset;
        }
        case OperandType::RelativeAddress8sp: {
            const auto offset = static_cast<s8>(fetch_u8());
            return m_registers.sp + offset;
        }
        case OperandType::HLI: {
            const auto hl = m_registers.get_u16(Register::HL);
            m_registers.set_u16(Register::HL, hl + 1);
            return hl;
        }
        case OperandType::HLD: {
            const auto hl = m_registers.get_u16(Register::HL);
            m_registers.set_u16(Register::HL, hl - 1);
            return hl;
        }
        default: throw NotImplementedError();
    }
}

u8 Cpu::decode_u8(const OperandDescription &op) {
    if (is_address(op)) {
        const auto address = decode_address(op);
        return m_emulator.m_bus.read_u8(address);
    }

    switch (op.m_type) {
        case OperandType::Register: return m_registers.get_u8(op.m_register.value());
        case OperandType::Data8u: return fetch_u8();
        case OperandType::HLI: {
            const auto hl = m_registers.get_u16(Register::HL);
            const auto data = m_emulator.m_bus.read_u8(hl);
            m_registers.set_u16(Register::HL, hl + 1);
            return data;
        }
        case OperandType::HLD: {
            const auto hl = m_registers.get_u16(Register::HL);
            const auto data = m_emulator.m_bus.read_u8(hl);
            m_registers.set_u16(Register::HL, hl - 1);
            return data;
        }
        default: throw NotImplementedError();
    }
}

u16 Cpu::decode_u16(const OperandDescription &op) {
    if (is_address(op)) {
        const auto address = decode_address(op);
        return m_emulator.m_bus.read_u16(address);
    }

    switch (op.m_type) {
        case OperandType::Register: return m_registers.get_u16(op.m_register.value());
        case OperandType::Data16: return fetch_u16();
        default: throw UnexpectedError();
    }
}

void Cpu::write_register_u8(const Register reg, const u8 value) {
    if (reg == Register::HL) {
        const auto address = m_registers.get_u16(Register::HL);
        m_emulator.m_bus.write_u8(address, value);
    } else {
        m_registers.set_u8(reg, value);
    }
}

u8 Cpu::read_register_u8(const Register reg) const {
    if (reg == Register::HL) {
        const auto address = m_registers.get_u16(Register::HL);
        return m_emulator.m_bus.read_u8(address);
    }

    return m_registers.get_u8(reg);
}
