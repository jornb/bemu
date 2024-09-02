#include <spdlog/fmt/fmt.h>

#include <bemu/gb/emulator.hpp>
#include <fstream>
#include <thread>

using namespace bemu::gb;

void Emulator::run() {
    while (m_running) {
        if (m_paused) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        if (!m_cpu.step()) {
            throw std::runtime_error("CPU step stopped");
        }
    }
}

void Emulator::add_cycles(const u16 cycles) {
    // PPU has 4 dots per M-cycle
    for (int m=0; m<cycles; m++) {
        for (int d=0; d<4; d++) {
            ++m_ticks;
            m_bus.m_ppu.dot_tick();
        }

        m_bus.m_ppu.cycle_tick();
    }
}
