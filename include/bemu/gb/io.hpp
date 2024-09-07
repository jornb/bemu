#pragma once

#include "lcd.hpp"
#include "timer.hpp"
#include "types.hpp"

namespace bemu::gb {

struct Io {
    Lcd &m_lcd;
    Timer m_timer;

    /// FF00 - P1/JOYP: Joypad
    ///
    /// The eight Game Boy action/direction buttons are arranged as a 2×4 matrix. Select either action or direction
    /// buttons by writing to this register, then read out the bits 0-3.
    ///
    /// The lower nibble is Read-only. Note that, rather unconventionally for the Game Boy, a button being pressed is
    /// seen as the corresponding bit being 0, not 1.
    u8 m_joypad = 0x0;

    /// FF01 - SB: Serial transfer data
    u8 m_serial_data = 0;
    u8 m_serial_control = 0;

    [[nodiscard]] u8 read(u16 address) const;
    void write(u16 address, u8 value);
};
}  // namespace bemu::gb