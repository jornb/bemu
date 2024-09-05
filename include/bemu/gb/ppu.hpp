
#pragma once

#include "cpu.hpp"
#include "lcd.hpp"
#include "ram.hpp"
#include "types.hpp"

namespace bemu::gb {

struct Ppu;
struct Bus;
struct Cpu;
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
    Cpu &m_cpu;

    explicit Ppu(Bus &bus, Lcd &lcd, Screen &screen, Cpu &cpu) : m_bus(bus), m_lcd(lcd), m_screen(screen), m_cpu(cpu), m_oam_dma(bus) {}

    [[nodiscard]] bool contains(u16 address) const;
    [[nodiscard]] u8 read(u16 address) const;
    void write(u16 address, u8 value);

    void dot_tick();
    void cycle_tick();

private:
    std::optional<PpuMode> dot_tick_handle_and_get_next_mode();
    std::optional<PpuMode> dot_tick_horizontal_blank();
    std::optional<PpuMode> dot_tick_vertical_blank();
    std::optional<PpuMode> dot_tick_oam();
    std::optional<PpuMode> dot_tick_draw();

    [[nodiscard]] u16 get_line_tick() const;
    [[nodiscard]] u16 get_line_number() const;

    RAM<0xFE00, 0xFE9F> m_oam;
    RAM<0x8000, 0x9FFF> m_vram;
    DmaState m_oam_dma;

    u64 m_frame_number = 0;
    u32 m_frame_tick = 0; ///< Dot tick within current frame
    u16 m_remaining_ticks_in_mode = 80;
};

}  // namespace bemu::gb
