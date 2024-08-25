#pragma once
#include <span>
#include <vector>

#include "../../types.hpp"
#include "../cartridge_header.hpp"

namespace bemu::gb {
struct IMapper {
    virtual ~IMapper() = default;
    virtual u8 read_ram(u16 address) const = 0;
    virtual void write_ram(u16 address, u8 value) = 0;
    virtual u8 read_rom(u16 address) const = 0;
    virtual void write_rom(u16 address, u8 value) = 0;
};

struct BaseMapper : IMapper {
    BaseMapper(const RomSizeType rom_size, const RamSizeType ram_size, std::vector<u8>& data)
        : m_num_rom_banks(num_rom_banks(rom_size)), m_num_ram_banks(num_ram_banks(ram_size)), m_data(data) {
        // Each RAM bank is 8KB, initialize to 0
        m_ram.resize(num_ram_banks(ram_size) * 8 * 1024, 0x00);
    }

    virtual size_t ram_address_to_index(const u16 address) const {
        return address - 0xA000 + (m_ram_bank_number % m_num_ram_banks) * 8 * 1024;
    }

    u8 read_ram(const u16 address) const override {
        if (m_ram.empty()) {
            return 0xFF;  // No RAM present
        }

        return m_ram.at(ram_address_to_index(address));
    }

    void write_ram(const u16 address, const u8 value) override {
        if (m_ram.empty()) {
            return;  // No RAM present
        }

        m_ram.at(ram_address_to_index(address)) = value;
    }

    std::span<u8> get_ram() { return m_ram; }

    void serialize(auto& ar) {
        ar(m_ram_bank_number);
        ar(m_ram);
    }

   protected:
    u8 m_ram_bank_number = 0;

    u8 m_num_rom_banks = 0;
    u8 m_num_ram_banks = 0;
    std::vector<u8>& m_data;
    std::vector<u8> m_ram;
};

struct MBC0 : BaseMapper {
    using BaseMapper::BaseMapper;

    u8 read_rom(const u16 address) const override { return m_data.at(address); }
    void write_rom(const u16 address, const u8 value) override {}
};
}  // namespace bemu::gb