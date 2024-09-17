#pragma once

#include <array>
#include <magic_enum.hpp>
#include <mutex>
#include <optional>

#include "lcd.hpp"
#include "timer.hpp"
#include "types.hpp"

namespace bemu::gb {

struct ButtonStates {
    bool m_a = false;
    bool m_b = false;

    bool m_up = false;
    bool m_down = false;
    bool m_left = false;
    bool m_right = false;

    bool m_start = false;
    bool m_select = false;
};

struct Cpu;

struct Joypad {
    enum Button : u8 {
        BUTTON_A,
        BUTTON_B,
        BUTTON_START,
        BUTTON_SELECT,
        BUTTON_UP,
        BUTTON_DOWN,
        BUTTON_LEFT,
        BUTTON_RIGHT
    };

    explicit Joypad(Cpu &cpu) : m_cpu(cpu) {}

    [[nodiscard]] bool contains(u16 address) const;
    [[nodiscard]] u8 read_memory(u16 address) const;
    void write_memory(u16 address, u8 value);

    void cycle_tick();

    void set_button(Button button, bool pushed);

    [[nodiscard]] bool get_buttons_enabled() const { return !get_bit(m_joypad, 5); }
    [[nodiscard]] bool get_d_pad_enabled() const { return !get_bit(m_joypad, 4); }

   private:
    void handle_button(bool &current_value, std::optional<bool> &pending_value, bool interrupt_enabled);

    Cpu &m_cpu;

    /// FF00 - P1/JOYP: Joypad
    ///
    /// The eight Game Boy action/direction buttons are arranged as a 2×4 matrix. Select either action or direction
    /// buttons by writing to this register, then read out the bits 0-3.
    ///
    /// The lower nibble is Read-only. Note that, rather unconventionally for the Game Boy, a button being pressed is
    /// seen as the corresponding bit being 0, not 1.
    u8 m_joypad = 0x0;

    std::mutex m_mutex;
    ButtonStates m_button_states;
    std::array<std::optional<bool>, magic_enum::enum_count<Button>()> m_pending_buttons;
};
}  // namespace bemu::gb