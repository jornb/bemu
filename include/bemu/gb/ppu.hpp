
#pragma once

#include "lcd.hpp"
#include "ram.hpp"
#include "types.hpp"

namespace bemu::gb {

struct Ppu;
struct Bus;

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

    explicit Ppu(Bus &bus, Lcd &lcd) : m_bus(bus), m_lcd(lcd), m_oam_dma(bus) {}

    enum class Mode : uint8_t {
        /// Waiting until the end of the scanline.
        ///
        /// Duration: 376 - Drawing mode duration
        /// Accessible video memory: VRAM, OAM, CGB palettes
        HorizontalBlank,

        /// Waiting until the next frame
        ///
        /// Duration: 4560 dots (10 scanlines)
        /// Accessible video memory: VRAM, OAM, CGB palettes
        VerticalBlank,

        /// Searching for OBJs which overlap this line
        ///
        /// Duration: 10 dots
        /// Accessible video memory: VRAM, CGB palettes
        OemScan,

        /// Sending pixels to the LCD
        ///
        /// Duration: 172 - 289 dots
        /// Accessible video memory: None
        Drawing
    };

    [[nodiscard]] bool contains(u16 address) const;
    [[nodiscard]] u8 read(u16 address) const;
    void write(u16 address, u8 value);

    void dot_tick();
    void cycle_tick();

    Mode m_mode = Mode::OemScan;

    RAM<0xFE00, 0xFE9F> m_oam;
    RAM<0x8000, 0x9FFF> m_vram;
    DmaState m_oam_dma;
};

}  // namespace bemu::gb
