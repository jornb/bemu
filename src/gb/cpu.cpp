#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <bemu/gb/cpu.hpp>
#include <bemu/gb/emulator.hpp>
#include <bemu/gb/utils.hpp>
#include <magic_enum.hpp>
#include <stdexcept>

using namespace bemu::gb;

void CpuRegisters::set_z(const bool z) { set_bit(f, 7, z); }

u8 CpuRegisters::get_u8(const RegisterType type) const {
    if (is_16bit(type)) {
        throw std::runtime_error(fmt::format("Tried to get CPU register {} as 8 bit", magic_enum::enum_name(type)));
    }

    switch (type) {
        case RegisterType::A:
            return a;
        case RegisterType::F:
            return f;
        case RegisterType::B:
            return b;
        case RegisterType::C:
            return c;
        case RegisterType::D:
            return d;
        case RegisterType::E:
            return e;
        case RegisterType::H:
            return h;
        case RegisterType::L:
            return l;
        default:
            throw std::runtime_error(fmt::format("Unknown CPU register {}", magic_enum::enum_name(type)));
    }
}

void CpuRegisters::set_u8(const RegisterType type, const u8 value) {
    if (is_16bit(type)) {
        throw std::runtime_error(fmt::format("Tried to set CPU register {} as 8 bit", magic_enum::enum_name(type)));
    }

    switch (type) {
        case RegisterType::A:
            a = value;
            break;
        case RegisterType::F:
            f = value;
            break;
        case RegisterType::B:
            b = value;
            break;
        case RegisterType::C:
            c = value;
            break;
        case RegisterType::D:
            d = value;
            break;
        case RegisterType::E:
            e = value;
            break;
        case RegisterType::H:
            h = value;
            break;
        case RegisterType::L:
            l = value;
            break;
        default:
            throw std::runtime_error(fmt::format("Unknown CPU register {}", magic_enum::enum_name(type)));
    }
}

u16 CpuRegisters::get_u16(RegisterType type) const {
    if (!is_16bit(type)) {
        throw std::runtime_error(fmt::format("Tried to get CPU register {} as 16 bit", magic_enum::enum_name(type)));
    }
    switch (type) {
        case RegisterType::AF:
            return combine_bytes(a, f);
        case RegisterType::BC:
            return combine_bytes(b, c);
        case RegisterType::DE:
            return combine_bytes(d, e);
        case RegisterType::HL:
            return combine_bytes(h, l);
        case RegisterType::PC:
            return pc;
        case RegisterType::SP:
            return sp;
        default:
            throw std::runtime_error(fmt::format("Unknown CPU register {}", magic_enum::enum_name(type)));
    }
}

void CpuRegisters::set_u16(const RegisterType type, const u16 value) {
    if (!is_16bit(type)) {
        throw std::runtime_error(fmt::format("Tried to get CPU register {} as 16 bit", magic_enum::enum_name(type)));
    }
    auto [hi, lo] = split_bytes(value);

    switch (type) {
        case RegisterType::AF: {
            a = hi;
            f = lo;
            break;
        }
        case RegisterType::BC: {
            b = hi;
            c = lo;
            break;
        }
        case RegisterType::DE: {
            d = hi;
            e = lo;
            break;
        }
        case RegisterType::HL: {
            h = hi;
            l = lo;
            break;
        }
        case RegisterType::PC: {
            pc = value;
            break;
        }
        case RegisterType::SP:
            sp = value;
            break;

        default:
            throw std::runtime_error(fmt::format("Unknown CPU register {}", magic_enum::enum_name(type)));
    }
}

u8 Cpu::fetch_u8() { return m_emulator.m_bus.read_u8(m_registers.pc++); }

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
        "{:08d} (+{:>2})    {:04x}    [A={:02x} F={:02x} B={:02x} C={:02x} D={:02x} E={:02x} H={:02x} L={:02x}]    "
        "({:02x} {:02x} {:02x})   ",
        ticks, ticks - last_ticks, pc, m_registers.a, m_registers.f, m_registers.b, m_registers.c, m_registers.d,
        m_registers.e, m_registers.h, m_registers.l, opcode, m_emulator.m_bus.read_u8(m_registers.pc, false),
        m_emulator.m_bus.read_u8(m_registers.pc + 1, false));
    last_ticks = ticks;
    // spdlog::info(fmt::format("{}", prefix));

    switch (opcode) {
        case 0x00: {
            spdlog::info(fmt::format("{} NOOP", prefix));
            return;
        }
        case 0x03: {
            spdlog::info(fmt::format("{} INC BC", prefix));
            execute_inc(RegisterType::BC);
            return;
        }
        case 0x3E: {  // LD A,d8
            const auto d8 = fetch_u8();
            spdlog::info(fmt::format("{} LD A, {:02x}", prefix, d8));
            execute_ld(RegisterType::A, d8);
            return;
        }
        case 0xAF: {  // XOR A
            spdlog::info(fmt::format("{} XOR A", prefix));
            execute_xor(RegisterType::A);
            return;
        }
        case 0xC3: {  // JP a16
            const auto a16_lo = fetch_u8();
            const auto a16_hi = fetch_u8();
            const auto address = combine_bytes(a16_hi, a16_lo);
            spdlog::info(fmt::format("{} JP {:04x}", prefix, address));
            execute_jp(address);
            return;
        }
        case 0xE0: {  // LDH (a8),A = LD (0xFF00 + a8),A
            const auto a8 = fetch_u8();
            const auto address = 0xFF00 + a8;
            spdlog::info(fmt::format("{} LDH ({:02x}), A = LD ({:04x}), A", prefix, a8, address));
            execute_ld(address, RegisterType::A);
            return;
        }
        case 0xF3: {
            spdlog::info(fmt::format("{} DI", prefix));
            m_int_master_enabled = false;
            return;
        }
        default:
            throw std::runtime_error(fmt::format("{} Unsupported opcode: {:02x}", prefix, opcode));
    }
}

void Cpu::execute_jp(const u16 address) {
    m_registers.pc = address;
    m_emulator.add_cycles();
}

void Cpu::execute_ld(const RegisterType reg, const u8 value) { m_registers.set_u8(reg, value); }

void Cpu::execute_ld(u16 address, RegisterType reg) {
    if (is_16bit(reg)) {
        throw std::runtime_error("execute_ld 16 not supported");
    } else {
        m_emulator.m_bus.write_u8(address, m_registers.get_u8(reg));
    }
}

void Cpu::execute_inc(const RegisterType reg) {
    if (is_16bit(reg)) {
        const auto old_value = m_registers.get_u16(reg);
        const auto new_value = old_value + 1;
        m_registers.set_u16(reg, new_value);

        // 16-bit operations take 1 extra cycle
        m_emulator.add_cycles(1);
    } else {
        const auto old_value = m_registers.get_u8(reg);
        const auto new_value = old_value + 1;
        m_registers.set_u8(reg, new_value);
    }
}

void Cpu::execute_xor(const RegisterType input_reg) {
    const auto data = m_registers.get_u8(input_reg);
    m_registers.a ^= data;
    m_registers.set_z(m_registers.a == 0);
}
