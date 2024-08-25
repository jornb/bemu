#pragma once
#include <chrono>

#include "../../utils.hpp"
#include "MBC0.hpp"

namespace bemu::gb {

/// MBC5
/// It can map up to 64 Mbits (8 MiB) of ROM.
///
/// MBC5 (Memory Bank Controller 5) is the 5th generation MBC (MBC4 was not used in any released cartridges). It is the
/// first MBC that is guaranteed to work properly with GBC Double Speed mode.
struct MBC5 : BaseMapper {
    using BaseMapper::BaseMapper;

    u8 read_rom(const u16 address) const override {
        // 0000-3FFF - ROM Bank 00 (Read Only)
        //
        // Contains the first 16 KiB of the ROM.
        if (address <= 0x3FFF) {
            return m_data.at(address);
        }

        // 4000-7FFF - ROM Bank 01-7F (Read Only)
        //
        // Same as for MBC1, except that accessing up to bank $1FF is supported now. Also, bank 0 is actually bank 0.
        const auto a = 0x4000 * m_rom_bank_number + (address - 0x4000);
        return m_data.at(a);
    }

    void write_rom(const u16 address, u8 value) override {
        // 0000-1FFF - RAM and Timer Enable (Write Only)
        //
        // Mostly the same as for MBC1. Writing $0A will enable reading and writing to external RAM. Writing $00 will
        // disable it.
        //
        // Actual MBCs actually enable RAM when writing any value whose bottom 4 bits equal $A (so $0A, $1A, and so on),
        // and disable it when writing anything else. Relying on this behavior is not recommended for compatibility
        // reasons.
        if (address <= 0x1FFF) {
            m_ram_enabled = (value & 0xF) == 0xA;
        }

        // 2000-2FFF - 8 least significant bits of ROM bank number (Write Only)
        //
        // The 8 least significant bits of the ROM bank number go here. Writing 0 will indeed give bank 0 on MBC5,
        // unlike other MBCs.
        else if (address <= 0x2FFF) {
            m_rom_bank_number = (m_rom_bank_number & 0xFF00) | value;
        }

        // 3000-3FFF - 9th bit of ROM bank number (Write Only)
        //
        // The 9th bit of the ROM bank number goes here.
        else if (address <= 0x3FFF) {
            set_bit(m_rom_bank_number, 8, get_bit(value, 0));
        }

        // 4000-5FFF - RAM bank number (Write Only)
        //
        // As for the MBC1s RAM Banking Mode, writing a value in the range $00-$0F maps the corresponding external RAM
        // bank (if any) into the memory area at A000-BFFF.
        else if (address <= 0x5FFF) {
            m_ram_bank_number = value & 0b1111;
        }
    }

    u8 read_ram(const u16 address) const override {
        // A000-BFFF - RAM bank 00-0F, if any (Read/Write)
        //
        // Same as for MBC1, except that RAM sizes are 8 KiB, 32 KiB and 128 KiB.
        if (m_ram.empty() || !m_ram_enabled) {
            return 0xFF;
        }

        return m_ram.at(ram_address_to_index(address));
    }

    void write_ram(const u16 address, const u8 value) override {
        if (m_ram.empty() || !m_ram_enabled) {
            return;
        }

        m_ram.at(ram_address_to_index(address)) = value;
    }

    void serialize(auto &ar) {
        BaseMapper::serialize(ar);
        ar(m_rom_bank_number);
        ar(m_ram_enabled);
    }

    u16 m_rom_bank_number = 1;
    bool m_ram_enabled = true;
};
}  // namespace bemu::gb