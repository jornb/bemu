#include <bemu/gb/cpu.hpp>
#include <bemu/gb/external.hpp>
#include <bemu/gb/joypad.hpp>

using namespace bemu;
using namespace bemu::gb;

Joypad::Joypad(External& external, Cpu& cpu) : m_external(external), m_cpu(cpu) {}

bool Joypad::contains(const u16 address) const { return address == 0xFF00; }

u8 Joypad::read(u16) const {
    // If neither are selected, the low nibble reads 0xF (all buttons released)
    u8 buttons = 0xF;

    if (get_buttons_enabled()) {
        set_bit(buttons, 0, !m_button_states.m_a);
        set_bit(buttons, 1, !m_button_states.m_b);
        set_bit(buttons, 2, !m_button_states.m_select);
        set_bit(buttons, 3, !m_button_states.m_start);
    } else if (get_d_pad_enabled()) {
        set_bit(buttons, 0, !m_button_states.m_right);
        set_bit(buttons, 1, !m_button_states.m_left);
        set_bit(buttons, 2, !m_button_states.m_up);
        set_bit(buttons, 3, !m_button_states.m_down);
    }

    // Lower nibble is read-only buttons, upper nibble is writable
    return m_joypad & 0xF0 | buttons;
}

void Joypad::write(u16, const u8 value) {
    // The lower nibble is read-only, so only write the top
    m_joypad = value & 0xF0 | m_joypad & 0x0F;
}

void Joypad::cycle_tick() {
    const bool interrupt_enabled = get_buttons_enabled();

    const auto handle_button = [&, this](bool& current_value, const Button button) {
        if (!m_external.m_pending_buttons.contains(button)) {
            return;
        }

        const auto new_value = m_external.m_pending_buttons[button];
        if (current_value == new_value) {
            return;
        }

        // When a button is pressed, raise the interrupt (not on release)
        if (new_value && interrupt_enabled) {
            m_cpu.set_pending_interrupt(InterruptType::Joypad);
        }

        current_value = new_value;
    };

    handle_button(m_button_states.m_a, BUTTON_A);
    handle_button(m_button_states.m_b, BUTTON_B);
    handle_button(m_button_states.m_start, BUTTON_START);
    handle_button(m_button_states.m_select, BUTTON_SELECT);
    handle_button(m_button_states.m_up, BUTTON_UP);
    handle_button(m_button_states.m_down, BUTTON_DOWN);
    handle_button(m_button_states.m_left, BUTTON_LEFT);
    handle_button(m_button_states.m_right, BUTTON_RIGHT);

    m_external.m_pending_buttons.clear();
}
