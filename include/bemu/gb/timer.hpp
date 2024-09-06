#pragma once

#include "types.hpp"
#include "utils.hpp"
#include "ram.hpp"

namespace bemu::gb {
enum class TimerClockType : u8 {
    Timer256 = 0b00,
    Timer4 = 0b01,
    Timer16 = 0b10,
    Timer64 = 0b11,
};

#pragma pack(push, 1)
/// Timer and Divider Registers
struct Timer : MemoryRegion<0xFF04, Timer> {
    /// FF04 - DIV: Divider register
    ///
    /// This register is incremented at a rate of 16384Hz (~16779Hz on SGB). Writing any value to this register resets
    /// it to $00. Additionally, this register is reset when executing the stop instruction, and only begins ticking
    /// again once stop mode ends. This also occurs during a speed switch.
    ///
    /// Note: The divider is affected by CGB double speed mode, and will increment at 32768Hz in double speed.
    u8 m_divider;

    /// FF05 - TIMA: Timer counter
    ///
    /// This timer is incremented at the clock frequency specified by the TAC register ($FF07). When the value overflows
    /// (exceeds $FF) it is reset to the value specified in TMA (FF06) and an interrupt is requested, as described
    /// below.
    u8 m_counter;

    /// FF06 - TMA: Timer modulo
    ///
    /// When TIMA overflows, it is reset to the value in this register and an interrupt is requested. Example of use: if
    /// TMA is set to $FF, an interrupt is requested at the clock frequency selected in TAC (because every increment is
    /// an overflow). However, if TMA is set to $FE, an interrupt is only requested every two increments, which
    /// effectively divides the selected clock by two. Setting TMA to $FD would divide the clock by three, and so on.
    ///
    /// If a TMA write is executed on the same M-cycle as the content of TMA is transferred to TIMA due to a timer
    /// overflow, the old value is transferred to TIMA.
    u8 m_modulo;

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
    u8 m_control;

    void set_enable(const bool enable) { set_bit(m_control, 2, enable); }
    [[nodiscard]] bool get_enable() const { return get_bit(m_control, 2); }

    [[nodiscard]] TimerClockType get_clock_select() const {
        return static_cast<TimerClockType>(m_control & 0b00000011);
    }

    void set_clock_select(TimerClockType select) { m_control = (m_control & ~0b00000011) | static_cast<u8>(select); }
};
#pragma pack(pop)
}  // namespace bemu::gb