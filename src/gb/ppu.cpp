#include <bemu/gb/bus.hpp>
#include <bemu/gb/ppu.hpp>
#include <stdexcept>

using namespace bemu::gb;

bool DmaState::contains(u16 address) const { return address == 0xFF46; }

u8 DmaState::read(u16) const { return m_written_value; }

void DmaState::write(u16, const u8 value) {
    m_active = true;
    m_written_value = value;
    m_current_byte = 0;
    m_start_delay = 2;
}

void DmaState::cycle_tick() {
    if (!m_active) return;

    if (m_start_delay > 0) {
        --m_start_delay;
        return;
    }

    const auto source_address = 0x100 * m_written_value + m_current_byte;
    const auto destination_address = 0xFE00 + m_current_byte;
    ++m_current_byte;

    const auto data = m_bus.read_u8(source_address, false);
    m_bus.write_u8(destination_address, data, false);
}

bool Ppu::contains(const u16 address) const {
    return m_oam_dma.contains(address) || m_oam.contains(address) || m_vram.contains(address);
}

u8 Ppu::read(const u16 address) const {
    if (m_oam_dma.contains(address)) {
        return m_oam_dma.read(address);
    }

    const auto mode = m_lcd.get_ppu_mode();
    if (mode == PpuMode::Drawing) {
        return 0xFF;
    }

    if (m_oam.contains(address) && mode == PpuMode::OemScan) {
        return 0xFF;
    }

    if (m_oam.contains(address)) {
        return m_oam.read(address);
    }

    if (m_vram.contains(address)) {
        return m_vram.read(address);
    }

    throw std::out_of_range("Out of range");
}

void Ppu::write(const u16 address, const u8 value) {
    if (m_oam_dma.contains(address)) {
        return m_oam_dma.write(address, value);
    }

    const auto mode = m_lcd.get_ppu_mode();
    if (mode == PpuMode::Drawing) {
        return;
    }

    if (m_oam.contains(address) && mode == PpuMode::OemScan) {
        return;
    }

    if (m_oam.contains(address)) {
        return m_oam.write(address, value);
    }

    if (m_vram.contains(address)) {
        return m_vram.write(address, value);
    }
}

void Ppu::dot_tick() {}

void Ppu::cycle_tick() { m_oam_dma.cycle_tick(); }
