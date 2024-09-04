#include <spdlog/fmt/fmt.h>

#include <bemu/gb/cartridge.hpp>
#include <fstream>

using namespace bemu::gb;

std::string CartridgeHeader::get_title() const { return title; }

const CartridgeHeader& Cartridge::header() const { return *reinterpret_cast<const CartridgeHeader*>(&m_data[0x0100]); }

Cartridge Cartridge::from_file(const std::string& filename) {
    std::ifstream file{filename, std::ios::binary};
    if (!file.is_open()) {
        throw std::runtime_error(fmt::format("Could not open file {}", filename));
    }

    file.seekg(0, std::ios::end);
    const unsigned size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size < 0x0150) {
        throw std::runtime_error(fmt::format("File size {} too small", size));
    }

    Cartridge cartridge;
    cartridge.m_data.resize(size, 0x00);

    file.read(reinterpret_cast<char*>(cartridge.m_data.data()), size);
    return cartridge;
}

bool Cartridge::contains(const u16 address) const {
    return address <= 0x7FFF || m_external_ram_banks[0].contains(address);
}

u8 Cartridge::read(const u16 address) const {
    // TODO: Implement bank switching
    if (m_external_ram_banks[0].contains(address)) {
        if (m_ram_enabled) {
            return m_external_ram_banks[0].read(address);
        }
        return 0xFF;
    }

    // 0000–3FFF - ROM Bank X0 [read-only]
    if (address <= 0x3FFF) {
        return m_data.at(address);
    }

    // 0x4000 <= address <= 7FFF
    return m_data.at(0x4000 * m_rom_bank_number + (address - 0x4000));
}

void Cartridge::write(const u16 address, const u8 value) {
    // 0000-1FFF - RAM Enable (Write Only)
    //
    // Before external RAM can be read or written, it must be enabled by writing $A to anywhere in this address space.
    // Any value with $A in the lower 4 bits enables the RAM attached to the MBC, and any other value disables the RAM.
    // It is unknown why $A is the value used to enable RAM.
    //
    // It is recommended to disable external RAM after accessing it, in order to protect its contents from corruption
    // during power down of the Game Boy or removal of the cartridge. Once the cartridge has completely lost power from
    // the Game Boy, the RAM is automatically disabled to protect it.
    if (address <= 0x1FFF) {
        m_ram_enabled = value & 0xF == 0xA;
    }

    // 2000-3FFF - ROM Bank Number (Write Only)
    //
    // This 5-bit register (range $01-$1F) selects the ROM bank number for the 4000–7FFF region. Higher bits are
    // discarded — writing $E1 (binary 11100001) to this register would select bank $01.
    //
    // If this register is set to $00, it behaves as if it is set to $01. This means you cannot duplicate bank $00 into
    // both the 0000–3FFF and 4000–7FFF ranges by setting this register to $00.
    //
    // If the ROM Bank Number is set to a higher value than the number of banks in the cart, the bank number is masked
    // to the required number of bits. e.g. a 256 KiB cart only needs a 4-bit bank number to address all of its 16
    // banks, so this register is masked to 4 bits. The upper bit would be ignored for bank selection.
    //
    // TODO: On larger carts which need a >5 bit bank number, the secondary banking register at 4000–5FFF is used to
    //       supply an additional 2 bits for the effective bank number: Selected ROM Bank = (Secondary Bank << 5) + ROM
    //       Bank.
    //       https://gbdev.io/pandocs/MBC1.html
    if (0x2000 <= address <= 0x3FFF) {
        m_rom_bank_number = value & 0b11111;
        if (m_rom_bank_number == 0) m_rom_bank_number = 1;
    }

    // TODO: 4000–5FFF - RAM Bank Number - or - Upper Bits of ROM Bank Number (Write Only)
    //       This second 2-bit register can be used to select a RAM Bank in range from $00–$03 (32 KiB ram carts only),
    //       or to specify the upper two bits (bits 5-6) of the ROM Bank number (1 MiB ROM or larger carts only). If
    //       neither ROM nor RAM is large enough, setting this register does nothing.
    //       In 1MB MBC1 multi-carts (see below), this 2-bit register is instead applied to bits 4-5 of the ROM bank
    //       number and the top bit of the main 5-bit main ROM banking register is ignored.
    //       https://gbdev.io/pandocs/MBC1.html

    // TODO: 6000–7FFF - Banking Mode Select (Write Only)
    //       This 1-bit register selects between the two MBC1 banking modes, controlling the behaviour of the secondary
    //       2-bit banking register (above). If the cart is not large enough to use the 2-bit register (≤ 8 KiB RAM and
    //       ≤ 512 KiB ROM) this mode select has no observable effect. The program may freely switch between the two
    //       modes at any time.
    //       https://gbdev.io/pandocs/MBC1.html

    // TODO: Implement bank switching
    if (m_external_ram_banks[0].contains(address)) {
        if (m_ram_enabled) {
            m_external_ram_banks[0].write(address, value);
        }

        return;
    }

    // Read-only
}