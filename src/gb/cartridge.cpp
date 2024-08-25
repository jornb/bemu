#include <spdlog/fmt/fmt.h>

#include <bemu/gb/cartridge.hpp>
#include <fstream>

using namespace bemu::gb;

std::string CartridgeHeader::get_title() const { return title; }

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
    cartridge.data.resize(size, 0x00);

    file.read(reinterpret_cast<char*>(cartridge.data.data()), size);
    return cartridge;
}

const CartridgeHeader& Cartridge::header() const {
    return *reinterpret_cast<const CartridgeHeader*>(&data[0x0100]);
}
