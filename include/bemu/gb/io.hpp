#pragma once

#include "lcd.hpp"
#include "timer.hpp"
#include "types.hpp"

namespace bemu::gb {

struct Io {
    Timer m_timer;
    Lcd m_lcd{};

    /// FF00 - P1/JOYP: Joypad
    u8 m_joypad = 0;

    /// FF01 - SB: Serial transfer data
    u8 m_serial_data = 0;
    u8 m_serial_control = 0;

    /// FF0F - IF: Interrupt flag
    ///
    /// Bit 0: VBlank
    /// Bit 1: LCD
    /// Bit 2: Timer
    /// Bit 3: Serial
    /// Bit 4: Joypad
    u8 m_interrupts = 0;

    [[nodiscard]] u8 read(u16 address) const;
    void write(u16 address, u8 value);
};
}  // namespace bemu::gb