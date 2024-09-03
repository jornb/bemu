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

    return m_data.at(address);
}

void Cartridge::write(const u16 address, const u8 value) {

    // TODO: Implement bank switching
    if (m_external_ram_banks[0].contains(address)) {
        if (m_ram_enabled) {
            m_external_ram_banks[0].write(address, value);
        }

        return;
    }

    m_data.at(address) = value;
}