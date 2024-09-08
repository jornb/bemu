#pragma once
#include <spdlog/spdlog.h>

#include "types.hpp"
#include "utils.hpp"

namespace bemu::gb {
#pragma pack(push, 1)
struct SerialPort : MemoryRegion<0xFF01, SerialPort> {
    u8 m_data;
    u8 control;
    void write_memory(const u16 address, const u8 value) {

        static std::string s;

        MemoryRegion::write_memory(address, value);

        if (address == 0xFF02 && get_bit(value, 7)) {
            s += static_cast<char>(m_data);
            spdlog::warn("Serial port data: {:02x} {}: {}", m_data, static_cast<char>(m_data), s);
        }
    }
};
#pragma pack(pop)
}  // namespace bemu::gb
