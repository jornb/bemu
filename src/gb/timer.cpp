#include <bemu/gb/cpu.hpp>
#include <bemu/gb/timer.hpp>
#include <bemu/utils.hpp>

using namespace bemu;
using namespace bemu::gb;

namespace {
std::array<u8, 4> g_clock_select_to_bit_number = {9, 3, 5, 7};
}

bool Timer::contains(const u16 address) const { return 0xFF04 <= address && address <= 0xFF07; }

[[nodiscard]] u8 Timer::read(const u16 address) const {
    switch (address) {
        case 0xFF04: return div >> 8;
        case 0xFF05: return tima;
        case 0xFF06: return tma;
        case 0xFF07: return tac;
        default: throw std::runtime_error("Invalid timer memory access");
    }
}

void Timer::write(const u16 address, const u8 value) {
    // TODO [Timer][Inaccuracy]
    //
    // See https://gbdev.io/pandocs/Timer_Obscure_Behaviour.html
    // * Resetting the entire system counter (by writing to DIV) can reset the bit currently selected by the
    //   multiplexer, thus sending a "Timer tick" and/or "DIV-APU event" pulse early.
    // * Changing which bit of the system counter is selected (by changing the "Clock select" bits of TAC) from a bit
    //   currently set to another that is currently unset, will send a "Timer tick" pulse. (For example: if the system
    //   counter is equal to $3FF0 and TAC to $FC, writing $05 or $06 to TAC will instantly send a "Timer tick", but $04
    //   or $07 won’t.)
    // * On monochrome consoles, disabling the timer if the currently selected bit is set, will send a "Timer tick"
    //   once. This does not happen on Color models.
    // * On Color models, a write to TAC that fulfills the previous bullet's conditions and turns the timer on (it was
    //   disabled before) may or may not send a "Timer tick". The exact behaviour varies between individual consoles.

    switch (address) {
        case 0xFF04: {
            /// Writing any value to this register resets it to $00
            div = 0x00;
            break;
        }
        case 0xFF05: {
            tima = value;
            break;
        }
        case 0xFF06: {
            tma = value;
            break;
        }
        case 0xFF07: {
            tac = value;
            break;
        }
        default: throw std::runtime_error("Invalid timer memory access");
    }
}

void Timer::dot_tick() {
    // DIV counts regardless of whether the timer is enabled or not
    const u16 prev_div = div;
    div++;

    // Skip if not enabled
    const bool enabled = get_bit(tac, 2);

    // Get which bit of DIV we are using as the timer clock
    // See https://gbdev.io/pandocs/Timer_and_Divider_Registers.html#timer-and-divider-registers
    // Counts on falling edge, see https://gbdev.io/pandocs/Timer_Obscure_Behaviour.html
    const u8 clock_select = tac & 0b11;
    const u8 bit_number = g_clock_select_to_bit_number[clock_select];
    const bool bit_switched = get_bit(prev_div, bit_number) && !get_bit(div, bit_number);

    if (m_overflowed) {
        m_overflowed = false;
        tima = tma;
        m_cpu.set_pending_interrupt(InterruptType::Timer);
    } else if (bit_switched && enabled) {
        tima++;
        if (tima == 0x00) {
            m_overflowed = true;
        }
    }
}
