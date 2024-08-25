#include <spdlog/fmt/fmt.h>

#include <bemu/gb/io.hpp>
#include <stdexcept>

using namespace bemu::gb;

u8 Io::read(u16 address) const {
    if (address == 0xFF0F) {
        return m_interrupts;
    }

    if (0xFF40 <= address && address <= 0xFF4B) {
        // LCD
        const auto lcd_ptr = reinterpret_cast<const u8*>(&m_lcd);
        return lcd_ptr[address - 0xFF40];
    }

    throw std::runtime_error(fmt::format("Unsupported read to I/O address {:04x}", address));
}

void Io::write(u16 address, const u8 value) {
    if (address == 0xFF0F) {
        m_interrupts = value;
        return;
    }
    if (0xFF40 <= address && address <= 0xFF4B) {
        // LCD
        const auto lcd_ptr = reinterpret_cast<u8*>(&m_lcd);
        lcd_ptr[address - 0xFF40] = value;
        return;
    }

    throw std::runtime_error(fmt::format("Unsupported write to I/O address {:04x}", address));
}