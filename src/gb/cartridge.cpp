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

u8 Cartridge::read(const u16 address) const {
    if (address >= m_data.size()) {
        throw std::runtime_error(fmt::format("Address {} too large", address));
    }
    return m_data[address];
}

void Cartridge::write(const u16 address, const u8 value) {
    if (address >= m_data.size()) {
        throw std::runtime_error(fmt::format("Address {} too large", address));
    }

    m_data[address] = value;
}