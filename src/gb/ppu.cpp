#include <spdlog/spdlog.h>

#include <algorithm>
#include <bemu/gb/bus.hpp>
#include <bemu/gb/ppu.hpp>
#include <bemu/gb/screen.hpp>
#include <stdexcept>

using namespace bemu::gb;

namespace {
constexpr u16 dots_per_oam_scan = 80;
constexpr u16 dots_per_line = 456;
constexpr u32 dots_per_frame = 70224;

u8 get_tile_px(const u8 *tile_data, const size_t local_x, const size_t local_y) {
    // A tile is 16 bytes, where each line is 2 bytes
    const u16 line_address = local_y * 2;

    const auto byte_1 = tile_data[line_address];
    const auto byte_2 = tile_data[line_address + 1];

    const auto bit_1 = get_bit(byte_1, 7 - local_x);  // Bit 7 represents the left-most pixel
    const auto bit_2 = get_bit(byte_2, 7 - local_x);

    u8 result = 0;
    set_bit(result, 0, bit_1);
    set_bit(result, 1, bit_2);

    return result;
}
}  // namespace

bool DmaState::contains(const u16 address) const { return address == 0xFF46; }

u8 DmaState::read(u16) const { return m_written_value; }

void DmaState::write(u16, const u8 value) {
    m_active = true;
    m_written_value = value;
    m_current_byte = 0;
    m_start_delay = 2;
}

void DmaState::cycle_tick() {
    static Bus *bus = &m_bus;
    if (&m_bus != bus) {
        throw std::runtime_error("Bus has changed!");
    }

    if (!m_active) return;

    if (m_start_delay > 0) {
        --m_start_delay;
        return;
    }

    const auto source_address = 0x100 * m_written_value + m_current_byte;
    const auto destination_address = 0xFE00 + m_current_byte;
    ++m_current_byte;

    const auto data = m_bus.read_u8(source_address, false);
    if (!m_oam.contains(destination_address)) {
        m_active = false;
    } else {
        m_oam.write_memory(destination_address, data);
    }
}

bool Ppu::contains(const u16 address) const {
    return m_oam_dma.contains(address) || m_oam.contains(address) || m_vram.contains(address);
}

