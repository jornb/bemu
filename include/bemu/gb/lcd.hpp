
#pragma once

#include "lcd.hpp"
#include "ram.hpp"
#include "types.hpp"
#include "utils.hpp"

namespace bemu::gb {

enum class PpuMode : u8 {
    /// Waiting until the end of the scanline.
    ///
    /// Duration: 376 - Drawing mode duration
    /// Accessible video memory: VRAM, OAM, CGB palettes
    HorizontalBlank = 0b00,

    /// Waiting until the next frame
    ///
    /// Duration: 4560 dots (10 scanlines)
    /// Accessible video memory: VRAM, OAM, CGB palettes
    VerticalBlank = 0b01,

    /// Searching for OBJs which overlap this line
    ///
    /// Duration: 10 dots
    /// Accessible video memory: VRAM, CGB palettes
    OamScan = 0b10,

    /// Sending pixels to the LCD
    ///
    /// Duration: 172 - 289 dots
    /// Accessible video memory: None
    Drawing
};

#pragma pack(push, 1)
struct Lcd : MemoryRegion<0xFF40, Lcd> {
    explicit Lcd();

    /// FF40 - LCDC: LCD control
    ///
    /// LCDC is the main LCD Control register. Its bits toggle what elements are displayed on the screen, and how.
    u8 m_control = 0x91;

    /// FF41 - STAT: LCD status
    u8 m_status = static_cast<u8>(PpuMode::OamScan);  // Start in OAM scan mode

    /// FF42, FF43 - SCY, SCX: Background viewport Y position, X position
    u8 scroll_y = 0;
    u8 scroll_x = 0;

    /// FF44 - LY: LCD Y coordinate [read-only]
    /// LY indicates the current horizontal line, which might be about to be drawn, being drawn, or just been drawn. LY
    /// can hold any value from 0 to 153, with values from 144 to 153 indicating the VBlank period.
    u8 ly = 0;

    /// FF45 - LYC: LY compare
    /// The Game Boy constantly compares the value of the LYC and LY registers. When both values are identical, the
    /// “LYC=LY” flag in the STAT register is set, and (if enabled) a STAT interrupt is requested.
    u8 ly_compare = 0;

    u8 dma = 0;

    /// FF47 - BGP (Non-CGB Mode only): BG palette data
    ///
    /// This register assigns gray shades to the color IDs of the BG and Window tiles.
    ///           | 7  6 | 5  4 | 3  2 | 1  0 |
    /// Color for | ID 3 | ID 2 | ID 1 | ID 0 |
    ///
    /// Each of the two-bit values map to a color thusly:
    ///     * 0: White
    ///     * 1: Light gray
    ///     * 2: Dark gray
    ///     * 3: Black
    ///
    /// In CGB Mode the color palettes are taken from CGB palette memory instead.
    u8 bg_palette = 0xFC;

    /// FF48 - FF49: OBP0, OBP1 (Non-CGB Mode only): OBJ palette 0, 1 data
    /// These registers assigns gray shades to the color indexes of the OBJs that use the corresponding palette. They
    /// work exactly like BGP, except that the lower two bits are ignored because color index 0 is transparent for OBJs.
    u8 obj_palette[2] = {0xFF, 0xFF};
    u8 window_y = 0;
    u8 window_x = 0;

    // other data...
    u32 bg_colors[4];
    u32 sp1_colors[4];
    u32 sp2_colors[4];

    void write_memory(u16 address, u8 value);

    /// When Bit 0 is cleared, both background and window become blank (white), and the Window Display Bit is ignored in
    /// that case. Only objects may still be displayed (if enabled in Bit 1).
    ///
    /// TODO GBC: Different meaning
    [[nodiscard]] bool get_background_and_window_enable() const { return get_bit(m_control, 0); }

    /// This bit toggles whether objects are displayed or not.
    ///
    /// This can be toggled mid-frame, for example to avoid objects being displayed on top of a status bar or text box.
    [[nodiscard]] bool get_object_enable() const { return get_bit(m_control, 1); }

    /// This bit controls the size of all objects (1 tile or 2 stacked vertically).
    ///
    /// Be cautious when changing object size mid-frame. Changing from 8x8 to 8x16 pixels mid-frame within 8 scanlines
    /// of the bottom of an object causes the object’s second tile to be visible for the rest of those 8 lines. If the
    /// size is changed during mode 2 or 3, remnants of objects in range could “leak” into the other tile and cause
    /// artifacts.
    ///
    /// \return 16 or 8
    [[nodiscard]] u8 get_object_height() const { return get_bit(m_control, 2) ? 16 : 8; }

    /// This bit controls which background map the background uses for rendering. If the bit is clear (0), the BG uses
    /// tilemap $9800, otherwise tilemap $9C00.
    ///
    /// The map is a row-major 32x32 list of u8 tile IDs, creating a 256x256 image.
    [[nodiscard]] u16 get_background_tile_map_start_address() const { return get_bit(m_control, 3) ? 0x9C00 : 0x9800; }

    /// This bit controls which addressing mode the BG and Window use to pick tiles.
    ///
    /// Points to the actual 8x8 tiles.
    ///
    /// Objects (sprites) aren’t affected by this, and will always use the $8000 addressing mode.
    [[nodiscard]] u16 get_background_and_window_tile_data_start_address() const {
        return get_bit(m_control, 4) ? 0x8000 : 0x8800;
    }

    /// This bit controls whether the window shall be displayed or not. This bit is overridden on DMG by bit 0 if that
    /// bit is clear.
    ///
    /// Changing the value of this register mid-frame triggers a more complex behaviour: see further below.
    ///
    /// TODO GBC: Note that on CGB models, setting this bit to 0 then back to 1 mid-frame may cause the second write to
    ///           be ignored.
    [[nodiscard]] bool get_window_enable() const { return get_bit(m_control, 5); }

    /// This bit controls which background map the Window uses for rendering. When it’s clear (0), the $9800 tilemap is
    /// used, otherwise it’s the $9C00 one.
    ///
    /// The map is a row-major 32x32 list of u8 tile IDs, creating a 256x256 image.
    [[nodiscard]] u16 get_window_tile_map_start_address() const { return get_bit(m_control, 6) ? 0x9C00 : 0x9800; }

    /// This bit controls whether the LCD is on and the PPU is active. Setting it to 0 turns both off, which grants
    /// immediate and full access to VRAM, OAM, etc.
    ///
    /// When re-enabling the LCD, the PPU will immediately start drawing again, but the screen will stay blank during
    /// the first frame.
    [[nodiscard]] bool get_enable_lcd_and_ppu() const { return get_bit(m_control, 7); }

    PpuMode get_ppu_mode() const { return static_cast<PpuMode>(m_status & 0b11); }
    void set_ppu_mode(PpuMode mode) { m_status = (m_status & ~0b11) | static_cast<u8>(mode); }

    [[nodiscard]] bool is_horizontal_blank_interrupt_enabled() const { return get_bit(m_status, 3); }
    [[nodiscard]] bool is_vertical_blank_interrupt_enabled() const { return get_bit(m_status, 4); }
    [[nodiscard]] bool is_oam_interrupt_enabled() const { return get_bit(m_status, 5); }
    [[nodiscard]] bool is_ly_compare_interrupt_enabled() const { return get_bit(m_status, 6); }
};
#pragma pack(pop)

}  // namespace bemu::gb
