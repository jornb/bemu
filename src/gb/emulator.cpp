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

void Emulator::add_cycles(const u16 cycles) { m_ticks += 4 * cycles; }
