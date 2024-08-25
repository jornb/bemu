#pragma once
#include <chrono>

#include "MBC0.hpp"

namespace bemu::gb {

/// MBC3
/// (max 2MByte ROM and/or 32KByte RAM and Timer)
///
/// Beside for the ability to access up to 2MB ROM (128 banks), and 32KB RAM (4 banks), the MBC3 also includes a
/// built-in Real Time Clock (RTC). The RTC requires an external 32.768 kHz Quartz Oscillator, and an external battery
/// (if it should continue to tick when the Game Boy is turned off).
struct MBC3 : BaseMapper {
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
        // Same as for MBC1, except that accessing banks $20, $40, and $60 is supported now.
        const auto a = 0x4000 * m_rom_bank_number + (address - 0x4000);
        return m_data.at(a);
    }

    void write_rom(const u16 address, u8 value) override {
        // 0000-1FFF - RAM and Timer Enable (Write Only)
        //
        // Mostly the same as for MBC1, a value of $0A will enable reading and writing to external RAM - and to the RTC
        // Registers! A value of $00 will disable either.
        if (address <= 0x1FFF) {
            m_ram_enabled = (value & 0xF) == 0xA;
        }

        // 2000-3FFF - ROM Bank Number (Write Only)
        //
        // Same as for MBC1, except that the whole 7 bits of the ROM Bank Number are written directly to this address.
        // As for the MBC1, writing a value of $00 will select Bank $01 instead. All other values $01-$7F select the
        // corresponding ROM Banks.
        else if (address <= 0x3FFF) {
            // Mask to 7-bit number
            value = value & 0b1111111;

            auto selection = value;

            // Writing 0 behaves as if 1 was written
            if (selection == 0) selection = 1;

            // Mask to the number of bits necessary for the number of banks we have
            // e.g. 256 KiB ROM = 16 banks = 4 bits
            selection &= m_num_rom_banks - 1;

            m_rom_bank_number = selection;
        }

        // 4000–5FFF - RAM Bank Number - or - Upper Bits of ROM Bank Number (Write Only)
        //
        // This second 2-bit register can be used to select a RAM Bank in range from $00–$03 (32 KiB ram carts only), or
        // to specify the upper two bits (bits 5-6) of the ROM Bank number (1 MiB ROM or larger carts only). If neither
        // ROM nor RAM is large enough, setting this register does nothing.
        //
        // In 1MB MBC1 multi-carts (see below), this 2-bit register is instead applied to bits 4-5 of the ROM bank
        // number and the top bit of the main 5-bit main ROM banking register is ignored.
        else if (address <= 0x5FFF) {
            m_ram_bank_number = value & 0b1111;
        }

        // 6000-7FFF - Latch Clock Data (Write Only)
        //
        // When writing $00, and then $01 to this register, the current time becomes latched into the RTC registers. The
        // latched data will not change until it becomes latched again, by repeating the write $00->$01 procedure. This
        // provides a way to read the RTC registers while the clock keeps ticking.
        else if (address <= 0x7FFF) {
            if (m_last_latch_write == 0 && value == 1) {
                last_latch = std::chrono::system_clock::now();
            }
            m_last_latch_write = value;
        }
    }

    u8 read_ram(const u16 address) const override {
        if (m_ram.empty() || !m_ram_enabled) {
            return 0xFF;
        }

        // RTC
        if (m_ram_bank_number > 0x07) {
            // TODO
            return 0x00;
        }

        return m_ram.at(ram_address_to_index(address));
    }

    void write_ram(const u16 address, const u8 value) override {
        if (m_ram.empty() || !m_ram_enabled) {
            return;
        }

        // RTC
        if (m_ram_bank_number > 0x07) {
            // TODO
            return;
        }

        m_ram.at(ram_address_to_index(address)) = value;
    }

    void serialize(auto &ar) {
        BaseMapper::serialize(ar);

        // ar(m_last_latch_write);
        // ar(last_latch);
        ar(m_rtc_register_select);
        ar(m_rom_bank_number);
        ar(m_ram_enabled);
    }

    u8 m_last_latch_write = 0xFF;
    std::chrono::system_clock::time_point last_latch = std::chrono::system_clock::now();
    bool m_rtc_register_select = false;  ///< If set, reads RTC registers instead of RAM
    u8 m_rom_bank_number = 1;
    bool m_ram_enabled = true;
};
}  // namespace bemu::gb