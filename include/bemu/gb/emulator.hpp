#pragma once
#include <functional>
#include <memory>
#include <optional>

#include "../emulator.hpp"
#include "bus.hpp"
#include "cartridge.hpp"
#include "cpu.hpp"
#include "external.hpp"
#include "interfaces.hpp"

namespace bemu::gb {

struct Cartridge;

struct Emulator : IEmulator, ICycler {
    explicit Emulator(std::unique_ptr<Cartridge> cartridge);

    size_t get_tick_count() const override { return m_external->m_ticks; }
    Screen &get_screen() override { return m_external->m_screen; }
    const Screen &get_screen() const override { return m_external->m_screen; }

    void run();
    void add_cycles() override;

    /// Run until some condition is met, or the emulator stops running.
    /// The condition is checked after each CPU step.
    /// Returns true if the condition was met, false if the emulator stopped running for some other reason.
    bool run_until(const std::function<bool(const Emulator &)> &condition, size_t max_dots = 4 * 1024 * 1024 * 60);

    bool run_to_next_frame();
    bool run_to_next_scan_line();

    void serialize(auto &ar) {
        ar(m_running);
        m_cpu.serialize(ar);
        m_bus.serialize(ar);
        m_cartridge->serialize(ar);
        m_external->serialize(ar);
    }

    std::shared_ptr<External> m_external;
    std::unique_ptr<Cartridge> m_cartridge;
    Cpu m_cpu;
    Bus m_bus;

    bool m_running = true;
};
}  // namespace bemu::gb
