#include <bemu/gb/bus.hpp>
#include <bemu/gb/ppu.hpp>
#include <stdexcept>

#include <spdlog/spdlog.h>

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

    if (m_oam.contains(address) && mode == PpuMode::OamScan) {
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

    if (m_oam.contains(address) && mode == PpuMode::OamScan) {
        return;
    }

    if (m_oam.contains(address)) {
        return m_oam.write(address, value);
    }

    if (m_vram.contains(address)) {
        return m_vram.write(address, value);
    }
}

void Ppu::dot_tick() {
    const auto next_mode = dot_tick_handle_and_get_next_mode();
    if (next_mode.has_value()) {
        m_lcd.set_ppu_mode(*next_mode);

        // Signal interrupts
        if (next_mode == PpuMode::VerticalBlank) {
            m_cpu.set_pending_interrupt(InterruptType::VBlank);

            if (m_lcd.is_vertical_blank_interrupt_enabled()) {
                m_cpu.set_pending_interrupt(InterruptType::LCD);
            }
        } else if (next_mode == PpuMode::HorizontalBlank) {
            if (m_lcd.is_horizontal_blank_interrupt_enabled()) {
                m_cpu.set_pending_interrupt(InterruptType::LCD);
            }
        } else if (next_mode == PpuMode::OamScan) {
            if (m_lcd.is_oam_interrupt_enabled()) {
                m_cpu.set_pending_interrupt(InterruptType::LCD);
            }
        }
    }

    m_frame_tick++;

    // Enter new frame
    if (next_mode == PpuMode::OamScan) {
        m_frame_number++;
        m_frame_tick = 0;
    }

    // Check for ly compare
    m_lcd.ly = get_line_number();

    if (get_line_tick() == 0)
        spdlog::info("ly = {}", m_lcd.ly);

    // Check ly compare every new line
    if (get_line_tick() == 0 && m_lcd.ly == m_lcd.ly_compare && m_lcd.is_ly_compare_interrupt_enabled()) {
        m_cpu.set_pending_interrupt(InterruptType::LCD);
    }
}

void Ppu::cycle_tick() { m_oam_dma.cycle_tick(); }

std::optional<PpuMode> Ppu::dot_tick_handle_and_get_next_mode() {
    switch (m_lcd.get_ppu_mode()) {
        case PpuMode::HorizontalBlank: return dot_tick_horizontal_blank();
        case PpuMode::VerticalBlank: return dot_tick_vertical_blank();
        case PpuMode::OamScan: return dot_tick_oam();
        case PpuMode::Drawing: return dot_tick_draw();
        default: return std::nullopt;
    }
}

std::optional<PpuMode> Ppu::dot_tick_horizontal_blank() {
    if (get_line_tick() >= 456 - 1) {
        return PpuMode::VerticalBlank;
    }

    return std::nullopt;
}

std::optional<PpuMode> Ppu::dot_tick_vertical_blank() {
    if (m_frame_tick >= 70224 - 1) {
        return PpuMode::OamScan;
    }

    return std::nullopt;
}

std::optional<PpuMode> Ppu::dot_tick_oam() {
    if (get_line_tick() == 80 - 1) {
        return PpuMode::Drawing;
    }

    return std::nullopt;
}

std::optional<PpuMode> Ppu::dot_tick_draw() {
    // TODO: Variable length

    if (get_line_tick() >= 80 + 289 - 1) {
        return PpuMode::HorizontalBlank;
    }

    return std::nullopt;
}

u16 Ppu::get_line_tick() const { return m_frame_tick % 456; }

u16 Ppu::get_line_number() const { return m_frame_tick / 456; }