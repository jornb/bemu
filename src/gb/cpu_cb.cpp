// #include <spdlog/fmt/fmt.h>
// #include <spdlog/spdlog.h>
//
// #include <bemu/gb/cpu.hpp>
// #include <bemu/gb/emulator.hpp>
// #include <bemu/gb/utils.hpp>
// #include <magic_enum.hpp>
// #include <stdexcept>
//
// using namespace bemu::gb;
//
// // Lookup of the register name in the CB opcode (last 3 bits)
// // https://gbdev.io/gb-opcodes/optables/octal
// static Register col_to_register[] = {Register::B, Register::C, Register::D,  Register::E,
//                                      Register::H, Register::L, Register::HL, Register::A};
//
// bool Cpu::execute_cb(const std::string &debug_prefix) {
//     const auto opcode = m_emulator.m_bus.read_u8(m_registers.pc++);
//
//     // Note: Use the octal CB mapping to make sense of this
//     // https://gbdev.io/gb-opcodes/optables/octal
//     const u8 col = opcode & 0b111;
//     const auto row = (opcode >> 3);
//
//     // Lookup the register the CB opcode applies to
//     const auto reg_type = col_to_register[opcode & 0b111];
//     const auto reg_value =
//         reg_type == Register::HL ? m_emulator.m_bus.read_u8(m_registers.get_HL()) : m_registers.get_u8(reg_type);
//
//     spdlog::info("{} {:0x} {:o} {}={}", debug_prefix, opcode, row, col, magic_enum::enum_name(reg_type));
//
//     throw std::runtime_error(debug_prefix + "CB not implemented");
//     return false;
// }
