
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

#pragma pack(push, 1)
struct OamEntry {
    /// Byte 0 - Y Position
    ///
    /// Object’s vertical position on the screen + 16. So for example:
    ///     * Y = 0 hides an object
    ///     * Y = 2 hides an 8× 8 object but displays the last two rows of an 8× 16 object
    ///     * Y = 16 displays an object at the top of the screen
    ///     * Y = 144 displays an 8× 16 object aligned with the bottom of the screen
    ///     * Y = 152 displays an 8× 8 object aligned with the bottom of the screen
    ///     * Y = 154 displays the first six rows of an object at the bottom of the screen, Y = 160 hides an object
    u8 m_y = 0;

    /// Byte 1 - X Position
    ///
    /// Object’s horizontal position on the screen + 8. This works similarly to the examples above, except that the
    /// width of an object is always 8. An off-screen value (X=0 or X>=168) hides the object, but the object still
    /// contributes to the limit of ten objects per scanline. This can cause objects later in OAM not to be drawn on
    /// that line. A better way to hide an object is to set its Y-coordinate off-screen.
    u8 m_x = 0;

    /// Byte 2 - Tile Index
    ///
    /// In 8x8 mode (LCDC bit 2 = 0), this byte specifies the object’s only tile index ($00-$FF). This unsigned value
    /// selects a tile from the memory area at $8000-$8FFF. In CGB Mode this could be either in VRAM bank 0 or 1,
    /// depending on bit 3 of the following byte. In 8×16 mode (LCDC bit 2 = 1), the memory area at $8000-$8FFF is still
    /// interpreted as a series of 8×8 tiles, where every 2 tiles form an object. In this mode, this byte specifies the
    /// index of the first (top) tile of the object. This is enforced by the hardware: the least significant bit of the
    /// tile index is ignored; that is, the top 8x8 tile is “NN & $FE”, and the bottom 8×8 tile is “NN | $01”.
    u8 m_tile_index = 0;

    u8 m_flags = 0;

    /// GBC only: Which of OBP0-7 to use
    [[nodiscard]] u8 get_palette() const { return m_flags & 0b111; }

    /// GBC only: VRAM bank 0 or 1
    [[nodiscard]] u8 get_bank() const { return get_bit(m_flags, 3); }

    /// Non-GBC only: OBP0 (0) or OBP1 (1)
    [[nodiscard]] u8 get_dmg_palette() const { return get_bit(m_flags, 4); }

    /// If set, objects horizontally mirrored
    [[nodiscard]] bool get_x_flip() const { return get_bit(m_flags, 5); }

    /// If set, objects vertically mirrored
    [[nodiscard]] bool get_y_flip() const { return get_bit(m_flags, 6); }

    /// If set, BG and Window colors 1-3 are drawn over this OBJ
    [[nodiscard]] bool background_has_priority() const { return get_bit(m_flags, 7); }
};

struct OamRam : MemoryRegion<0xFE00, OamRam> {
    std::array<OamEntry, 40> m_entries;
};
#pragma pack(pop)

struct Ppu {
    Bus &m_bus;
    Lcd &m_lcd;
    Screen &m_screen;
    Cpu &m_cpu;

    explicit Ppu(Bus &bus, Lcd &lcd, Screen &screen, Cpu &cpu)
        : m_bus(bus), m_lcd(lcd), m_screen(screen), m_cpu(cpu), m_oam_dma(bus) {}

    [[nodiscard]] bool contains(u16 address) const;
    [[nodiscard]] u8 read(u16 address) const;
    void write(u16 address, u8 value);

    void dot_tick();
    void cycle_tick();

   private:
    void dot_tick_handle_and_get_next_mode();
    std::optional<PpuMode> dot_tick_horizontal_blank();
    std::optional<PpuMode> dot_tick_vertical_blank();
    std::optional<PpuMode> dot_tick_oam();
    std::optional<PpuMode> dot_tick_draw();

    void render_scanline();
    void render_scanline_background();
    void render_scanline_window();
    void render_scanline_objects();

    [[nodiscard]] u16 get_line_tick() const;
    [[nodiscard]] u16 get_line_number() const;

    /// Populates objects for the current line. Maximum of 10.
    /// Loaded sequentially from m_oam. Only Y coordinate is considered.
    ///
    /// Sorted by x coordinate
    std::vector<const OamEntry *> load_line_objects();

    OamRam m_oam;
    RAM<0x8000, 0x9FFF> m_vram;
    DmaState m_oam_dma;

    u64 m_frame_number = 0;
    u32 m_frame_tick = -1;  ///< Dot tick within current frame
};

}  // namespace bemu::gb
