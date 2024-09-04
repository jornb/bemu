#pragma once
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

    bool m_paused = false;
    bool m_running = true;
    // bool m_die;
    u64 m_ticks = 0;

    void run();

    void add_cycles(u16 cycles = 1);
};
}  // namespace bemu::gb
