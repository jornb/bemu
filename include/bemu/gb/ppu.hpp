
#pragma once

#include "lcd.hpp"
#include "ram.hpp"
#include "types.hpp"

namespace bemu::gb {

struct Ppu;
struct Bus;
struct Screen;

struct DmaState {
    Bus &m_bus;

    [[nodiscard]] bool contains(u16 address) const;
    [[nodiscard]] u8 read(u16 address) const;
    void write(u16 address, u8 value);

    void cycle_tick();

    bool m_active = false;
    u8 m_start_delay = 0;
    u8 m_written_value = 0;
    u8 m_current_byte = 0;
    bool m_transferring = false;
};

struct Ppu {
    Bus &m_bus;
    Lcd &m_lcd;
    Screen &m_screen;

    explicit Ppu(Bus &bus, Lcd &lcd, Screen &screen) : m_bus(bus), m_lcd(lcd), m_screen(screen), m_oam_dma(bus) {}

    [[nodiscard]] bool contains(u16 address) const;
    [[nodiscard]] u8 read(u16 address) const;
    void write(u16 address, u8 value);

    void dot_tick();
    void cycle_tick();

    RAM<0xFE00, 0xFE9F> m_oam;
    RAM<0x8000, 0x9FFF> m_vram;
    DmaState m_oam_dma;
};

}  // namespace bemu::gb
