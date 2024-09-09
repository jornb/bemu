#pragma once

#include <string>
#include <vector>

#include "cartridge.hpp"
#include "io.hpp"
#include "lcd.hpp"
#include "ppu.hpp"
#include "ram.hpp"
#include "serial.hpp"
#include "timer.hpp"
#include "types.hpp"

namespace bemu::gb {
struct Emulator;
struct Bus {
    Emulator &m_emulator;
    WRAM m_wram{};
    RAM<0xFF80, 0xFFFE> m_hram{};
    Lcd m_lcd{};
    Io m_io{m_lcd};
    RAM<0xFF10, 0xFF26> m_audio{};
    Ppu m_ppu;
    SerialPort m_serial;
    Timer m_timer;

    explicit Bus(Emulator &emulator);

    u8 read_u8(u16 address, bool add_cycles = true) const;
    void write_u8(u16 address, u8 value, bool add_cycles = true);

    u16 read_u16(u16 address, bool add_cycles = true) const;
    void write_u16(u16 address, u16 value, bool add_cycles = true);
};
}  // namespace bemu::gb