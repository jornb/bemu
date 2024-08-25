
#pragma once

#include <chrono>

#include "../types.hpp"
#include "ram.hpp"

namespace bemu::gb {

struct Bus;
struct Cpu;
struct External;
struct Lcd;
struct Ppu;

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

    [[nodiscard]] int get_screen_x() const { return m_x - 8; }
    [[nodiscard]] int get_screen_y() const { return m_y - 16; }

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
#pragma pack(pop)

#pragma pack(push, 1)
struct OamRamData {
    std::array<OamEntry, 40> m_entries;
};
#pragma pack(pop)
static_assert(sizeof(OamRamData) == 40 * sizeof(OamEntry));

struct OamRam : MemoryRegion<0xFE00, OamRamData> {};

/// Handler for OAM DMA transfers, controlled by register 0xFF46
///
/// FF46 - DMA: OAM DMA source address & start
///
/// Writing to this register starts a DMA transfer from ROM or RAM to OAM (Object Attribute Memory). The written
/// value specifies the transfer source address divided by $100, that is, source and destination are:
///
///     Source:      $XX00-$XX9F   ;XX = $00 to $DF
///     Destination: $FE00-$FE9F
///
/// The transfer takes 160 M-cycles: 640 dots (1.4 lines) in normal speed, or 320 dots (0.7 lines) in CGB Double
/// Speed Mode. This is much faster than a CPU-driven copy.
struct DmaState {
    Bus &m_bus;
    OamRam &m_oam;  ///< DMA can access the OAM, regardless of PPU state

    [[nodiscard]] bool contains(u16 address) const;
    [[nodiscard]] u8 read(u16 address) const;
    void write(u16 address, u8 value);

    void cycle_tick();

    bool m_active = false;
    u8 m_start_delay = 0;
    u8 m_written_value = 0;
    u8 m_current_byte = 0;
    bool m_transferring = false;

    void serialize(auto &ar) {
        ar(m_active);
        ar(m_start_delay);
        ar(m_written_value);
        ar(m_current_byte);
        ar(m_transferring);
    }
};

struct Ppu : IMemoryRegion, ICycled {
    External &m_external;
    Bus &m_bus;
    Lcd &m_lcd;
    Cpu &m_cpu;
    RAM<0x8000, 0x9FFF> m_vram{};
    OamRam m_oam;
    DmaState m_oam_dma;
    u32 m_frame_tick = 0;  ///< Dot tick within current frame

    explicit Ppu(External &external, Bus &bus, Lcd &lcd, Cpu &cpu);

    [[nodiscard]] bool contains(u16 address) const override;
    [[nodiscard]] u8 read(u16 address) const override;
    void write(u16 address, u8 value) override;

    void dot_tick() override;
    void cycle_tick() override;

    void serialize(auto &ar) {
        m_vram.serialize(ar);
        m_oam.serialize(ar);
        m_oam_dma.serialize(ar);
        ar(m_frame_tick);
    }

   private:
    void dot_tick_handle_and_get_next_mode();
    std::optional<PpuMode> dot_tick_horizontal_blank();
    std::optional<PpuMode> dot_tick_vertical_blank();
    std::optional<PpuMode> dot_tick_oam();
    std::optional<PpuMode> dot_tick_draw();

    void render_scanline();
    void render_scanline_from_tilemap(int screen_y, int start_x, int offset_x, int offset_y, u16 tile_set_address,
                                      u16 tile_map_address, u8 palette);
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
};

}  // namespace bemu::gb
