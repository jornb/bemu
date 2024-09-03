#pragma once

#include <optional>
#include <string>
#include <vector>

#include "ram.hpp"
#include "types.hpp"

namespace bemu::gb {

enum class CartridgeType : u8 {
    ROM_ONLY = 0x00,
    MBC1 = 0x01,
    MBC1_RAM = 0x02,
    MBC1_RAM_BATTERY = 0x03,
    MBC2 = 0x05,
    MBC2_BATTERY = 0x06,
    ROM_RAM = 0x08,
    ROM_RAM_BATTERY = 0x09,
    MMM01 = 0x0B,
    MMM01_RAM = 0x0C,
    MMM01_RAM_BATTERY = 0x0D,
    MBC3_TIMER_BATTERY = 0x0F,
    MBC3_TIMER_RAM_BATTERY = 0x10,
    MBC3 = 0x11,
    MBC3_RAM = 0x12,
    MBC3_RAM_BATTERY = 0x13,
    MBC5 = 0x19,
    MBC5_RAM = 0x1A,
    MBC5_RAM_BATTERY = 0x1B,
    MBC5_RUMBLE = 0x1C,
    MBC5_RUMBLE_RAM = 0x1D,
    MBC5_RUMBLE_RAM_BATTERY = 0x1E,
    MBC6 = 0x20,
    MBC7_SENSOR_RUMBLE_RAM_BATTERY = 0x22,
    POCKET_CAMERA = 0xFC,
    BANDAI_TAMA5 = 0xFD,
    HuC3 = 0xFE,
    HuC1_RAM_BATTERY = 0xFF,
};

enum class RomSizeType : u8 {
    Kb32_bank2 = 0x00,
    Kb64_bank4 = 0x01,
    Kb128_bank8 = 0x02,
    Kb256_bank16 = 0x03,
    Kb512_bank32 = 0x04,
    Kb1024_bank64 = 0x05,
    Kb2048_bank128 = 0x06,
    Kb4096_bank256 = 0x07,
    Kb8192_bank512 = 0x08,
};

enum class RamSizeType : u8 {
    None = 0x00,   ///< No RAM
    Kb8 = 0x02,    ///< 1 bank
    Kb32 = 0x03,   ///< 4 banks of 8 KiB each
    Kb64 = 0x05,   ///< 8 banks of 8 KiB each
    Kb128 = 0x04,  ///< 16 banks of 8 KiB each
};

#pragma pack(push, 1)
/// Each cartridge contains a header, located at the address range $0100-$014F.
struct CartridgeHeader {
    /// 0100-0103 - Entry point
    ///
    /// After displaying the Nintendo logo, the built-in boot ROM jumps to the address $0100, which should then jump to
    /// the actual main program in the cartridge. Most commercial games fill this 4-byte area with a nop instruction
    /// followed by a jp $0150.
    u8 entry[4];

    /// 0104-0133 - Nintendo logo
    /// This area contains a bitmap image that is displayed when the Game Boy is powered on.
    u8 logo[0x30];

    /// 0134-0143 - Title
    /// These bytes contain the title of the game in upper case ASCII. If the title is less than 16 characters long, the
    /// remaining bytes should be padded with $00s.
    char title[16];

    /// 0144–0145 - New licensee code
    /// This area contains a two-character ASCII "icensee code" indicating the game’s publisher. It is only meaningful
    /// if the Old licensee is exactly $33 (which is the case for essentially all games made after the SGB was
    /// released); otherwise, the old code must be considered.
    u16 new_lic_code;

    /// 0146 - SGB flag
    /// This byte specifies whether the game supports SGB functions. The SGB will ignore any command packets if this
    /// byte is set to a value other than $03 (typically $00).
    u8 sgb_flag;

    /// 0147 - Cartridge type
    /// This byte indicates what kind of hardware is present on the cartridge - most notably its mapper.
    CartridgeType cartridge_type;

    /// 0148 - ROM size
    /// This byte indicates how much ROM is present on the cartridge. In most cases, the ROM size is given by
    ///   32 KiB * (1 << <value>)
    RomSizeType rom_size;

    RamSizeType ram_size;

    /// 014A - Destination code This byte specifies whether this version of the game is intended to be sold in Japan or
    /// elsewhere. Only two values are defined :
    ///   0x00  Japan(and possibly overseas)
    ///   0x01  Overseas only
    u8 dest_code;

    /// 014B - Old licensee code
    /// This byte is used in older(pre - SGB) cartridges to specify the game's publisher. However, the value $33
    /// indicates that the New licensee codes must be considered instead. (The SGB will ignore any command packets
    /// unless this value is $33.)
    u8 lic_code;

    /// 014C - Mask ROM version number
    /// This byte specifies the version number of the game. It is usually $00.
    u8 version;

    /// 014D — Header checksum
    /// This byte contains an 8-bit checksum computed from the cartridge header bytes $0134–014C. The boot ROM computes
    /// the checksum as follows:
    ///
    /// uint8_t checksum = 0;
    /// for (uint16_t address = 0x0134; address <= 0x014C; address++) {
    ///   checksum = checksum - rom[address] - 1;
    /// }
    ///
    /// The boot ROM verifies this checksum.If the byte at $014D does not match the lower 8 bits of checksum, the boot
    /// ROM will lock up and the program in the cartridge won't run.
    u8 checksum;

    /// 014E-014F — Global checksum
    ///
    /// These bytes contain a 16-bit (big-endian) checksum simply computed as the sum of all the bytes of the cartridge
    /// ROM (except these two checksum bytes).
    ///
    /// This checksum is not verified, except by Pokémon Stadiums "GB Tower" emulator (presumably to detect Transfer Pak
    /// errors).
    u16 global_checksum;

    std::string get_title() const;
};
#pragma pack(pop)

struct Cartridge {
    static Cartridge from_file(const std::string& filename);

    [[nodiscard]] const CartridgeHeader& header() const;

    bool contains(u16 address) const;
    [[nodiscard]] u8 read(u16 address) const;
    void write(u16 address, u8 value);

private:
    std::vector<u8> m_data;

    bool m_ram_enabled = true;
    std::array<RAM<0xA000, 0xBFFF>, 16> m_external_ram_banks{};
};

}  // namespace bemu::gb
