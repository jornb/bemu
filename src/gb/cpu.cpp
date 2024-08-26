#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <bemu/gb/cpu.hpp>
#include <bemu/gb/emulator.hpp>
#include <bemu/gb/utils.hpp>
#include <magic_enum.hpp>
#include <stdexcept>

using namespace bemu::gb;

bool CpuRegisters::get_z() const { return get_bit(f, 7); }
bool CpuRegisters::get_n() const { return get_bit(f, 6); }
bool CpuRegisters::get_h() const { return get_bit(f, 5); }
bool CpuRegisters::get_c() const { return get_bit(f, 4); }
void CpuRegisters::set_z(const bool z) { set_bit(f, 7, z); }
void CpuRegisters::set_n(const bool n) { set_bit(f, 6, n); }
void CpuRegisters::set_h(const bool h) { set_bit(f, 5, h); }
void CpuRegisters::set_c(const bool c) { set_bit(f, 4, c); }

bool CpuRegisters::check_flags(const Condition condition) const {
    switch (condition) {
        case Z: return get_z();
        case NZ: return !get_z();
        case C: return get_c();
        case NC: return !get_c();
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

u8 Cpu::fetch_u8() { return m_emulator.m_bus.read_u8(m_registers.pc++); }

u16 Cpu::fetch_u16() {
    const auto lo = fetch_u8();
    const auto hi = fetch_u8();
    return combine_bytes(hi, lo);
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
    }

    return true;
}

void Cpu::execute_next_instruction() {
    static u64 last_ticks = 0;  // FIXME

    // Get the original state debugging
    u16 pc = m_registers.pc;

    // Read the next opcode from the program counter
    auto ticks = m_emulator.m_ticks;
    const auto opcode = m_emulator.m_bus.read_u8(m_registers.pc++);

    auto prefix = fmt::format(
        "{:08d} (+{:>2})    {:04x}    [A={:02x} F={:02x} B={:02x} C={:02x} D={:02x} E={:02x} H={:02x} L={:02x} "
        "SP={:04x}]    [{}{}{}{}]    ({:02x} {:02x} {:02x})   ",
        ticks, ticks - last_ticks, pc, m_registers.a, m_registers.f, m_registers.b, m_registers.c, m_registers.d,
        m_registers.e, m_registers.h, m_registers.l, m_registers.sp, m_registers.get_z() ? "Z" : "-",
        m_registers.get_n() ? "N" : "-", m_registers.get_h() ? "H" : "-", m_registers.get_c() ? "C" : "-", opcode,
        m_emulator.m_bus.read_u8(m_registers.pc, false), m_emulator.m_bus.read_u8(m_registers.pc + 1, false));
    last_ticks = ticks;
    // spdlog::info(fmt::format("{}", prefix));

    switch (opcode) {
        case 0x00: {
            spdlog::info(fmt::format("{} NOOP", prefix));
            return;
        }

        case 0xF3: {
            spdlog::info(fmt::format("{} DI", prefix));
            m_int_master_enabled = false;
            return;
        }
        default:
            if (execute_ld(prefix, opcode) || execute_xor(prefix, opcode) || execute_cp(prefix, opcode) ||
                execute_and(prefix, opcode) || execute_sub(prefix, opcode) || execute_jp(prefix, opcode) ||
                execute_call(prefix, opcode) || execute_inc(prefix, opcode) || execute_dec(prefix, opcode))
                return;
    }
    throw std::runtime_error(fmt::format("{} Unknown opcode {:02x}", prefix, opcode));
}

bool Cpu::execute_ld(const std::string &debug_prefix, u8 opcode) {
    switch (opcode) {
        // LD R, d8
        case 0x06: return execute_ld_d(debug_prefix, Register::B);
        case 0x0E: return execute_ld_d(debug_prefix, Register::C);
        case 0x16: return execute_ld_d(debug_prefix, Register::D);
        case 0x1E: return execute_ld_d(debug_prefix, Register::E);
        case 0x26: return execute_ld_d(debug_prefix, Register::H);
        case 0x2E: return execute_ld_d(debug_prefix, Register::L);
        case 0x3E: return execute_ld_d(debug_prefix, Register::A);

        // LD R, d16
        case 0x01: return execute_ld_d(debug_prefix, Register::BC);
        case 0x11: return execute_ld_d(debug_prefix, Register::DE);
        case 0x21: return execute_ld_d(debug_prefix, Register::HL);
        case 0x31: return execute_ld_d(debug_prefix, Register::SP);

        // LD R, R
        case 0x40: return execute_ld_r8_r8(debug_prefix, Register::B, Register::B);
        case 0x41: return execute_ld_r8_r8(debug_prefix, Register::B, Register::C);
        case 0x42: return execute_ld_r8_r8(debug_prefix, Register::B, Register::D);
        case 0x43: return execute_ld_r8_r8(debug_prefix, Register::B, Register::E);
        case 0x44: return execute_ld_r8_r8(debug_prefix, Register::B, Register::H);
        case 0x45: return execute_ld_r8_r8(debug_prefix, Register::B, Register::L);
        case 0x47: return execute_ld_r8_r8(debug_prefix, Register::B, Register::A);

        case 0x48: return execute_ld_r8_r8(debug_prefix, Register::C, Register::B);
        case 0x49: return execute_ld_r8_r8(debug_prefix, Register::C, Register::C);
        case 0x4A: return execute_ld_r8_r8(debug_prefix, Register::C, Register::D);
        case 0x4B: return execute_ld_r8_r8(debug_prefix, Register::C, Register::E);
        case 0x4C: return execute_ld_r8_r8(debug_prefix, Register::C, Register::H);
        case 0x4D: return execute_ld_r8_r8(debug_prefix, Register::C, Register::L);
        case 0x4F: return execute_ld_r8_r8(debug_prefix, Register::C, Register::A);

        case 0x50: return execute_ld_r8_r8(debug_prefix, Register::D, Register::B);
        case 0x51: return execute_ld_r8_r8(debug_prefix, Register::D, Register::C);
        case 0x52: return execute_ld_r8_r8(debug_prefix, Register::D, Register::D);
        case 0x53: return execute_ld_r8_r8(debug_prefix, Register::D, Register::E);
        case 0x54: return execute_ld_r8_r8(debug_prefix, Register::D, Register::H);
        case 0x55: return execute_ld_r8_r8(debug_prefix, Register::D, Register::L);
        case 0x57: return execute_ld_r8_r8(debug_prefix, Register::D, Register::A);

        case 0x58: return execute_ld_r8_r8(debug_prefix, Register::E, Register::B);
        case 0x59: return execute_ld_r8_r8(debug_prefix, Register::E, Register::C);
        case 0x5A: return execute_ld_r8_r8(debug_prefix, Register::E, Register::D);
        case 0x5B: return execute_ld_r8_r8(debug_prefix, Register::E, Register::E);
        case 0x5C: return execute_ld_r8_r8(debug_prefix, Register::E, Register::H);
        case 0x5D: return execute_ld_r8_r8(debug_prefix, Register::E, Register::L);
        case 0x5F: return execute_ld_r8_r8(debug_prefix, Register::E, Register::A);

        case 0x60: return execute_ld_r8_r8(debug_prefix, Register::H, Register::B);
        case 0x61: return execute_ld_r8_r8(debug_prefix, Register::H, Register::C);
        case 0x62: return execute_ld_r8_r8(debug_prefix, Register::H, Register::D);
        case 0x63: return execute_ld_r8_r8(debug_prefix, Register::H, Register::E);
        case 0x64: return execute_ld_r8_r8(debug_prefix, Register::H, Register::H);
        case 0x65: return execute_ld_r8_r8(debug_prefix, Register::H, Register::L);
        case 0x67: return execute_ld_r8_r8(debug_prefix, Register::H, Register::A);

        case 0x68: return execute_ld_r8_r8(debug_prefix, Register::L, Register::B);
        case 0x69: return execute_ld_r8_r8(debug_prefix, Register::L, Register::C);
        case 0x6A: return execute_ld_r8_r8(debug_prefix, Register::L, Register::D);
        case 0x6B: return execute_ld_r8_r8(debug_prefix, Register::L, Register::E);
        case 0x6C: return execute_ld_r8_r8(debug_prefix, Register::L, Register::H);
        case 0x6D: return execute_ld_r8_r8(debug_prefix, Register::L, Register::L);
        case 0x6F: return execute_ld_r8_r8(debug_prefix, Register::L, Register::A);

        case 0x78: return execute_ld_r8_r8(debug_prefix, Register::A, Register::B);
        case 0x79: return execute_ld_r8_r8(debug_prefix, Register::A, Register::C);
        case 0x7A: return execute_ld_r8_r8(debug_prefix, Register::A, Register::D);
        case 0x7B: return execute_ld_r8_r8(debug_prefix, Register::A, Register::E);
        case 0x7C: return execute_ld_r8_r8(debug_prefix, Register::A, Register::H);
        case 0x7D: return execute_ld_r8_r8(debug_prefix, Register::A, Register::L);
        case 0x7F: return execute_ld_r8_r8(debug_prefix, Register::A, Register::A);

        // LD (HL+), A
        case 0x22: {
            // Load to the absolute address specified by the 16-bit register HL, data from the 8-bit A register. The
            // value of HL is incremented after the memory write.
            //
            // Cycles: 2
            // Flags: - - - -
            spdlog::info(fmt::format("{} LD (HL+), A ", debug_prefix));
            const auto HL = m_registers.get_HL();
            m_emulator.m_bus.write_u8(HL, m_registers.a);
            m_registers.set_HL(HL + 1);
            return true;
        }

        // LD (HL-), A
        case 0x32: {
            // Load to the absolute address specified by the 16-bit register HL, data from the 8-bit A register. The
            // value of HL is decremented after the memory write.
            //
            // Cycles: 2
            // Flags: - - - -
            spdlog::info(fmt::format("{} LD (HL-), A ", debug_prefix));
            const auto HL = m_registers.get_HL();
            m_emulator.m_bus.write_u8(HL, m_registers.a);
            m_registers.set_HL(HL - 1);
            return true;
        }

        case 0xE0: {  // LDH (a8),A = LD (0xFF00 + a8),A
            const auto a8 = fetch_u8();
            const auto address = 0xFF00 + a8;
            spdlog::info(fmt::format("{} LDH ({:02x}), A = LD ({:04x}), A", debug_prefix, a8, address));
            execute_ld(address, Register::A);
            return true;
        }

        case 0xEA: {  // LD (a16),A
            const auto address = fetch_u16();
            spdlog::info(fmt::format("{} LDH ({:04x}), A", debug_prefix, address));
            execute_ld(address, Register::A);
            return true;
        }

        case 0xF0: {  // LDH A,(a8) = LD A,(0xFF00 + a8)
            const auto a8 = fetch_u8();
            const auto address = 0xFF00 + a8;
            spdlog::info(fmt::format("{} LDH A, ({:02x}) = LD A, ({:04x})", debug_prefix, a8, address));
            execute_ld(Register::A, address);
            return true;
        }

        case 0xFA: {  // LD A,(a16)
            const auto address = fetch_u16();
            spdlog::info(fmt::format("{} LD A, ({:04x})", debug_prefix, address));
            execute_ld(Register::A, address);
            return true;
        }

        default: return false;
    }
}

bool Cpu::execute_ld_d(const std::string &debug_prefix, const Register reg) {
    if (is_16bit(reg)) {
        const auto d16 = fetch_u16();
        spdlog::info(fmt::format("{} LD {}, {:04x}", debug_prefix, magic_enum::enum_name(reg), d16));
        m_registers.set_u16(reg, d16);

    } else {
        const auto d8 = fetch_u8();
        spdlog::info(fmt::format("{} LD {}, {:02x}", debug_prefix, magic_enum::enum_name(reg), d8));
        m_registers.set_u8(reg, d8);
    }

    return true;
}

void Cpu::execute_ld(const u16 address, const Register reg) {
    if (is_16bit(reg)) {
        m_emulator.m_bus.write_u16(address, m_registers.get_u16(reg));
    } else {
        m_emulator.m_bus.write_u8(address, m_registers.get_u8(reg));
    }
}

void Cpu::execute_ld(const Register reg, const u16 address) {
    if (is_16bit(reg)) {
        const auto data = m_emulator.m_bus.read_u16(address);
        m_registers.set_u16(reg, data);
    } else {
        const auto data = m_emulator.m_bus.read_u8(address);
        m_registers.set_u8(reg, data);
    }
}

bool Cpu::execute_ld_r8_r8(const std::string &debug_prefix, const Register reg1, const Register reg2) {
    spdlog::info(fmt::format("{} LD {}, {}", debug_prefix, magic_enum::enum_name(reg1), magic_enum::enum_name(reg2)));
    m_registers.set_u8(reg1, m_registers.get_u8(reg2));
    return true;
}

bool Cpu::execute_inc(const std::string &debug_prefix, u8 opcode) {
    switch (opcode) {
        // INC R
        case 0x04: return execute_inc_r8(debug_prefix, Register::B);
        case 0x14: return execute_inc_r8(debug_prefix, Register::C);
        case 0x24: return execute_inc_r8(debug_prefix, Register::H);
        case 0x0C: return execute_inc_r8(debug_prefix, Register::C);
        case 0x1C: return execute_inc_r8(debug_prefix, Register::E);
        case 0x2C: return execute_inc_r8(debug_prefix, Register::L);
        case 0x3C: return execute_inc_r8(debug_prefix, Register::A);

        // INC RR
        case 0x03: return execute_inc_r8(debug_prefix, Register::BC);
        case 0x13: return execute_inc_r8(debug_prefix, Register::DE);
        case 0x23: return execute_inc_r8(debug_prefix, Register::HL);
        case 0x33: return execute_inc_r8(debug_prefix, Register::SP);

        // INC [HL]
        case 0x34: throw std::runtime_error("INC (HL)");

        default: return false;
    }
}

bool Cpu::execute_inc_r8(const std::string &debug_prefix, const Register reg) {
    spdlog::info(fmt::format("{} INC {}", debug_prefix, magic_enum::enum_name(reg)));

    const u8 old_value = m_registers.get_u8(reg);
    const u8 new_value = old_value + 1;

    m_registers.set_z(new_value == 0);
    m_registers.set_n(false);
    m_registers.set_h(new_value & 0x0F == 0);

    m_registers.set_u8(reg, new_value);
    return true;
}

bool Cpu::execute_inc_r16(const std::string &debug_prefix, const Register reg) {
    spdlog::info(fmt::format("{} INC {}", debug_prefix, magic_enum::enum_name(reg)));

    const u16 old_value = m_registers.get_u16(reg);
    const u16 new_value = old_value + 1;

    m_registers.set_u16(reg, new_value);

    // 16-bit operations take 1 extra cycle and don't modify flags
    m_emulator.add_cycles(1);
    return true;
}

bool Cpu::execute_dec(const std::string &debug_prefix, u8 opcode) {
    switch (opcode) {
        // INC R
        case 0x05: return execute_dec_r8(debug_prefix, Register::B);
        case 0x15: return execute_dec_r8(debug_prefix, Register::C);
        case 0x25: return execute_dec_r8(debug_prefix, Register::H);
        case 0x0D: return execute_dec_r8(debug_prefix, Register::C);
        case 0x1D: return execute_dec_r8(debug_prefix, Register::E);
        case 0x2D: return execute_dec_r8(debug_prefix, Register::L);
        case 0x3D: return execute_dec_r8(debug_prefix, Register::A);

        // INC RR
        case 0x0B: return execute_dec_r8(debug_prefix, Register::BC);
        case 0x1B: return execute_dec_r8(debug_prefix, Register::DE);
        case 0x2B: return execute_dec_r8(debug_prefix, Register::HL);
        case 0x3B: return execute_dec_r8(debug_prefix, Register::SP);

        // INC [HL]
        case 0x35: throw std::runtime_error("DEC (HL)");

        default: return false;
    }
}

bool Cpu::execute_dec_r8(const std::string &debug_prefix, const Register reg) {
    spdlog::info(fmt::format("{} DEC {}", debug_prefix, magic_enum::enum_name(reg)));

    const u8 old_value = m_registers.get_u8(reg);
    const u8 new_value = old_value - 1;

    m_registers.set_z(new_value == 0);
    m_registers.set_n(true);
    m_registers.set_h(new_value & 0x0F == 0);

    m_registers.set_u8(reg, new_value);
    return true;
}

bool Cpu::execute_dec_r16(const std::string &debug_prefix, const Register reg) {
    spdlog::info(fmt::format("{} DEC {}", debug_prefix, magic_enum::enum_name(reg)));

    const u16 old_value = m_registers.get_u16(reg);
    const u16 new_value = old_value - 1;

    m_registers.set_u16(reg, new_value);

    // 16-bit operations take 1 extra cycle and don't modify flags
    m_emulator.add_cycles(1);
    return true;
}

bool Cpu::execute_xor(const std::string &debug_prefix, u8 opcode) {
    switch (opcode) {
        case 0xA8: execute_xor_reg(debug_prefix, Register::B); return true;
        case 0xA9: execute_xor_reg(debug_prefix, Register::C); return true;
        case 0xAA: execute_xor_reg(debug_prefix, Register::D); return true;
        case 0xAB: execute_xor_reg(debug_prefix, Register::E); return true;
        case 0xAC: execute_xor_reg(debug_prefix, Register::H); return true;
        case 0xAD: execute_xor_reg(debug_prefix, Register::L); return true;
        case 0xAF: execute_xor_reg(debug_prefix, Register::A); return true;
        default: return false;
    }
}

void Cpu::execute_xor_reg(const std::string &debug_prefix, const Register input_reg) {
    spdlog::info(fmt::format("{} XOR {}", debug_prefix, magic_enum::enum_name(input_reg)));

    const auto data = m_registers.get_u8(input_reg);
    m_registers.a ^= data;
    m_registers.set_z(m_registers.a == 0);
}

bool Cpu::execute_cp(const std::string &debug_prefix, u8 opcode) {
    switch (opcode) {
        case 0xFE: execute_cp(debug_prefix, Register::B); return true;
    }

    return false;
}

// CP r: Compare (register)
//
// Subtracts from the 8-bit A register, the 8-bit register r, and updates flags based on the result.
// This instruction is basically identical to SUB r, but does not update the A register
//
// opcode = read_memory(addr=PC); PC = PC + 1
// if opcode == 0xB8: # example: CP B
//     result, carry_per_bit = A - B
//     flags.Z = 1 if result == 0 else 0
//     flags.N = 1
//     flags.H = 1 if carry_per_bit[3] else 0
//     flags.C = 1 if carry_per_bit[7] else 0
void Cpu::execute_cp(const std::string &debug_prefix, const Register reg) {
    spdlog::info(fmt::format("{} CP {}", debug_prefix, magic_enum::enum_name(reg)));

    const auto a = static_cast<int>(m_registers.a);
    const auto r = static_cast<int>(m_registers.get_u8(reg));
    const auto result = a - r;
    const auto half_result = (a & 0x0F) - (r & 0x0F);

    m_registers.set_z(result == 0);
    m_registers.set_n(true);
    m_registers.set_h(half_result < 0);
    m_registers.set_c(result < 0);
}

bool Cpu::execute_and(const std::string &debug_prefix, const u8 opcode) {
    switch (opcode) {
        case 0xA0: return execute_and(debug_prefix, Register::B);
        case 0xA1: return execute_and(debug_prefix, Register::C);
        case 0xA2: return execute_and(debug_prefix, Register::D);
        case 0xA3: return execute_and(debug_prefix, Register::E);
        case 0xA4: return execute_and(debug_prefix, Register::H);
        case 0xA5: return execute_and(debug_prefix, Register::L);
        case 0xA6: return execute_and(debug_prefix, Register::HL);
        case 0xA7: return execute_and(debug_prefix, Register::A);
        default: return false;
    }
}

// Performs a bitwise AND operation between the 8-bit A register and the 8-bit register r, and stores the result back
// into the A register.
bool Cpu::execute_and(const std::string &debug_prefix, const Register reg) {
    spdlog::info(fmt::format("{} AND A, {}", debug_prefix, magic_enum::enum_name(reg)));

    if (reg == Register::HL) {
        throw std::runtime_error("TODO: AND A, HL");
    }

    if (is_16bit(reg)) {
        throw std::invalid_argument("reg");
    }

    m_registers.a = m_registers.a & m_registers.get_u8(reg);
    m_registers.set_z(m_registers.a == 0);
    m_registers.set_n(false);
    m_registers.set_h(true);
    m_registers.set_c(false);

    return true;
}

bool Cpu::execute_sub(const std::string &debug_prefix, const u8 opcode) {
    switch (opcode) {
        case 0x90: return execute_sub(debug_prefix, Register::B);
        case 0x91: return execute_sub(debug_prefix, Register::C);
        case 0x92: return execute_sub(debug_prefix, Register::D);
        case 0x93: return execute_sub(debug_prefix, Register::E);
        case 0x94: return execute_sub(debug_prefix, Register::H);
        case 0x95: return execute_sub(debug_prefix, Register::L);
        case 0x96: return execute_sub(debug_prefix, Register::HL);
        case 0x97: return execute_sub(debug_prefix, Register::A);
        default: return false;
    }
}

// Subtracts from the 8-bit A register, the 8-bit register r, and stores the result back into the A register
bool Cpu::execute_sub(const std::string &debug_prefix, const Register reg) {
    spdlog::info(fmt::format("{} SUB A, {}", debug_prefix, magic_enum::enum_name(reg)));

    if (reg == Register::HL) {
        throw std::runtime_error("TODO: SUB A, HL");
    }
    const auto a = static_cast<int>(m_registers.a);
    const auto r = static_cast<int>(m_registers.get_u8(reg));
    const auto result = a - r;
    const auto half_result = (a & 0x0F) - (r & 0x0F);

    m_registers.set_z(result == 0);
    m_registers.set_n(true);
    m_registers.set_h(half_result < 0);
    m_registers.set_c(result < 0);

    m_registers.a = result;
    return true;
}

bool Cpu::execute_jp(const std::string &debug_prefix, const u8 opcode) {
    switch (opcode) {
        // Absolute
        case 0xC2: return execute_jp_a16(debug_prefix, NZ);
        case 0xC3: return execute_jp_a16(debug_prefix, None);
        case 0xCA: return execute_jp_a16(debug_prefix, Z);
        case 0xB2: return execute_jp_a16(debug_prefix, NC);
        case 0xBA: return execute_jp_a16(debug_prefix, C);

        // Relative
        case 0x18: return execute_jr_e8(debug_prefix, None);
        case 0x20: return execute_jr_e8(debug_prefix, NZ);
        case 0x28: return execute_jr_e8(debug_prefix, Z);
        case 0x30: return execute_jr_e8(debug_prefix, NC);
        case 0x38: return execute_jr_e8(debug_prefix, C);

        case 0xE9: {
            spdlog::info(fmt::format("{} JP HL", debug_prefix));
            return execute_jp(None, m_registers.get_u16(Register::HL));
        }

        default: return false;
    }
}

bool Cpu::execute_jp_a16(const std::string &debug_prefix, const Condition condition) {
    const auto address = fetch_u16();
    if (condition == None) {
        spdlog::info(fmt::format("{} JP {:04x}", debug_prefix, address));
    } else {
        spdlog::info(fmt::format("{} JP {}, {:04x}", debug_prefix, magic_enum::enum_name(condition), address));
    }

    return execute_jp(condition, address);
}

bool Cpu::execute_jr_e8(const std::string &debug_prefix, const Condition condition) {
    const auto rel = static_cast<s8>(fetch_u8());
    const auto address = m_registers.pc + rel;
    if (condition == None) {
        spdlog::info(fmt::format("{} JR {} = JP {:04x}", debug_prefix, rel, address));
    } else {
        spdlog::info(fmt::format("{} JR {}, {} = JP {}, {:04x}", debug_prefix, magic_enum::enum_name(condition), rel,
                                 magic_enum::enum_name(condition), address));
    }

    return execute_jp(condition, address);
}

bool Cpu::execute_jp(const Condition condition, const u16 address) {
    if (m_registers.check_flags(condition)) {
        m_registers.pc = address;
        m_emulator.add_cycles();
    }
    return true;
}

bool Cpu::execute_call(const std::string &debug_prefix, const u8 opcode) {
    switch (opcode) {
        case 0xC4: return execute_call(debug_prefix, NZ);
        case 0xD4: return execute_call(debug_prefix, NC);
        case 0xCC: return execute_call(debug_prefix, Z);
        case 0xDC: return execute_call(debug_prefix, C);
        case 0xCD: return execute_call(debug_prefix, None);

        default: return false;
    }
}

bool Cpu::execute_call(const std::string &debug_prefix, const Condition condition) {
    const auto address = fetch_u16();
    if (condition == None) {
        spdlog::info(fmt::format("{} CALL {:04x}", debug_prefix, address));
    } else {
        spdlog::info(fmt::format("{} CALL {}, {:04x}", debug_prefix, magic_enum::enum_name(condition), address));
    }

    if (m_registers.check_flags(condition)) {
        stack_push16(m_registers.pc);
        m_registers.pc = address;
        m_emulator.add_cycles();
    }

    return true;
}
