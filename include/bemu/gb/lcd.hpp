
#pragma once

#include "lcd.hpp"
#include "types.hpp"

namespace bemu::gb {

#pragma pack(push, 1)
struct Lcd {
    /// FF40 - LCDC: LCD control
    /// Bit 0: BG & Window enable / priority [Different meaning in CGB Mode]: 0 = Off; 1 = On
    /// Bit 1: OBJ enable
    /// Bit 2: OBJ size: 0 = 8×8; 1 = 8×16
    /// Bit 3: BG tile map area: 0 = 9800–9BFF; 1 = 9C00–9FFF
    /// Bit 4: BG & Window tile data area: 0 = 8800–97FF; 1 = 8000–8FFF
    /// Bit 5: Window enable: 0 = Off; 1 = On
    /// Bit 6: Window tile map area: 0 = 9800–9BFF; 1 = 9C00–9FFF
    /// Bit 7: LCD & PPU enable: 0 = Off; 1 = On
    u8 m_control;

    u8 m_status;
    u8 scroll_y;
    u8 scroll_x;

    /// FF44 - LY: LCD Y coordinate [read-only]
    /// LY indicates the current horizontal line, which might be about to be drawn, being drawn, or just been drawn. LY
    /// can hold any value from 0 to 153, with values from 144 to 153 indicating the VBlank period.
    u8 ly;

    /// FF45 - LYC: LY compare
    /// The Game Boy constantly compares the value of the LYC and LY registers. When both values are identical, the
    /// “LYC=LY” flag in the STAT register is set, and (if enabled) a STAT interrupt is requested.
    u8 ly_compare;

    u8 dma;
    u8 bg_palette;
    u8 obj_palette[2];
    u8 win_y;
    u8 win_x;

    // other data...
    u32 bg_colors[4];
    u32 sp1_colors[4];
    u32 sp2_colors[4];
};
#pragma pack(pop)

}  // namespace bemu::gb
