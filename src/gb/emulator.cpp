#include <bemu/gb/bus.hpp>
#include <bemu/gb/cartridge.hpp>
#include <bemu/gb/cpu.hpp>
#include <bemu/gb/emulator.hpp>
#include <bemu/gb/ppu.hpp>
#include <fstream>
#include <future>
#include <thread>

using namespace bemu;
using namespace bemu::gb;

Emulator::Emulator(std::unique_ptr<Cartridge> cartridge)
    : m_external(std::make_shared<External>()),
      m_cartridge(std::move(cartridge)),
      m_bus(*this, m_cpu, *m_cartridge, *m_external) {
    m_cpu.connect(this, &m_bus);
}

void Emulator::run() {
    while (m_running) {
        if (!m_cpu.step()) {
            throw std::runtime_error("CPU step stopped");
        }
    }
}

bool Emulator::run_until(const std::function<bool(const Emulator &)> &condition, const size_t max_dots) {
    const auto start_dots = m_external->m_ticks;
    while (m_running && m_external->m_ticks - start_dots < max_dots) {
        if (!m_cpu.step()) {
            spdlog::info("CPU step stopped");
            return false;
        }

        if (condition(*this)) {
            return true;
        }
    }

    return false;
}

bool Emulator::run_to_next_frame() {
    return run_until([start_frame = m_external->m_frame_number](auto &self) {
        return self.m_external->m_frame_number != start_frame;
    });
}

bool Emulator::run_to_next_scan_line() {
    return run_until([start_ly = m_bus.m_lcd.m_data.ly](auto &self) { return self.m_bus.m_lcd.m_data.ly != start_ly; });
}

void Emulator::add_cycles() {
    // PPU has 4 dots per M-cycle.
    // TODO: Support double-speed, 2 dots per M-cycle
    for (int d = 0; d < 4; d++) {
        ++m_external->m_ticks;
        m_bus.m_ppu.dot_tick();

        // TODO: Add an extra cycle in double-speed mode
        m_bus.m_timer.dot_tick();
    }
    m_bus.m_ppu.cycle_tick();

    m_bus.m_joypad.cycle_tick();
}
