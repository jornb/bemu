#pragma once
#include <array>

#include "bus.hpp"
#include "types.hpp"
#include "utils.hpp"

namespace bemu::gb {

template <int Width, int Height>
struct RenderTarget {
    constexpr static int screen_width = Width;
    constexpr static int screen_height = Height;
    std::array<std::array<u8, screen_width>, screen_height> m_pixels{};

    u8 get_pixel(int x, int y) { return m_pixels[y][x]; }

    void set_pixel(int x, int y, u8 pixel) { m_pixels[y][x] = pixel; }

    void render_tile(Bus &bus, const u16 start_address, size_t target_top_left_x, size_t target_top_left_y) {
        u16 current_address = start_address;
        for (size_t line = 0; line < 8; ++line) {
            const auto byte_1 = bus.read_u8(current_address++);
            const auto byte_2 = bus.read_u8(current_address++);

            for (size_t column = 0; column < 8; ++column) {
                // 7 is the left-most pixel
                const auto bit_1 = get_bit(byte_1, 7 - column);
                const auto bit_2 = get_bit(byte_2, 7 - column);

                u8 result = 0;
                set_bit(result, 0, bit_1);
                set_bit(result, 1, bit_2);

                m_pixels.at(target_top_left_y + line).at(target_top_left_x + column) = result;
            }
        }
    }

    void clear() { m_pixels = {}; }
};

struct Screen : RenderTarget<screen_width, screen_height> {};
}  // namespace bemu::gb
