#pragma once
#include <spdlog/spdlog.h>

#include "../types.hpp"
#include "../utils.hpp"
#include "external.hpp"

namespace bemu::gb {
#pragma pack(push, 1)
struct SerialRegisters {
    u8 m_data;
    u8 control;
};
#pragma pack(pop)

struct SerialPort : MemoryRegion<0xFF01, SerialRegisters> {
    External &m_external;

    explicit SerialPort(External &external) : m_external{external} {}

    void write(const u16 address, const u8 value) override {
        MemoryRegion::write(address, value);

        if (address == 0xFF02 && get_bit(value, 7)) {
            m_external.m_serial_data_received.emplace_back(m_data.m_data);
        }
    }
};
}  // namespace bemu::gb
