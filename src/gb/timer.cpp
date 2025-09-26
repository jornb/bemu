#include <bemu/gb/cpu.hpp>
#include <bemu/gb/timer.hpp>

using namespace bemu::gb;

bool Timer::contains(const u16 address) const { return 0xFF04 <= address && address <= 0xFF07; }

[[nodiscard]] u8 Timer::read_memory(const u16 address) const {
    // switch (address) {
    //     case 0xFF04: return m_divider;
    //     case 0xFF05: return m_counter;
    //     case 0xFF06: return m_modulo;
    //     case 0xFF07: return m_control;
    //     default: throw std::runtime_error("Invalid timer memory access");
    // }
    switch (address) {
        case 0xFF04: return div >> 8;
        case 0xFF05: return tima;
        case 0xFF06: return tma;
        case 0xFF07: return tac;
        default: throw std::runtime_error("Invalid timer memory access");
    }
}

void Timer::write_memory(const u16 address, const u8 value) {
    // switch (address) {
    //     case 0xFF04: {
    //         /// Writing any value to this register resets it to $00
    //         m_divider = 0x00;
    //         m_cycle_ticks_since_div_update = 0x00;
    //         break;
    //     }
    //     case 0xFF05: {
    //         m_counter = value;
    //         break;
    //     }
    //     case 0xFF06: {
    //         m_modulo = value;
    //         break;
    //     }
    //     case 0xFF07: {
    //         m_control = value;
    //         break;
    //     }
    //     default: throw std::runtime_error("Invalid timer memory access");
    // }
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

void Timer::cycle_tick() {
    // // Count DIV every 64 M-cycles = 16384 Hz
    // m_cycle_ticks_since_div_update++;
    // if (m_cycle_ticks_since_div_update >= 64/4 - 1) {
    //     m_cycle_ticks_since_div_update = 0;
    //     m_divider++;
    // }
    //
    // // Count TMA every X cycles
    // if (get_enable_tima()) {
    //     m_cycle_ticks_since_tma_update++;
    //     if (m_cycle_ticks_since_tma_update >= get_clock_select_m_cycles() - 1) {
    //         m_cycle_ticks_since_tma_update = 0;
    //
    //         if (m_counter < 0xFF) {
    //             m_counter++;
    //         } else {
    //             m_counter = m_modulo;
    //             m_cpu.set_pending_interrupt(InterruptType::Timer);
    //         }
    //     }
    // }

    const u16 prev_div = div;

    div++;

    bool timer_update = false;

    switch(tac & (0b11)) {
        case 0b00:
            timer_update = (prev_div & (1 << 9)) && (!(div & (1 << 9)));
            break;
        case 0b01:
            timer_update = (prev_div & (1 << 3)) && (!(div & (1 << 3)));
            break;
        case 0b10:
            timer_update = (prev_div & (1 << 5)) && (!(div & (1 << 5)));
            break;
        case 0b11:
            timer_update = (prev_div & (1 << 7)) && (!(div & (1 << 7)));
            break;
    }

    if (timer_update && tac & (1 << 2)) {
        tima++;

        if (tima == 0xFF) {
            tima = tma;

            m_cpu.set_pending_interrupt(InterruptType::Timer);
        }
    }
}
