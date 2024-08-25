#pragma once

#include "lcd.hpp"
#include "types.hpp"

namespace bemu::gb {
struct Io {
    Lcd m_lcd;

    /// FF0F - IF: Interrupt flag
    ///
    /// Bit 0: VBlank
    /// Bit 1: LCD
    /// Bit 2: Timer
    /// Bit 3: Serial
    /// Bit 4: Joypad
    u8 m_interrupts = 0;

    /// FF41 - STAT: LCD Status
    u8 m_lcd_status = 0;

    [[nodiscard]] u8 read(u16 address) const;
    void write(u16 address, u8 value);
};
}  // namespace bemu::gb