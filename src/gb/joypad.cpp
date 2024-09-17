#include <bemu/gb/cpu.hpp>
#include <bemu/gb/joypad.hpp>

using namespace bemu::gb;

bool Joypad::contains(u16 address) const { return address == 0xFF00; }

u8 Joypad::read_memory(u16) const {
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

void Joypad::write_memory(u16, const u8 value) {
    // The lower nibble is read-only, so only write the top
    m_joypad = value & 0xF0 | m_joypad & 0x0F;
}

void Joypad::cycle_tick() {
    std::scoped_lock lock{m_mutex};
    handle_button(m_button_states.m_a, m_pending_buttons[BUTTON_A], get_buttons_enabled());
    handle_button(m_button_states.m_b, m_pending_buttons[BUTTON_B], get_buttons_enabled());
    handle_button(m_button_states.m_start, m_pending_buttons[BUTTON_START], get_buttons_enabled());
    handle_button(m_button_states.m_select, m_pending_buttons[BUTTON_SELECT], get_buttons_enabled());
    handle_button(m_button_states.m_up, m_pending_buttons[BUTTON_UP], get_d_pad_enabled());
    handle_button(m_button_states.m_down, m_pending_buttons[BUTTON_DOWN], get_d_pad_enabled());
    handle_button(m_button_states.m_left, m_pending_buttons[BUTTON_LEFT], get_d_pad_enabled());
    handle_button(m_button_states.m_right, m_pending_buttons[BUTTON_RIGHT], get_d_pad_enabled());
    m_pending_buttons = {};
}

void Joypad::handle_button(bool &current_value, std::optional<bool> &pending_value, const bool interrupt_enabled) {
    if (!pending_value.has_value() || *pending_value == current_value) {
        return;
    }

    // When a button is pressed, raise the interrupt
    if (current_value && interrupt_enabled) {
        m_cpu.set_pending_interrupt(InterruptType::Joypad);
    }

    // Pop the pending value into the current value
    current_value = *pending_value;
    pending_value = std::nullopt;
}

void Joypad::set_button(const Button button, bool pushed) {
    std::scoped_lock lock{m_mutex};
    m_pending_buttons[button] = pushed;
}
