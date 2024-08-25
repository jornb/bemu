#pragma once
#include <memory>
#include <string>
#include <vector>

#include "cartridge_header.hpp"
#include "mappers/MBC0.hpp"
#include "mappers/MBC1_0.hpp"
#include "mappers/MBC3.hpp"
#include "mappers/MBC5.hpp"
#include "ram.hpp"

namespace bemu::gb {
struct IMapper;

struct Cartridge : IMemoryRegion {
    ~Cartridge() override;

    static std::unique_ptr<Cartridge> from_file(const std::string& filename);
    static std::unique_ptr<Cartridge> from_program_code(const std::vector<u8>& data);

    [[nodiscard]] const CartridgeHeader& header() const;

    [[nodiscard]] bool contains(u16 address) const override;
    [[nodiscard]] u8 read(u16 address) const override;
    void write(u16 address, u8 value) override;

    static std::unique_ptr<IMapper> make_mapper(const CartridgeHeader& header, std::vector<u8>& data);

    void serialize(auto& ar) {
        if (auto mapper = dynamic_cast<MBC0*>(m_mapper.get())) {
            mapper->serialize(ar);
        } else if (auto mapper = dynamic_cast<MBC1_0*>(m_mapper.get())) {
            mapper->serialize(ar);
        } else if (auto mapper = dynamic_cast<MBC3*>(m_mapper.get())) {
            mapper->serialize(ar);
        } else if (auto mapper = dynamic_cast<MBC5*>(m_mapper.get())) {
            mapper->serialize(ar);
        }
    }

   private:
    std::unique_ptr<std::vector<u8>> m_data = std::make_unique<std::vector<u8>>();
    std::unique_ptr<IMapper> m_mapper;
};

}  // namespace bemu::gb
