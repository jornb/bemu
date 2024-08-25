#pragma once

#include "../types.hpp"
#include "ram.hpp"

namespace bemu::gb {
struct Cpu;

/// Timer and Divider Registers
struct Timer : IMemoryRegion, ICycled {
    explicit Timer(Cpu &cpu) : m_cpu{cpu} {}

    [[nodiscard]] bool contains(u16 address) const override;
    [[nodiscard]] u8 read(u16 address) const override;
    void write(u16 address, u8 value) override;

    /// Called every M-cycle
    /// The timers are updated @ 16384 Hz, which is every 64 M-cycles on regular speed, and every 32 M-cycles on
    /// double speed
    void dot_tick() override;

    /// FF04 - DIV: Divider register
    ///
    /// This register is incremented at a rate of 16384Hz (~16779Hz on SGB). Writing any value to this register resets
    /// it to $00. Additionally, this register is reset when executing the stop instruction, and only begins ticking
    /// again once stop mode ends. This also occurs during a speed switch.
    ///
    /// Note: The divider is affected by CGB double speed mode, and will increment at 32768Hz in double speed.
    u16 div = 0xABCC;

    /// FF05 - TIMA: Timer counter
    ///
    /// This timer is incremented at the clock frequency specified by the TAC register ($FF07). When the value overflows
    /// (exceeds $FF) it is reset to the value specified in TMA (FF06) and an interrupt is requested, as described
    /// below.
    u8 tima = 0;

    /// FF06 - TMA: Timer modulo
    ///
    /// When TIMA overflows, it is reset to the value in this register and an interrupt is requested. Example of use: if
    /// TMA is set to $FF, an interrupt is requested at the clock frequency selected in TAC (because every increment is
    /// an overflow). However, if TMA is set to $FE, an interrupt is only requested every two increments, which
    /// effectively divides the selected clock by two. Setting TMA to $FD would divide the clock by three, and so on.
    ///
    /// If a TMA write is executed on the same M-cycle as the content of TMA is transferred to TIMA due to a timer
    /// overflow, the old value is transferred to TIMA.
    u8 tma = 0;

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
    u8 tac = 0;

    void serialize(auto &ar) {
        ar(div);
        ar(tima);
        ar(tma);
        ar(tac);
        ar(m_overflowed);
    }

   private:
    Cpu &m_cpu;

    /// Used to stored whether we overflowed TIMA on the last dot tick.
    ///
    /// Needed because the interrupt is triggered one cycle after the overflow.
    bool m_overflowed = false;
};
}  // namespace bemu::gb