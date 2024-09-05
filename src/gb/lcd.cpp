#include <bemu/gb/lcd.hpp>

using namespace bemu::gb;

static u32 colors_default[4] = {0xFFFFFFFF, 0xFFAAAAAA, 0xFF555555, 0xFF000000};

Lcd::Lcd() {
    for (int i = 0; i < 4; i++) {
        bg_colors[i] = colors_default[i];
        sp1_colors[i] = colors_default[i];
        sp2_colors[i] = colors_default[i];
    }
}


bool Lcd::contains(u16 address) const {
    return 0xFF40 <= address && address <= 0xFF40 + sizeof(Lcd);
}

u8 Lcd::read_memory(u16 address) const {
    const u16 offset = address - 0xFF40;
    const auto ptr = reinterpret_cast<const u8*>(this);
    return ptr[offset];
}

// void update_palette(u8 palette_data, u8 pal) {
//     u32 *p_colors = ctx.bg_colors;
//
//     switch(pal) {
//         case 1:
//             p_colors = ctx.sp1_colors;
//         break;
//         case 2:
//             p_colors = ctx.sp2_colors;
//         break;
//     }
//
//     p_colors[0] = colors_default[palette_data & 0b11];
//     p_colors[1] = colors_default[(palette_data >> 2) & 0b11];
//     p_colors[2] = colors_default[(palette_data >> 4) & 0b11];
//     p_colors[3] = colors_default[(palette_data >> 6) & 0b11];
// }

void Lcd::write_memory(const u16 address, const u8 value) {
    if (address == 0xFF41) {
        // Two lower bits (LYC==LY and PPU mode) are read-only
        m_status |= value & 0b11111100;
        return;
    }

    const u16 offset = address - 0xFF40;
    const auto ptr = reinterpret_cast<u8*>(this);
    ptr[offset] = value;

    // if (address == 0xFF47) {
    //     update_palette(value, 0);
    // } else if (address == 0xFF48) {
    //     update_palette(value & 0b11111100, 1);
    // } else if (address == 0xFF49) {
    //     update_palette(value & 0b11111100, 2);
    // }
}

