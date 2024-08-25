#pragma once
#include "MBC0.hpp"

namespace bemu::gb {

/// MBC1_0 is a simplified version of MBC1 which only supports up to 512 KiB ROM and 8 KiB RAM.
///
/// See https://gbdev.io/pandocs/MBC1.html for details.
///
/// (max 2MByte ROM and/or 32 KiB RAM)
///
/// This is the first MBC chip for the Game Boy. Any newer MBC chips work similarly, so it is relatively easy to
/// upgrade a program from one MBC chip to another — or to make it compatible with several types of MBCs.
///
/// In its default configuration, MBC1 supports up to 512 KiB ROM with up to 32 KiB of banked RAM. Some cartridges wire
/// the MBC differently, where the 2-bit RAM banking register is wired as an extension of the ROM banking register
/// (instead of to RAM) in order to support up to 2 MiB ROM, at the cost of only supporting a fixed 8 KiB of cartridge
/// RAM. All MBC1 cartridges with 1 MiB of ROM or more use this alternate wiring. Also see the note on MBC1M multi-game
/// compilation carts below.
///
/// Note that the memory in range 0000–7FFF is used both for reading from ROM and writing to the MBCs Control Registers.
struct MBC1_0 : BaseMapper {
    using BaseMapper::BaseMapper;

    u8 read_rom(const u16 address) const override {
        // 0000–3FFF - ROM Bank X0 [read-only]
        if (address <= 0x3FFF) {
            return m_data.at(address);
        }

        // 4000–7FFF — ROM Bank 01-7F
        const auto a = 0x4000 * m_rom_bank_number + (address - 0x4000);
        return m_data.at(a);
    }

    void write_rom(const u16 address, u8 value) override {
        // 0000-1FFF - RAM Enable (Write Only)
        //
        // Before external RAM can be read or written, it must be enabled by writing $A to anywhere in this address
        // space. Any value with $A in the lower 4 bits enables the RAM attached to the MBC, and any other value
        // disables the RAM. It is unknown why $A is the value used to enable RAM.
        //
        // It is recommended to disable external RAM after accessing it, in order to protect its contents from
        // corruption during power down of the Game Boy or removal of the cartridge. Once the cartridge has completely
        // lost power from the Game Boy, the RAM is automatically disabled to protect it.
        if (address <= 0x1FFF) {
            m_ram_enabled = (value & 0xF) == 0xA;
        }

        // 2000-3FFF - ROM Bank Number (Write Only)
        //
        // This 5-bit register (range $01-$1F) selects the ROM bank number for the 4000–7FFF region. Higher bits are
        // discarded — writing $E1 (binary 11100001) to this register would select bank $01.
        //
        // If this register is set to $00, it behaves as if it is set to $01. This means you cannot duplicate bank $00
        // into both the 0000–3FFF and 4000–7FFF ranges by setting this register to $00.
        //
        // If the ROM Bank Number is set to a higher value than the number of banks in the cart, the bank number is
        // masked to the required number of bits. e.g. a 256 KiB cart only needs a 4-bit bank number to address all of
        // its 16 banks, so this register is masked to 4 bits. The upper bit would be ignored for bank selection.
        else if (0x2000 <= address && address <= 0x3FFF) {
            // Mask to 5-bit number
            value = value & 0b11111;

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
        else if (0x4000 <= address && address <= 0x5FFF) {
            // We only support up to 8 KiB RAM and 512 KiB ROM, so this is always RAM banking mode
            m_ram_bank_number = value & 0b11;
        }

        // 6000–7FFF - Banking Mode Select (Write Only)
        //
        // This 1-bit register selects between the two MBC1 banking modes, controlling the behaviour of the
        // secondary 2-bit banking register (above). If the cart is not large enough to use the 2-bit register (≤
        // 8 KiB RAM and ≤ 512 KiB ROM) this mode select has no observable effect. The program may freely switch
        // between the two modes at any time.
        else if (0x6000 <= address && address <= 0x7FFF) {
            // We only support up to 8 KiB RAM and 512 KiB ROM, so this is always regular RAM banking mode
        }
    }

    u8 read_ram(const u16 address) const override {
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

    u8 m_rom_bank_number = 1;
    bool m_ram_enabled = true;
};
}  // namespace bemu::gb