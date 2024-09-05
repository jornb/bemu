#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <bemu/gb/io.hpp>
#include <stdexcept>

#include "spdlog/spdlog.h"

using namespace bemu::gb;

u8 Io::read(u16 address) const {
    if (address == 0xFF00) {
        // auto select_buttons = get_bit(m_joypad, 5);
        // auto select_pad = get_bit(m_joypad, 4);

        // TODO: Return buttons. If both buttons are selected, the low nibble reads 0xF (all buttons released)
        //       All buttons released (for now)

        return m_joypad & 0xF0 | 0x0F;
    }

    if (address == 0xFF01) {
        return m_serial_data;
    }

    if (address == 0xFF02) {
        return m_serial_control;
    }

    if (0xFF04 <= address && address <= 0xFF07) {
        return m_timer.read(address);
    }

    if (0xFF40 <= address && address <= 0xFF4B) {
        // LCD
        const auto lcd_ptr = reinterpret_cast<const u8*>(&m_lcd);
        return lcd_ptr[address - 0xFF40];
    }

    spdlog::warn("Unsupported read to I/O address {:04x}", address);
    return 0x00;

    // throw std::runtime_error(fmt::format("Unsupported read to I/O address {:04x}", address));
}

void Io::write(u16 address, const u8 value) {
    if (address == 0xFF00) {
        // The lower nibble is read-only, so only write the top
        m_joypad = value & 0xF0 | m_joypad & 0x0F;

        // Top bits are not used, but seem to be set to 1?
        m_joypad |= 0b11000000;

        return;
    }

    if (address == 0xFF01) {
        m_serial_data = value;
        return;
    }

    if (address == 0xFF02) {
        m_serial_control = value;
        return;
    }

    if (0xFF04 <= address && address <= 0xFF07) {
        return m_timer.write(address, value);
    }

    if (0xFF40 <= address && address <= 0xFF4B) {
        // LCD
        const auto lcd_ptr = reinterpret_cast<u8*>(&m_lcd);
        lcd_ptr[address - 0xFF40] = value;
        return;
    }

    spdlog::warn("Unsupported write to I/O address {:04x} = {:02x}", address, value);
    // throw std::runtime_error(fmt::format("Unsupported write to I/O address {:04x}", address));
}