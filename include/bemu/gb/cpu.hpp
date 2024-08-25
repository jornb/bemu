#pragma once
#include <array>
#include <string>

#include "../types.hpp"
#include "interfaces.hpp"

namespace bemu::gb {

enum class InterruptType : u8 { VBlank, LCD, Timer, Serial, Joypad };

/// CPU register type
enum class Register : u8 { NoRegister, A, F, B, C, D, E, H, L, AF, BC, DE, HL, SP, PC };
enum class Register8 : u8 { A, F, B, C, D, E, H, L };
enum class Register16 : u8 { AF, BC, DE, HL, SP, PC };
constexpr bool is_16bit(const Register reg) { return reg >= Register::AF; }

/// Jump condition
enum class Condition {
    NoCondition,  ///< Always jump
    Z,            ///< Jump if zero flag is set
    NZ,           ///< Jump if zero flag is not set
    C,            ///< Jump if carry flag is set
    NC            ///< Jump if carry flag is not set
};

struct Cpu;
struct CpuInstruction;

using free_instruction_function_t = void(Cpu &);
using instruction_function_t = void (Cpu::*)(const std::string &debug_prefix, const CpuInstruction &instruction);

struct CpuRegisters {
    u8 a = 0x01;  ///< Accumulator

    /// Flags
    ///
    /// Bit 7: Z - Zero flag
    /// Bit 6: N - Subtract flag
    /// Bit 5: H - Half carry flag
    /// Bit 4: C - Carry flag
    /// Bits 3..0: 0
    u8 f = 0xB0;

    u8 b = 0x00;
    u8 c = 0x13;
    u8 d = 0x00;
    u8 e = 0xD8;
    u8 h = 0x01;
    u8 l = 0x4D;
    u16 pc = 0x0100;  ///< Program counter
    u16 sp = 0xFFFE;  ///< Stack pointer

    [[nodiscard]] bool get_z() const;
    [[nodiscard]] bool get_n() const;
    [[nodiscard]] bool get_h() const;
    [[nodiscard]] bool get_c() const;

    void set_z(bool z);
    void set_n(bool n);
    void set_h(bool h);
    void set_c(bool c);
    void set_flags(bool z, bool n, bool h, bool c);

    [[nodiscard]] bool check_flags(Condition condition) const;

    [[nodiscard]] u8 get_u8(Register type) const;
    void set_u8(Register type, u8 value);

    [[nodiscard]] u16 get_u16(Register type) const;
    void set_u16(Register type, u16 value);

    [[nodiscard]] u8 read(Register8 type) const;
    [[nodiscard]] u16 read(Register16 type) const;

    void write(Register8 type, u8 value);
    void write(Register16 type, u16 value);

    void serialize(auto &ar) {
        ar(a);
        ar(b);
        ar(c);
        ar(d);
        ar(e);
        ar(f);
        ar(h);
        ar(l);
        ar(pc);
        ar(sp);
    }
};

struct External;
struct Lcd;
struct MemoryBus;
struct Timer;

/// Sharp Z80 CPU
struct Cpu : IMemoryRegion {
    Cpu();

    void connect(ICycler *cycler, MemoryBus *memory);

    void add_cycle();

    // IMemoryRegion
    [[nodiscard]] bool contains(u16 address) const override;
    [[nodiscard]] u8 read(u16 address) const override;
    void write(u16 address, u8 value) override;

    void set_pending_interrupt(InterruptType type, bool pending_interrupt = true);
    [[nodiscard]] bool has_pending_interrupt() const;

    [[nodiscard]] u8 peek_u8() const;    ///< Fetch u8 from program counter
    [[nodiscard]] u16 peek_u16() const;  ///< Fetch u16 from program counter, little-endian
    [[nodiscard]] u8 fetch_u8();         ///< Fetch u8 from program counter
    [[nodiscard]] u16 fetch_u16();       ///< Fetch u16 from program counter, little-endian

    void stack_push8(u8 value);
    u8 stack_pop8();
    void stack_push16(u16 value);
    u16 stack_pop16();

    /// Execute a single CPU instruction
    ///
    /// If halted, adds 1 cycle and returns.
    bool step();
    void execute_next_instruction();
    void execute_interrupts();

    void serialize(auto &ar) {
        m_registers.serialize(ar);
        ar(m_halted);
        ar(m_stepping);
        ar(m_interrupt_master_enable);
        ar(m_set_interrupt_master_enable_next_cycle);
        ar(m_interrupt_request_flags);
        ar(m_interrupt_enable_flags);
    }

    CpuRegisters m_registers;

    bool m_halted = false;
    bool m_stepping = false;

    /// IME is a flag internal to the CPU that controls whether any interrupt handlers are called, regardless of the
    /// contents of IE. IME cannot be read in any way, and is modified by these instructions/events only:
    ///     * ei
    ///     * di
    ///     * reti
    ///     * When an interrupt handler is executed: Disables interrupts before calling the interrupt handler
    ///
    /// IME is unset (interrupts are disabled) when the game starts running.
    ///
    /// The effect of ei is delayed by one instruction. This means that ei followed immediately by di does not allow any
    /// interrupts between them. This interacts with the halt bug in an interesting way.
    bool m_interrupt_master_enable = false;

    /// The delay of interrupt master enable is delayed with 1 instruction. Hence, we set this flag instead.
    bool m_set_interrupt_master_enable_next_cycle = false;

    /// FF0F - IF: Interrupt flag
    ///
    /// Bit 0: VBlank
    /// Bit 1: LCD
    /// Bit 2: Timer
    /// Bit 3: Serial
    /// Bit 4: Joypad
    ///
    /// When an interrupt request signal (some internal wire going from the PPU/APU/… to the CPU) changes from low to
    /// high, the corresponding bit in the IF register becomes set. For example, bit 0 becomes set when the PPU enters
    /// the VBlank period.
    ///
    /// Any set bits in the IF register are only requesting an interrupt. The actual execution of the interrupt handler
    /// happens only if both the IME flag and the corresponding bit in the IE register are set; otherwise the interrupt
    /// “waits” until both IME and IE allow it to be serviced.
    ///
    /// Since the CPU automatically sets and clears the bits in the IF register, it is usually not necessary to write to
    /// the IF register. However, the user may still do that in order to manually request (or discard) interrupts. Just
    /// like real interrupts, a manually requested interrupt isn’t serviced unless/until IME and IE allow it.
    u8 m_interrupt_request_flags = 0;

    /// FFFF — IE: Interrupt enable
    u8 m_interrupt_enable_flags = 0;

    // Pointers back to memory and cycler.
    // Both cycler and memory own CPU, so raw pointers are fine.
    //
    // Set up in connect()
    ICycler *m_cycler = nullptr;
    MemoryBus *m_memory = nullptr;

   private:
    std::array<free_instruction_function_t *, 256> m_instruction_handlers{nullptr};
    std::array<free_instruction_function_t *, 256> m_instruction_handlers_cb{nullptr};
};
}  // namespace bemu::gb
