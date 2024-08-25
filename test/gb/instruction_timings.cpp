#include <bemu/gb/cpu/opcodes.hpp>
#include <bemu/gb/emulator.hpp>
#include <functional>
#include <iostream>
#include <optional>

using namespace bemu;
using namespace bemu::gb;

namespace {
constexpr u16 RAM_START = 0xC000;
int count_ticks(std::vector<u8> program, const std::optional<std::function<void(Cpu &)>> &setup) {
    for (size_t i = 0; i < 100; ++i) program.push_back(0x00);

    Emulator emulator{Cartridge::from_program_code(program)};

    // Step into 0x0150
    emulator.m_cpu.step();
    emulator.m_cpu.step();

    emulator.m_cpu.m_registers.set_u16(Register::BC, RAM_START);
    emulator.m_cpu.m_registers.set_u16(Register::DE, RAM_START);
    emulator.m_cpu.m_registers.set_u16(Register::HL, RAM_START);

    if (setup) setup.value()(emulator.m_cpu);

    const int t0 = emulator.m_external->m_ticks;
    emulator.m_cpu.step();
    const int t1 = emulator.m_external->m_ticks;

    return t1 - t0;
}

bool do_test(u8 opcode, const std::array<OpcodeMetadata, 256> &opcodes, const std::string &name,
             const std::vector<u8> &program, const std::optional<std::function<void(Cpu &)>> &setup,
             const bool branched) {
    const int ticks = count_ticks(program, setup);
    const auto &op = opcodes[opcode];
    const auto expected_tics = branched ? op.dots_branched : op.dots;
    if (ticks != expected_tics) {
        std::cout << "ERROR: ";
        std::cout << fmt::format("{:02x} {:<20}: {} | expected: {}\n", opcode, name, ticks, expected_tics);
        return false;
    }
    return true;
}

bool test(const u8 opcode, const u8 arg1 = 0x00, const u8 arg2 = 0x00,
          const std::optional<std::function<void(Cpu &)>> &setup = std::nullopt) {
    const std::string name = opcodes[opcode].mnemonic;
    return do_test(opcode, opcodes, name, {opcode, arg1, arg2}, setup, false);
}

bool test_cb(const u8 opcode, const u8 arg1 = 0x00, const u8 arg2 = 0x00,
             const std::optional<std::function<void(Cpu &)>> &setup = std::nullopt) {
    const std::string name = opcodes[opcode].mnemonic;
    return do_test(opcode, opcodes_cb, name, {0xCB, opcode, arg1, arg2}, setup, false);
}

bool test_branched(const u8 opcode, const u8 arg1 = 0x00, const u8 arg2 = 0x00,
                   const std::optional<std::function<void(Cpu &)>> &setup = std::nullopt) {
    const std::string name = opcodes[opcode].mnemonic;
    return do_test(opcode, opcodes, name + " (branched)", {opcode, arg1, arg2}, setup, true);
}
}  // namespace

