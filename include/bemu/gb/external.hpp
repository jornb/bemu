#pragma once
#include <functional>

#include "../types.hpp"
#include "joypad.hpp"
#include "screen.hpp"

namespace bemu::gb {
/// Data intended to be used by external system, e.g. rendering, input, audio, etc.
struct External {
    /// Set the state of the given button (pressed or released)
    ///
    /// Cleared on the next cycle. Only changes are necessary to report, e.g. don't need to set "held" buttons multiple
    /// times.
    void set_button(Joypad::Button button, bool pushed);

    /// Get the buttons set by set_button(), not yet processed by the CPU, and clear them.
    std::unordered_map<Joypad::Button, bool> pop_pending_buttons();

    void serialize(auto &ar) {
        m_screen.serialize(ar);
        ar(m_ticks);
        ar(m_frame_number);
        ar(m_serial_data_received);
        // Not pending buttons, since they are transient
    }

    /// Render target
    Screen m_screen{screen_width, screen_height};

    /// Number of ticks (t-cycles) since start of simulation
    u64 m_ticks = 0;

    /// Number of frames rendered since start of simulation
    u64 m_frame_number = 0;

    /// All received serial data. For debugging.
    std::vector<u8> m_serial_data_received;

    /// Buttons to be processed by the CPU next cycle.
    ///
    /// Cleared on the next cycle. Only changes are necessary to report, e.g. don't need to set "held" buttons multiple
    /// times.
    std::unordered_map<Joypad::Button, bool> m_pending_buttons;
};
}  // namespace bemu::gb
