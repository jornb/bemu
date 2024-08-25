#include <bemu/gb/lcd.hpp>

using namespace bemu;
using namespace bemu::gb;

void Lcd::write(const u16 address, const u8 value) {
    if (address == 0xFF41) {
        // Two lower bits (LYC==LY and PPU mode) are read-only
        m_data.m_status |= value & 0b11111100;
        return;
    }

    MemoryRegion::write(address, value);
}