int main() {
    bool result = test(0x00);

    result &= test(0x01);
    result &= test(0x11);
    result &= test(0x21);
    result &= test(0x31);

    result &= test(0x02);
    result &= test(0x12);
    result &= test(0x22);
    result &= test(0x32);

    result &= test(0x03);
    result &= test(0x13);
    result &= test(0x23);
    result &= test(0x33);

    result &= test(0x04);
    result &= test(0x14);
    result &= test(0x24);
    result &= test(0x34);

    result &= test(0x05);
    result &= test(0x15);
    result &= test(0x25);
    result &= test(0x35);

    result &= test(0x06);
    result &= test(0x16);
    result &= test(0x26);
    result &= test(0x36);

    result &= test(0x07);
    result &= test(0x17);
    result &= test(0x27);
    result &= test(0x37);

    result &= test(0x08, 0x00, 0xC0);

    result &= test(0x09);
    result &= test(0x19);
    result &= test(0x29);
    result &= test(0x39);

    result &= test(0x0A);
    result &= test(0x1A);
    result &= test(0x2A);
    result &= test(0x3A);

    result &= test(0x0B);
    result &= test(0x1B);
    result &= test(0x2B);
    result &= test(0x3B);

    result &= test(0x0C);
    result &= test(0x1C);
    result &= test(0x2C);
    result &= test(0x3C);

    result &= test(0x0D);
    result &= test(0x1D);
    result &= test(0x2D);
    result &= test(0x3D);

    result &= test(0x0E);
    result &= test(0x1E);
    result &= test(0x2E);
    result &= test(0x3E);

    result &= test(0x0F);
    result &= test(0x1F);
    result &= test(0x2F);
    result &= test(0x3F);

    // Jumps
    result &= test(0x20, 0x00, 0x00, [](Cpu &cpu) { cpu.m_registers.set_z(true); });
    result &= test_branched(0x20, 0x00, 0x00, [](Cpu &cpu) { cpu.m_registers.set_z(false); });
    result &= test(0x18);
    result &= test(0x28, 0x00, 0x00, [](Cpu &cpu) { cpu.m_registers.set_z(false); });
    result &= test_branched(0x28, 0x00, 0x00, [](Cpu &cpu) { cpu.m_registers.set_z(true); });
    result &= test(0x38, 0x00, 0x00, [](Cpu &cpu) { cpu.m_registers.set_c(false); });
    result &= test_branched(0x38, 0x00, 0x00, [](Cpu &cpu) { cpu.m_registers.set_c(true); });

    // 80 - BF
    for (u8 opcode = 0x80; opcode <= 0xBF; ++opcode) {
        result &= test(opcode);
    }

    result &= test(0xC0, 0x00, 0x00, [](Cpu &cpu) { cpu.m_registers.set_z(true); });
    result &= test_branched(0xC0, 0x00, 0x00, [](Cpu &cpu) { cpu.m_registers.set_z(false); });
    result &= test(0xE0);
    result &= test(0xF0);

    result &= test(0xC1);
    result &= test(0xD1);
    result &= test(0xE1);
    result &= test(0xF1);

    result &= test(0xC2, 0x00, 0x00, [](Cpu &cpu) { cpu.m_registers.set_z(true); });
    result &= test_branched(0xC2, 0x00, 0x00, [](Cpu &cpu) { cpu.m_registers.set_z(false); });
    result &= test(0xD2, 0x00, 0x00, [](Cpu &cpu) { cpu.m_registers.set_c(true); });
    result &= test_branched(0xD2, 0x00, 0x00, [](Cpu &cpu) { cpu.m_registers.set_c(false); });
    result &= test(0xE2);
    result &= test(0xF2);

    result &= test(0xC3);
    result &= test(0xF3);

    result &= test(0xC4, 0x00, 0x00, [](Cpu &cpu) { cpu.m_registers.set_z(true); });
    result &= test_branched(0xC4, 0x00, 0x00, [](Cpu &cpu) { cpu.m_registers.set_z(false); });
    result &= test(0xD4, 0x00, 0x00, [](Cpu &cpu) { cpu.m_registers.set_c(true); });
    result &= test_branched(0xD4, 0x00, 0x00, [](Cpu &cpu) { cpu.m_registers.set_c(false); });

    result &= test(0xC5);
    result &= test(0xD5);
    result &= test(0xE5);
    result &= test(0xF5);

    result &= test(0xC6);
    result &= test(0xD6);
    result &= test(0xE6);
    result &= test(0xF6);

    result &= test(0xC7);
    result &= test(0xD7);
    result &= test(0xE7);
    result &= test(0xF7);

    result &= test(0xC8, 0x00, 0x00, [](Cpu &cpu) { cpu.m_registers.set_z(false); });
    result &= test_branched(0xC8, 0x00, 0x00, [](Cpu &cpu) { cpu.m_registers.set_z(true); });
    result &= test(0xD8, 0x00, 0x00, [](Cpu &cpu) { cpu.m_registers.set_c(false); });
    result &= test_branched(0xD8, 0x00, 0x00, [](Cpu &cpu) { cpu.m_registers.set_c(true); });
    result &= test(0xE8);
    result &= test(0xF8);

    result &= test(0xC9);
    result &= test(0xD9);
    result &= test(0xE9);
    result &= test(0xF9);

    result &= test(0xCA, 0x00, 0x00, [](Cpu &cpu) { cpu.m_registers.set_z(false); });
    result &= test_branched(0xCA, 0x00, 0x00, [](Cpu &cpu) { cpu.m_registers.set_z(true); });
    result &= test(0xDA, 0x00, 0x00, [](Cpu &cpu) { cpu.m_registers.set_c(false); });
    result &= test_branched(0xDA, 0x00, 0x00, [](Cpu &cpu) { cpu.m_registers.set_c(true); });

    result &= test(0xFB);

    result &= test(0xCC, 0x00, 0x00, [](Cpu &cpu) { cpu.m_registers.set_z(false); });
    result &= test_branched(0xCC, 0x00, 0x00, [](Cpu &cpu) { cpu.m_registers.set_z(true); });
    result &= test(0xDC, 0x00, 0x00, [](Cpu &cpu) { cpu.m_registers.set_c(false); });
    result &= test_branched(0xDC, 0x00, 0x00, [](Cpu &cpu) { cpu.m_registers.set_c(true); });

    result &= test(0xCD);

    result &= test(0xCE);
    result &= test(0xDE);
    result &= test(0xEE);
    result &= test(0xFE);

    result &= test(0xCF);
    result &= test(0xDF);
    result &= test(0xEF);
    result &= test(0xFF);

    for (int opcode = 0x00; opcode <= 0xFF; ++opcode) {
        result &= test_cb(static_cast<u8>(opcode), opcode);
    }

    return result ? 0 : 1;
}
