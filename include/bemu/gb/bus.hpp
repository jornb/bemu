#pragma once

#include "interfaces.hpp"
#include "joypad.hpp"
#include "lcd.hpp"
#include "memory.hpp"
#include "ppu.hpp"
#include "ram.hpp"
#include "serial.hpp"
#include "timer.hpp"

namespace bemu::gb {
struct Cartridge;
struct Cpu;
struct External;

// 0x0000 - 0x3FFF : ROM Bank 0
// 0x4000 - 0x7FFF : ROM Bank 1 - Switchable
// 0x8000 - 0x97FF : CHR RAM
// 0x9800 - 0x9BFF : BG Map 1
// 0x9C00 - 0x9FFF : BG Map 2
// 0xA000 - 0xBFFF : Cartridge RAM
// 0xC000 - 0xCFFF : RAM Bank 0
// 0xD000 - 0xDFFF : RAM Bank 1-7 - switchable - Color only
// 0xE000 - 0xFDFF : Reserved - Echo RAM
// 0xFE00 - 0xFE9F : Object Attribute Memory
// 0xFEA0 - 0xFEFF : Reserved - Unusable
// 0xFF00 - 0xFF7F : I/O Registers
// 0xFF80 - 0xFFFE : Zero Page

struct Bus : MemoryBus {
    Lcd m_lcd;
    Joypad m_joypad;
    Ppu m_ppu;
    Timer m_timer;
    RAM<0xC000, 0xCFFF> m_wram_fixed;
    WRAM m_wram;
    RAM<0xFF80, 0xFFFE> m_hram;
    RAM<0xFF10, 0xFF26> m_audio;         // Audio (ignored)
    RAM<0xFF30, 0xFF3F> m_wave_pattern;  // Wave pattern (ignored)
    SerialPort m_serial;
    NoopRegion<0xE000, 0xFDFF> m_reserved_echo;    // Reserved - Echo RAM
    NoopRegion<0xFEA0, 0xFEFF> m_reserved_unused;  // Reserved - Unusable

    explicit Bus(ICycler &cycler, Cpu &cpu, Cartridge &cartridge, External &external);

    void serialize(auto &ar) {
        m_lcd.serialize(ar);
        m_joypad.serialize(ar);
        m_ppu.serialize(ar);
        m_timer.serialize(ar);
        m_wram_fixed.serialize(ar);
        m_wram.serialize(ar);
        m_hram.serialize(ar);
        m_audio.serialize(ar);
        m_wave_pattern.serialize(ar);
        m_serial.serialize(ar);
        m_serial.serialize(ar);
    }
};
}  // namespace bemu::gb