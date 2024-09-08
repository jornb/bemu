#include <spdlog/fmt/fmt.h>

#include <bemu/gb/bus.hpp>
#include <bemu/gb/cpu.hpp>
#include <bemu/gb/emulator.hpp>
#include <stdexcept>

using namespace bemu;
using namespace bemu::gb;

// 0x0000 - 0x3FFF : ROM Bank 0
// 0x4000 - 0x7FFF : ROM Bank 1 - Switchable
// 0x8000 - 0x97FF : CHR RAM
// 0x9800 - 0x9BFF : BG Map 1
// 0x9C00 - 0x9FFF : BG Map 2
// 0xA000 - 0xBFFF : Cartridge RAM
// 0xC000 - 0xCFFF : RAM Bank 0
// 0xD000 - 0xDFFF : RAM Bank 1-7 - switchable - Color only
// 0xE000 - 0xFDFF : Reserved - Echo RAM
// 0xFE00 - 0xFE9F : Object Attribute Memory
// 0xFEA0 - 0xFEFF : Reserved - Unusable
// 0xFF00 - 0xFF7F : I/O Registers
// 0xFF80 - 0xFFFE : Zero Page

Bus::Bus(Emulator& emulator) : m_emulator(emulator), m_ppu(*this, m_lcd, m_emulator.m_screen, m_emulator.m_cpu) {}

u8 Bus::read_u8(const u16 address, const bool add_cycles) const {
    if (add_cycles) m_emulator.add_cycles();

    if (m_emulator.m_cpu.contains(address)) {
        return m_emulator.m_cpu.read_memory(address);
    }

    if (m_emulator.m_cartridge.contains(address)) {
        return m_emulator.m_cartridge.read(address);
    }

    if (m_wram.contains(address)) {
        return m_wram.read(address);
    }

    if (m_hram.contains(address)) {
        return m_hram.read(address);
    }

    if (m_ppu.contains(address)) {
        return m_ppu.read(address);
    }

    if (m_audio.contains(address)) {
        return m_audio.read(address);
    }

    if (m_serial.contains(address)) {
        return m_serial.read_memory(address);
    }

    if (address >= 0xFF00 && address <= 0xFF7F) {
        return m_io.read(address);
    }
    if (0xFEA0 <= address && address <= 0xFEFF) {
        // Nintendo indicates use of this area is prohibited. This area returns $FF when OAM is blocked, and otherwise
        // the behavior depends on the hardware revision.
        //
        // * On DMG, MGB, SGB, and SGB2, reads during OAM block trigger OAM corruption. Reads otherwise return $00.
        return 0x00;
    }

    // // Gameboy color only registers
    // if (address == 0xFF4D) {
    //     return 0x00;
    // }

    // static u8 val = 0x00;  /// FIXME
    // return val++;
    throw std::runtime_error(fmt::format("Unsupported memory address (read) {:04x}", address));
}

void Bus::write_u8(const u16 address, const u8 value, const bool add_cycles) {
    if (add_cycles) m_emulator.add_cycles();

    if (m_emulator.m_cpu.contains(address)) {
        return m_emulator.m_cpu.write_memory(address, value);
    }

    if (m_emulator.m_cartridge.contains(address)) {
        return m_emulator.m_cartridge.write(address, value);
    }

    if (m_wram.contains(address)) {
        return m_wram.write(address, value);
    }

    if (m_hram.contains(address)) {
        return m_hram.write(address, value);
    }

    if (m_ppu.contains(address)) {
        return m_ppu.write(address, value);
    }

    if (m_audio.contains(address)) {
        return m_audio.write(address, value);
    }

    if (m_serial.contains(address)) {
        return m_serial.write_memory(address, value);
    }

    if (address >= 0xFF00 && address <= 0xFF7F) {
        return m_io.write(address, value);
    }
    if (0xFEA0 <= address && address <= 0xFEFF) {
        // Reserved
        return;
    }

    throw std::runtime_error(fmt::format("Unsupported memory address (write) {:04x}", address));
}

u16 Bus::read_u16(const u16 address, const bool add_cycles) const {
    const auto lo = read_u8(address, add_cycles);
    const auto hi = read_u8(address + 1, add_cycles);
    return combine_bytes(hi, lo);
}

void Bus::write_u16(const u16 address, const u16 value, const bool add_cycles) {
    auto [hi, lo] = split_bytes(value);
    write_u8(address, hi, add_cycles);
    write_u8(address + 1, lo, add_cycles);
}
