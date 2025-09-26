#pragma once
#include <functional>
#include <memory>
#include <mutex>

#include "bus.hpp"
#include "cartridge.hpp"
#include "cpu.hpp"
#include "emulator.hpp"
#include "ppu.hpp"
#include "screen.hpp"

namespace bemu::gb {

struct Emulator {
    Cartridge m_cartridge;
    Bus m_bus{*this};
    Cpu m_cpu{*this};
    Screen m_screen;

    bool m_paused = false;
    bool m_running = true;
    // bool m_die;
    u64 m_ticks = 23440324;

    std::function<void()> m_callback_screen_rendered = [] {};
    std::function<void(const Screen&)> m_frame_callback = [](const Screen&) {};

    void run();

    void add_cycles(u16 cycles = 1);
};
}  // namespace bemu::gb