u8 Ppu::read(const u16 address) const {
    if (m_oam_dma.contains(address)) {
        return m_oam_dma.read(address);
    }

    // When LCD/PPU is not on, all memory is accessible
    const auto allow_all = !m_lcd.get_enable_lcd_and_ppu();
    const auto mode = m_lcd.get_ppu_mode();
    if (!allow_all && mode == PpuMode::Drawing) {
        return 0xFF;
    }

    if (m_oam.contains(address) && (allow_all || mode == PpuMode::OamScan)) {
        return 0xFF;
    }

    if (m_oam.contains(address)) {
        return m_oam.read_memory(address);
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

    // When LCD/PPU is not on, all memory is accessible
    const auto allow_all = !m_lcd.get_enable_lcd_and_ppu();
    const auto mode = m_lcd.get_ppu_mode();
    if (!allow_all && mode == PpuMode::Drawing) {
        return;
    }

    if (m_oam.contains(address) && (allow_all || mode == PpuMode::OamScan)) {
        return;
    }

    if (m_oam.contains(address)) {
        return m_oam.write_memory(address, value);
    }

    if (m_vram.contains(address)) {
        return m_vram.write(address, value);
    }
}

void Ppu::dot_tick() {
    m_frame_tick++;
    m_frame_tick %= dots_per_frame;

    // Handle current tick
    dot_tick_handle_and_get_next_mode();

    // const auto next_mode = dot_tick_handle_and_get_next_mode();
    // if (next_mode.has_value()) {
    //     m_lcd.set_ppu_mode(*next_mode);
    //
    //     // Signal interrupts
    //     if (next_mode == PpuMode::VerticalBlank) {
    //         m_cpu.set_pending_interrupt(InterruptType::VBlank);
    //
    //         if (m_lcd.is_vertical_blank_interrupt_enabled()) {
    //             m_cpu.set_pending_interrupt(InterruptType::LCD);
    //         }
    //     } else if (next_mode == PpuMode::HorizontalBlank) {
    //         if (m_lcd.is_horizontal_blank_interrupt_enabled()) {
    //             m_cpu.set_pending_interrupt(InterruptType::LCD);
    //         }
    //     } else if (next_mode == PpuMode::OamScan) {
    //         if (m_lcd.is_oam_interrupt_enabled()) {
    //             m_cpu.set_pending_interrupt(InterruptType::LCD);
    //         }
    //     }
    // }

    // Enter new frame
    if (m_frame_tick == 0) {
        m_frame_number++;
        // spdlog::info("m_frame_number = {}", m_frame_number);

        draw_output_screen();

        m_screen.clear();
    }

    // Check for ly compare
    m_lcd.ly = get_line_number();

    // if (get_line_tick() == 0)
    //     spdlog::info("ly = {}", m_lcd.ly);

    // Check ly compare every new line
    if (get_line_tick() == 0 && m_lcd.ly == m_lcd.ly_compare && m_lcd.is_ly_compare_interrupt_enabled()) {
        m_cpu.set_pending_interrupt(InterruptType::LCD);
    }
}

void Ppu::cycle_tick() { m_oam_dma.cycle_tick(); }

void Ppu::dot_tick_handle_and_get_next_mode() {
    const auto line_tick = get_line_tick();
    const auto y = get_line_number();

    // Start of VBlank period
    if (m_frame_tick == screen_height * dots_per_line - 1) {
        m_lcd.set_ppu_mode(PpuMode::VerticalBlank);
        m_cpu.set_pending_interrupt(InterruptType::VBlank);
        if (m_lcd.is_vertical_blank_interrupt_enabled()) {
            m_cpu.set_pending_interrupt(InterruptType::LCD);
        }
        return;
    }

    // If we're in vblank, then we don't transition until we're back at tick 0
    if (m_lcd.get_ppu_mode() == PpuMode::VerticalBlank && m_frame_tick != 0) return;

    if (line_tick == 0) {
        // Start of Mode 2: OAM scan
        m_lcd.set_ppu_mode(PpuMode::OamScan);

        if (m_lcd.is_oam_interrupt_enabled()) {
            m_cpu.set_pending_interrupt(InterruptType::LCD);
        }
    }

    if (line_tick == dots_per_oam_scan - 1) {
        // Start of mode 3: Drawing pixels
        m_lcd.set_ppu_mode(PpuMode::Drawing);

        // In reality, rendering is a complicated process taking multiple cycles.
        // However, since the memory is read only during this period anyway, we may as well do the whole line
        // immediately.
        render_scanline();
    }

    if (line_tick == dots_per_oam_scan + 289 - 1) {
        // TODO: Calculate length of mode 3. 172-289 dots. See https://gbdev.io/pandocs/Rendering.html#mode-3-length

        // Start of Mode 0: Horizontal blank
        m_lcd.set_ppu_mode(PpuMode::HorizontalBlank);

        if (m_lcd.is_horizontal_blank_interrupt_enabled()) {
            m_cpu.set_pending_interrupt(InterruptType::LCD);
        }
    }
}

std::optional<PpuMode> Ppu::dot_tick_horizontal_blank() {
    if (get_line_tick() >= dots_per_line - 1) {
        // When the last line is done, start vblank, otherwise start OAM scan on next line
        if (get_line_tick() >= screen_height - 1) {
            return PpuMode::VerticalBlank;
        }
        return PpuMode::OamScan;
    }

    return std::nullopt;
}

std::optional<PpuMode> Ppu::dot_tick_vertical_blank() {
    if (m_frame_tick >= dots_per_frame - 1) {
        return PpuMode::OamScan;
    }

    return std::nullopt;
}

std::optional<PpuMode> Ppu::dot_tick_oam() {
    if (get_line_tick() == 0) {
        load_line_objects();
    }

    if (get_line_tick() == dots_per_oam_scan - 1) {
        return PpuMode::Drawing;
    }

    return std::nullopt;
}

std::optional<PpuMode> Ppu::dot_tick_draw() {
    // TODO: Variable length

    // In reality, rendering is a complicated process taking multiple cycles.
    // However, since the memory is read only during this period anyway, we may as well do the whole line immediately.
    if (get_line_tick() == dots_per_oam_scan) {
        render_scanline();
    }

    if (get_line_tick() >= dots_per_oam_scan + 289 - 1) {
        return PpuMode::HorizontalBlank;
    }

    return std::nullopt;
}

u16 Ppu::get_line_tick() const { return m_frame_tick % dots_per_line; }

u16 Ppu::get_line_number() const { return m_frame_tick / dots_per_line; }

std::vector<const OamEntry *> Ppu::load_line_objects() {
    std::vector<const OamEntry *> line_objects;
    line_objects.reserve(10);

    const auto ly = get_line_number();
    const auto object_height = m_lcd.get_object_height();

    for (const auto &object : m_oam.m_entries) {
        // y pixel is stored -16
        const auto y_start = object.get_screen_y();
        const auto y_end = y_start + object_height;

        if (y_start <= ly && ly < y_end) {
            line_objects.push_back(&object);

            // Never more than 10 allowed
            if (line_objects.size() == 10) {
                break;
            }
        }
    }

    // Sort by x coordinate for simplicity later
    std::ranges::stable_sort(line_objects, [](const OamEntry *a, const OamEntry *b) { return a->m_x < b->m_x; });

    return line_objects;
}

void Ppu::render_scanline() {
    if (!m_lcd.get_enable_lcd_and_ppu()) return;

    if (m_lcd.get_background_and_window_enable()) {
        render_scanline_background();

        if (m_lcd.get_window_enable()) {
            render_scanline_window();
        }
    }

    if (m_lcd.get_object_enable()) {
        render_scanline_objects();
    }
}

void Ppu::render_scanline_background() {}

void Ppu::render_scanline_window() {}

void Ppu::render_scanline_objects() {
    const auto line_objects = load_line_objects();
    auto y = get_line_number();

    for (size_t i_object = 0; i_object < line_objects.size(); ++i_object) {
        const auto &object = *line_objects[i_object];

        for (int x = 0; x < Screen::screen_width; ++x) {
            // All objects are 8 px wide
            auto local_x = x - object.get_screen_x();
            if (local_x < 0 || local_x >= 8) {
                continue;
            }

            auto local_y = y - object.get_screen_y();
            if (local_y < 0 || local_y >= m_lcd.get_object_height()) {
                throw std::runtime_error("render_scanline_objects");
            }

            if (object.get_x_flip()) {
                local_x = 7 - local_x;
            }
            if (object.get_y_flip()) {
                local_y = m_lcd.get_object_height() - 1 - local_y;
            }

            // Don't draw on prioritized background
            const auto existing_px = m_screen.get_pixel(x, y);
            if (object.background_has_priority() && existing_px != 0) {
                continue;
            }

            const u8 *tile_data = m_vram.data() + 16 * object.m_tile_index;
            const auto tile_pixel = get_tile_px(tile_data, local_x, local_y);
            spdlog::error("Tile {:02x} @ {}, {} (height is {}). Setting {}, {}  ({}, {}) = {}", object.m_tile_index,
                          object.get_screen_x(), object.get_screen_y(), m_lcd.get_object_height(), x, y, local_x,
                          local_y, tile_pixel);
            m_screen.set_pixel(x, y, tile_pixel);
        }
    }
}

void Ppu::draw_output_screen() {
    // if (!m_lcd.get_enable_lcd_and_ppu()) return;

    for (int y = 0; y < m_screen.screen_height; ++y) {
        std::string s;
        for (int x = 0; x < m_screen.screen_width; ++x) {
            const auto b = m_screen.get_pixel(x, y);
            if (b == 0) {
                s += " ";
            } else if (b == 1) {
                s += "-";
            } else if (b == 2) {
                s += "x";
            } else if (b == 3) {
                s += "#";
            }
        }

        spdlog::warn(s);
    }
}
