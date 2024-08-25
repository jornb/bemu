#pragma once
#include <vector>

#include "types.hpp"

namespace bemu {
/// General-purpose 8-bit screen buffer
struct Screen {
    explicit Screen() = default;

    explicit Screen(const size_t width, const size_t height) {
        m_pixels.resize(height);
        for (size_t y = 0; y < height; ++y) {
            m_pixels[y].resize(width);
        }
    }

    [[nodiscard]] size_t get_width() const { return m_pixels.empty() ? 0 : m_pixels[0].size(); }
    [[nodiscard]] size_t get_height() const { return m_pixels.size(); }

    [[nodiscard]] u8 get_pixel(const int x, const int y) const { return m_pixels[y][x]; }

    void set_pixel(const int x, const int y, const u8 pixel) { m_pixels[y][x] = pixel; }

    void clear() {
        for (auto& row : m_pixels) {
            for (auto& pixel : row) {
                pixel = 0;
            }
        }
    }

    [[nodiscard]] bool empty() const {
        for (const auto& row : m_pixels) {
            for (const auto& pixel : row) {
                if (pixel != 0) {
                    return false;
                }
            }
        }
        return true;
    }

    void serialize(auto& ar) {
        for (auto& row : m_pixels) {
            ar(row);
        }
    }

    std::vector<std::vector<u8>> m_pixels;
};
}  // namespace bemu
