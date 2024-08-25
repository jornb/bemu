#pragma once

#include "../types.hpp"
#include "../utils.hpp"
#include "interfaces.hpp"

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

    void serialize(auto &ar) {
        ar(m_a);
        ar(m_b);
        ar(m_up);
        ar(m_down);
        ar(m_left);
        ar(m_right);
        ar(m_start);
        ar(m_select);
    }
};

struct Cpu;
struct External;

struct Joypad : IMemoryRegion, ICycled {
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

    explicit Joypad(External &external, Cpu &cpu);

    [[nodiscard]] bool contains(u16 address) const override;
    [[nodiscard]] u8 read(u16 address) const override;
    void write(u16 address, u8 value) override;

    void cycle_tick() override;

    [[nodiscard]] bool get_buttons_enabled() const { return !get_bit(m_joypad, 5); }
    [[nodiscard]] bool get_d_pad_enabled() const { return !get_bit(m_joypad, 4); }

    void serialize(auto &ar) {
        ar(m_joypad);
        m_button_states.serialize(ar);
    }

   private:
    External &m_external;
    Cpu &m_cpu;

    /// FF00 - P1/JOYP: Joypad
    ///
    /// The eight Game Boy action/direction buttons are arranged as a 2×4 matrix. Select either action or direction
    /// buttons by writing to this register, then read out the bits 0-3.
    ///
    /// The lower nibble is Read-only. Note that, rather unconventionally for the Game Boy, a button being pressed is
    /// seen as the corresponding bit being 0, not 1.
    u8 m_joypad = 0x0;

    ButtonStates m_button_states;
};
}  // namespace bemu::gb