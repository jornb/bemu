#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <bemu/gb/cartridge.hpp>
#include <bemu/gb/mappers/MBC0.hpp>
#include <bemu/gb/mappers/MBC1_0.hpp>
#include <bemu/gb/mappers/MBC3.hpp>
#include <bemu/gb/mappers/MBC5.hpp>
#include <fstream>
#include <magic_enum/magic_enum.hpp>

using namespace bemu;
using namespace bemu::gb;

std::string CartridgeHeader::get_title() const { return title; }

const CartridgeHeader& Cartridge::header() const {
    return *reinterpret_cast<const CartridgeHeader*>(m_data->data() + 0x0100);
}

Cartridge::~Cartridge() = default;

std::unique_ptr<Cartridge> Cartridge::from_file(const std::string& filename) {
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

    auto cartridge = std::make_unique<Cartridge>();
    cartridge->m_data->resize(size, 0x00);
    file.read(reinterpret_cast<char*>(cartridge->m_data->data()), size);
    cartridge->m_mapper = make_mapper(cartridge->header(), *cartridge->m_data);
    return cartridge;
}

std::unique_ptr<Cartridge> Cartridge::from_program_code(const std::vector<u8>& data) {
    CartridgeHeader header;

    header.entry[0] = 0x00;  // NOP
    header.entry[1] = 0xC3;  // JP
    header.entry[2] = 0x50;  // Low byte of address
    header.entry[3] = 0x01;  // High byte of address

    header.title[0] = 'T';
    header.title[1] = 'E';
    header.title[2] = 'S';
    header.title[3] = 'T';
    header.title[4] = '\0';

    header.cartridge_type = CartridgeType::ROM_ONLY;

    header.rom_size = RomSizeType::Kb32_bank2;
    header.ram_size = RamSizeType::Kb8;

    auto cartridge = std::make_unique<Cartridge>();
    cartridge->m_data->resize(0x0150 + data.size());

    // Set header
    reinterpret_cast<CartridgeHeader&>(cartridge->m_data->at(0x0100)) = header;

    // Set data
    for (size_t i = 0; i < data.size(); ++i) {
        cartridge->m_data->at(0x0150 + i) = data[i];
    }

    cartridge->m_mapper = make_mapper(cartridge->header(), *cartridge->m_data);

    return cartridge;
}

bool Cartridge::contains(const u16 address) const {
    return address <= 0x7FFF || (0xA000 <= address && address <= 0xBFFF);
}

u8 Cartridge::read(const u16 address) const {
    if (address < 0x8000) {
        return m_mapper->read_rom(address);
    }
    return m_mapper->read_ram(address);
}

void Cartridge::write(const u16 address, const u8 value) {
    if (address < 0x8000) {
        return m_mapper->write_rom(address, value);
    }
    return m_mapper->write_ram(address, value);
}

std::unique_ptr<IMapper> Cartridge::make_mapper(const CartridgeHeader& header, std::vector<u8>& data) {
    if (header.cartridge_type == CartridgeType::ROM_ONLY) {
        return std::make_unique<MBC0>(header.rom_size, header.ram_size, data);
    }
    if (header.cartridge_type == CartridgeType::MBC1 || header.cartridge_type == CartridgeType::MBC1_RAM ||
        header.cartridge_type == CartridgeType::MBC1_RAM_BATTERY && header.rom_size <= RomSizeType::Kb512_bank32) {
        return std::make_unique<MBC1_0>(header.rom_size, header.ram_size, data);
    }
    if (header.cartridge_type == CartridgeType::MBC3 || header.cartridge_type == CartridgeType::MBC3_RAM ||
        header.cartridge_type == CartridgeType::MBC3_RAM_BATTERY) {
        return std::make_unique<MBC3>(header.rom_size, header.ram_size, data);
    }
    if (header.cartridge_type == CartridgeType::MBC5 || header.cartridge_type == CartridgeType::MBC5_RAM ||
        header.cartridge_type == CartridgeType::MBC5_RAM_BATTERY ||
        header.cartridge_type == CartridgeType::MBC5_RUMBLE ||
        header.cartridge_type == CartridgeType::MBC5_RUMBLE_RAM ||
        header.cartridge_type == CartridgeType::MBC5_RUMBLE_RAM_BATTERY) {
        return std::make_unique<MBC5>(header.rom_size, header.ram_size, data);
    }

    throw std::runtime_error(fmt::format("Cartridge::make_mapper: unknown cartridge type {}, ROM size {}, RAM size {}",
                                         magic_enum::enum_name(header.cartridge_type),
                                         magic_enum::enum_name(header.rom_size),
                                         magic_enum::enum_name(header.ram_size)));
}
