#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <bemu/gb/io.hpp>
#include <stdexcept>

#include "spdlog/spdlog.h"

using namespace bemu::gb;

u8 Io::read(u16 address) const {
    if (address == 0xFF00) {
        const auto select_buttons = get_bit(m_joypad, 5);
        const auto select_pad = get_bit(m_joypad, 4);

        if (select_buttons & select_pad) {
            // If both buttons are selected, the low nibble reads 0xF (all buttons released)
            return m_joypad & 0xF0 | 0x0F;
        }

        // TODO: Return actual buttons. Thse are all released.
        return m_joypad & 0xF0 | 0x0F;
    }

    if (m_lcd.contains(address)) {
        return m_lcd.read_memory(address);
    }

    spdlog::warn("Unsupported read to I/O address {:04x}", address);
    return 0x00;

    // throw std::runtime_error(fmt::format("Unsupported read to I/O address {:04x}", address));
}

void Io::write(u16 address, const u8 value) {
    if (address == 0xFF00) {
        // The lower nibble is read-only, so only write the top
        m_joypad = value & 0xF0 | m_joypad & 0x0F;

        // // Top bits are not used, but seem to be set to 1?
        // m_joypad |= 0b11000000;

        return;
    }

    if (m_lcd.contains(address)) {
        return m_lcd.write_memory(address, value);
    }

    spdlog::warn("Unsupported write to I/O address {:04x} = {:02x}", address, value);
    // throw std::runtime_error(fmt::format("Unsupported write to I/O address {:04x}", address));
}