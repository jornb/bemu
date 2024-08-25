#pragma once
#include <chrono>
#include <optional>
#include <thread>

#include "../types.hpp"
#include "screen.hpp"

namespace bemu::gb {
/// Helper for timing emulation to real time
struct Clock {
    constexpr static double frame_rate = 59.7275;

    void sleep_frame(const std::optional<double> speedup_factor = std::nullopt) {
        sleep(1.0 / frame_rate, speedup_factor);
    }

    void sleep_scanline(const std::optional<double> speedup_factor = std::nullopt) {
        sleep(1.0 / frame_rate / screen_height, speedup_factor);
    }

    void sleep(const double target_interval, const std::optional<double> speedup_factor = std::nullopt) {
        if (m_then) {
            const auto target_duration =
                std::chrono::duration<double>(target_interval / m_speedup_factor / speedup_factor.value_or(1.0));
            const auto target_time = *m_then + target_duration;
            m_then = std::chrono::high_resolution_clock::now();
            std::this_thread::sleep_until(target_time);
        } else {
            m_then = std::chrono::high_resolution_clock::now();
        }
    }

    double m_speedup_factor = 1.0;

   private:
    std::optional<std::chrono::high_resolution_clock::time_point> m_then;
};
}  // namespace bemu::gb
