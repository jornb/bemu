#include <spdlog/spdlog.h>

#include <algorithm>
#include <bemu/gb/bus.hpp>
#include <bemu/gb/cpu.hpp>
#include <bemu/gb/external.hpp>
#include <bemu/gb/lcd.hpp>
#include <bemu/gb/ppu.hpp>
#include <bemu/gb/screen.hpp>
#include <stdexcept>

using namespace bemu;
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

u8 decode_palette(const u8 palette, const u8 id) { return (palette >> (2 * id)) & 0b11; }
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
    if (!m_active) return;

    if (m_start_delay > 0) {
        --m_start_delay;
        return;
    }

    const auto source_address = 0x100 * m_written_value + m_current_byte;
    const auto destination_address = 0xFE00 + m_current_byte;
    ++m_current_byte;

    const auto data = m_bus.peek_u8(source_address);
    if (!m_oam.contains(destination_address)) {
        m_active = false;
    } else {
        m_oam.write(destination_address, data);
    }
}

Ppu::Ppu(External &external, Bus &bus, Lcd &lcd, Cpu &cpu)
    : m_external(external), m_bus(bus), m_lcd(lcd), m_cpu(cpu), m_oam_dma(m_bus, m_oam) {}

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
        return m_oam.write(address, value);
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

    // Enter new frame
    if (m_frame_tick == 0) {
        m_external.m_frame_number++;
    }

    // Check for ly compare
    m_lcd.m_data.ly = get_line_number();

    // Check ly compare every new line
    if (get_line_tick() == 0 && m_lcd.m_data.ly == m_lcd.m_data.ly_compare && m_lcd.is_ly_compare_interrupt_enabled()) {
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

    for (const auto &object : m_oam.m_data.m_entries) {
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

void Ppu::render_scanline_from_tilemap(const int screen_y, const int start_x, const int offset_x, const int offset_y,
                                       const u16 tile_set_address, const u16 tile_map_address, const u8 palette) {
    for (int screen_x = start_x; screen_x < screen_width; ++screen_x) {
        // Position in map space
        const auto map_x = screen_x + offset_x;
        const auto map_y = screen_y + offset_y;

        // Tile coordinates
        const auto tile_x = (map_x & 0xFF) / 8;  // wrap at 256
        const auto tile_y = (map_y & 0xFF) / 8;

        // Pixel within the tile
        const auto local_x = map_x & 7;
        const auto local_y = map_y & 7;

        // Index into 32x32 tilemap
        const auto tile_id_index = tile_x + tile_y * 32;
        const auto encoded_tile_id = m_vram.read(tile_map_address + tile_id_index);

        // Handle signed IDs for 0x8800 mode
        const auto signed_tile_address = tile_set_address == 0x8800;
        const auto tile_id = signed_tile_address ? (static_cast<s8>(encoded_tile_id) + 128) : encoded_tile_id;

        // Fetch tile data
        const auto tile_address = tile_set_address + tile_id * 16;
        const u8 *tile_data = m_vram.data().data() + (tile_address - m_vram.first_address);

        // Get color
        const auto tile_color_id = get_tile_px(tile_data, local_x, local_y);
        const auto tile_color_value = decode_palette(palette, tile_color_id);

        // Draw
        m_external.m_screen.set_pixel(screen_x, screen_y, tile_color_value);
    }
}

void Ppu::render_scanline_background() {
    // Get the tile set in which the actual 8x8 tiles are stored
    const auto tile_set_address = m_lcd.get_background_and_window_tile_data_start_address();

    // Get the tile map, containing the IDs of 32x32
    const auto tile_map_address = m_lcd.get_background_tile_map_start_address();

    const auto screen_y = get_line_number();
    const auto palette = m_lcd.m_data.bg_palette;

    render_scanline_from_tilemap(screen_y, 0, m_lcd.m_data.scroll_x, m_lcd.m_data.scroll_y, tile_set_address,
                                 tile_map_address, palette);
}

void Ppu::render_scanline_window() {
    const auto screen_y = get_line_number();
    const auto wy = m_lcd.m_data.window_y;
    if (screen_y < wy) return;

    const auto tile_set_address = m_lcd.get_background_and_window_tile_data_start_address();
    const auto tile_map_address = m_lcd.get_window_tile_map_start_address();
    const auto palette = m_lcd.m_data.bg_palette;

    // window starts at WX-7
    const auto wx = m_lcd.m_data.window_x - 7;

    render_scanline_from_tilemap(screen_y, std::max(0, wx), -wx, -wy, tile_set_address, tile_map_address, palette);
}

void Ppu::render_scanline_objects() {
    const auto line_objects = load_line_objects();
    auto screen_y = get_line_number();

    for (size_t i_object = 0; i_object < line_objects.size(); ++i_object) {
        const auto &object = *line_objects[i_object];

        for (int screen_x = 0; screen_x < screen_width; ++screen_x) {
            // All objects are 8 px wide
            auto local_x = screen_x - object.get_screen_x();
            if (local_x < 0 || local_x >= 8) {
                continue;
            }

            auto local_y = screen_y - object.get_screen_y();
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
            if (const auto existing_px = m_external.m_screen.get_pixel(screen_x, screen_y);
                object.background_has_priority() && existing_px != 0) {
                continue;
            }

            const u8 *tile_data = m_vram.data().data() + 16 * object.m_tile_index;
            const auto tile_pixel_index = get_tile_px(tile_data, local_x, local_y);

            // TODO: Drawing priority
            //       When opaque pixels from two different objects overlap, which pixel ends up being displayed is
            //       determined by another kind of priority: the pixel belonging to the higher-priority object wins.
            //       -
            //       However, this priority is determined differently when in CGB mode. In Non - CGB mode, the smaller
            //       the X coordinate, the higher the priority.When X coordinates are identical, the object located
            //       first in OAM has higher priority.In CGB mode, only the object’ s location in OAM determines its
            //       priority. The earlier the object, the higher its priority.
            if (tile_pixel_index == 0) continue;

            const auto palette = m_lcd.m_data.obj_palette[object.get_dmg_palette()];
            const auto tile_pixel_value = decode_palette(palette, tile_pixel_index);

            m_external.m_screen.set_pixel(screen_x, screen_y, tile_pixel_value);
        }
    }
}
