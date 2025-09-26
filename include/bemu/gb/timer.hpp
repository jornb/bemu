#pragma once

#include "ram.hpp"
#include "types.hpp"
#include "utils.hpp"

namespace bemu::gb {
struct Cpu;

/// Timer and Divider Registers
struct Timer {
    explicit Timer(Cpu &cpu) : m_cpu{cpu} {}

    /// FF04 - DIV: Divider register
    ///
    /// This register is incremented at a rate of 16384Hz (~16779Hz on SGB). Writing any value to this register resets
    /// it to $00. Additionally, this register is reset when executing the stop instruction, and only begins ticking
    /// again once stop mode ends. This also occurs during a speed switch.
    ///
    /// Note: The divider is affected by CGB double speed mode, and will increment at 32768Hz in double speed.
    u8 m_divider = 0xAB;

    /// FF05 - TIMA: Timer counter
    ///
    /// This timer is incremented at the clock frequency specified by the TAC register ($FF07). When the value overflows
    /// (exceeds $FF) it is reset to the value specified in TMA (FF06) and an interrupt is requested, as described
    /// below.
    u8 m_counter = 0;

    /// FF06 - TMA: Timer modulo
    ///
    /// When TIMA overflows, it is reset to the value in this register and an interrupt is requested. Example of use: if
    /// TMA is set to $FF, an interrupt is requested at the clock frequency selected in TAC (because every increment is
    /// an overflow). However, if TMA is set to $FE, an interrupt is only requested every two increments, which
    /// effectively divides the selected clock by two. Setting TMA to $FD would divide the clock by three, and so on.
    ///
    /// If a TMA write is executed on the same M-cycle as the content of TMA is transferred to TIMA due to a timer
    /// overflow, the old value is transferred to TIMA.
    u8 m_modulo = 0;

    /// FF07 - TAC: Timer control
    ///
    /// Bits 0..1:  Clock select
    ///     00  256 M-cycles
    ///     01    4 M-cycles
    ///     10   16 M-cycles
    ///     11   64 M-cycles
    ///
    /// Bit 2: Enable: Controls whether TIMA is incremented. Note that DIV is always counting, regardless of this bit.
    ///
    /// Note that writing to this register may increase TIMA once!
    u8 m_control = 0xF8;

    [[nodiscard]] bool contains(u16 address) const;
    [[nodiscard]] u8 read_memory(u16 address) const;
    void write_memory(u16 address, u8 value);

    void set_enable(const bool enable) { set_bit(m_control, 2, enable); }
    [[nodiscard]] bool get_enable_tima() const { return get_bit(m_control, 2); }

    [[nodiscard]] u16 get_clock_select_m_cycles() const {
        switch (m_control & 0b00000011) {
            case 0b01: return 4;
            case 0b10: return 16;
            case 0b11: return 64;
            default: return 256;
        }
    }
    /// Called every M-cycle
    /// The timers are updated @ 16384 Hz, which is every 64 M-cycles on regular speed, and every 32 M-cycles on
    /// double speed
    void cycle_tick();

   // private:
    Cpu &m_cpu;

    u8 m_cycle_ticks_since_div_update = 0;
    u16 m_cycle_ticks_since_tma_update = 0;


    u16 div = 0xABCC;
    u8 tima = 0;
    u8 tma = 0;
    u8 tac = 0;
};
}  // namespace bemu::gb