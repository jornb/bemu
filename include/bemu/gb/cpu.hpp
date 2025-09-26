#pragma once
#include <array>
#include <optional>

#include "bus.hpp"
#include "enums.hpp"
#include "types.hpp"
#include "utils.hpp"

namespace bemu::gb {

enum class InterruptType : u8 { VBlank, LCD, Timer, Serial, Joypad };

/// CPU regsiter type
enum class Register : u8 { None, A, F, B, C, D, E, H, L, AF, BC, DE, HL, SP, PC };
constexpr bool is_16bit(const Register reg) { return reg >= Register::AF; }

/// Jump condition
enum class Condition {
    None,  ///< Always jump
    Z,     ///< Jump if zero flag is set
    NZ,    ///< Jump if zero flag is not set
    C,     ///< Jump if carry flag is set
    NC     ///< Jump if carry flag is not set
};

enum class OperandType {
    None,                ///< No operand
    Register,            ///< Name of register
    Data8s,              ///< e2: Immediate 8-bit signed data
    Data8u,              ///< n8: Immediate 8-bit unsigned data
    Data16,              ///< n16: Little
    Address8,            ///< a8: Unsigned 8-bit data, added with 0xFF00 to get address
    RelativeAddress8pc,  ///< a8: Signed 8-bit data, relative to current pc
    RelativeAddress8sp,  ///< a8: Signed 8-bit data, relative to current sp
    Address16,           ///< a16: Little-endian 16-bit address

    /// Special name for HL+, which is the same as HL, but it is incremented after used
    HLI,

    /// Special name for HL-, which is the same as HL, but it is decremented after used
    HLD
};

struct OperandDescription {
    OperandDescription() = default;
    OperandDescription(const OperandType type) : m_type(type) {}
    OperandDescription(const Register reg, const bool register_is_indirect = false, const u8 parameter = 0x00)
        : m_type(OperandType::Register),
          m_register(reg),
          m_register_is_indirect(register_is_indirect),
          m_parameter(parameter) {}

    OperandType m_type;
    std::optional<Register> m_register;
    bool m_register_is_indirect = false;
    u8 m_parameter;
};

bool is_16bit(OperandDescription reg);

struct Cpu;
struct CpuInstruction;

using instruction_function_t = void (Cpu::*)(const std::string &debug_prefix, const CpuInstruction &, bool &branched);

struct CpuInstruction {
    instruction_function_t m_handler = nullptr;
    OperandDescription m_op1 = OperandType::None;
    OperandDescription m_op2 = OperandType::None;
    Condition m_condition = Condition::None;
    u8 m_parameter = 0x00;
};

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

    [[nodiscard]] u16 get_HL() const { return get_u16(Register::HL); }
    void set_HL(const u16 value) { set_u16(Register::HL, value); }

    [[nodiscard]] bool check_flags(Condition condition) const;

    [[nodiscard]] u8 get_u8(Register type) const;
    void set_u8(Register type, u8 value);

    [[nodiscard]] u16 get_u16(Register type) const;
    void set_u16(Register type, u16 value);
};

struct Emulator;

struct Cpu {
    Emulator &m_emulator;

    CpuRegisters m_registers;

    // current fetch...
    u16 m_fetched_data = 0;
    u16 m_memory_destination = 0;
    bool m_destination_is_memory;
    u8 m_current_opcode;
    // instruction *cur_inst;

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

    explicit Cpu(Emulator &emulator);

    [[nodiscard]] bool contains(u16 address) const;
    [[nodiscard]] u8 read_memory(u16 address) const;
    void write_memory(u16 address, u8 value);

    void set_pending_interrupt(InterruptType type, bool pending_interrupt = true);
    [[nodiscard]] bool has_pending_interrupt() const;

    u8 peek_u8() const;    ///< Fetch u8 from program counter
    u16 peek_u16() const;  ///< Fetch u16 from program counter, little-endian

    u8 fetch_u8();    ///< Fetch u8 from program counter
    u16 fetch_u16();  ///< Fetch u16 from program counter, little-endian

    /// Read 8-bit "register", using 16-bit registers as address
    u8 read_data_u8(Register reg);

    /// Write 8-bit "register", using 16-bit registers as address
    void write_data_u8(Register reg, u8 value);

    void stack_push8(u8 value);
    u8 stack_pop8();
    void stack_push16(u16 value);
    u16 stack_pop16();

    bool step();
    u8 execute_next_instruction();
    u8 execute_interrupts();

    // void execute_cb(const std::string &debug_prefix);
    //
    // void execute_ld(const std::string &debug_prefix, u8 opcode);
    // void execute_ld_d(const std::string &debug_prefix, Register reg);
    // void execute_ld(u16 address, Register reg);                                            ///< from reg into address
    // void execute_ld(Register reg, u16 address);                                            ///< from address into reg
    // void execute_ld_r8_r8(const std::string &debug_prefix, Register reg1, Register reg2);  ///< from reg into reg
    // // void execute_ld_r8_hl(const std::string &debug_prefix, Register reg1);  ///< from (HL) into reg
    //
    // void execute_inc(const std::string &debug_prefix, u8 opcode);
    // void execute_inc_r8(const std::string &debug_prefix, Register reg);
    // void execute_inc_r16(const std::string &debug_prefix, Register reg);
    //
    // void execute_dec(const std::string &debug_prefix, u8 opcode);
    // void execute_dec_r8(const std::string &debug_prefix, Register reg);
    // void execute_dec_r16(const std::string &debug_prefix, Register reg);
    //
    // void execute_xor(const std::string &debug_prefix, u8 opcode);
    // void execute_xor_reg(const std::string &debug_prefix, Register input_reg);
    //
    // void execute_cp(const std::string &debug_prefix, u8 opcode);
    // void execute_cp(const std::string &debug_prefix, Register reg);
    //
    // void execute_and(const std::string &debug_prefix, u8 opcode);
    // void execute_and(const std::string &debug_prefix, Register reg);
    //
    // void execute_sub(const std::string &debug_prefix, u8 opcode);
    // void execute_sub(const std::string &debug_prefix, Register reg);
    //
    // void execute_call(const std::string &debug_prefix, u8 opcode);
    // void execute_call(const std::string &debug_prefix, Condition condition);

    void execute_noop(const std::string &dbg, const CpuInstruction &, bool &branched);
    void execute_stop(const std::string &dbg, const CpuInstruction &, bool &branched);
    void execute_halt(const std::string &dbg, const CpuInstruction &, bool &branched);
    void execute_ld(const std::string &dbg, const CpuInstruction &, bool &branched);     ///< Load
    void execute_ld_f8(const std::string &dbg, const CpuInstruction &, bool &branched);  ///< Load, special 0xF8 version
    void execute_inc(const std::string &dbg, const CpuInstruction &, bool &branched);    ///< Increment
    void execute_dec(const std::string &dbg, const CpuInstruction &, bool &branched);    ///< Decrement
    void execute_add(const std::string &dbg, const CpuInstruction &, bool &branched);    ///< Add
    void execute_adc(const std::string &dbg, const CpuInstruction &, bool &branched);    ///< Add with carry
    void execute_sub(const std::string &dbg, const CpuInstruction &, bool &branched);    ///< Sutract
    void execute_sbc(const std::string &dbg, const CpuInstruction &, bool &branched);    ///< Subtract with carry
    void execute_cp(const std::string &dbg, const CpuInstruction &, bool &branched);     ///< Compare
    void execute_and(const std::string &dbg, const CpuInstruction &, bool &branched);    ///< Bitwise AND
    void execute_or(const std::string &dbg, const CpuInstruction &, bool &branched);     ///< Bitwise OR
    void execute_xor(const std::string &dbg, const CpuInstruction &, bool &branched);    ///< Bitwise XOR
    void execute_jp(const std::string &dbg, const CpuInstruction &, bool &branched);     ///< Absolute jump
    void execute_call(const std::string &dbg, const CpuInstruction &, bool &branched);   ///< Call function
    void execute_rst(const std::string &dbg, const CpuInstruction &, bool &branched);    ///< Restart/call function
    void execute_ret(const std::string &dbg, const CpuInstruction &, bool &branched);    ///< Return from function
    void execute_reti(const std::string &dbg, const CpuInstruction &, bool &branched);   ///< Return from interrupt
    void execute_sra(const std::string &dbg, const CpuInstruction &, bool &branched);    ///< Shift Right Arithmetic
    void execute_srl(const std::string &dbg, const CpuInstruction &, bool &branched);    ///< Shift Right Logical
    void execute_rrc(const std::string &dbg, const CpuInstruction &, bool &branched);    ///< Rotate Right Circular
    void execute_rrca(const std::string &dbg, const CpuInstruction &, bool &branched);   ///< Rotate Right Circular A
    void execute_rr(const std::string &dbg, const CpuInstruction &, bool &branched);     ///< Rotate Right
    void execute_rra(const std::string &dbg, const CpuInstruction &, bool &branched);    ///< Rotate Right A
    void execute_sla(const std::string &dbg, const CpuInstruction &, bool &branched);    ///< Shift Left Arithmetic
    void execute_rlc(const std::string &dbg, const CpuInstruction &, bool &branched);    ///< Rotate Left Circular
    void execute_rlca(const std::string &dbg, const CpuInstruction &, bool &branched);   ///< Rotate Left Circular A
    void execute_rl(const std::string &dbg, const CpuInstruction &, bool &branched);     ///< Rotate Left
    void execute_rla(const std::string &dbg, const CpuInstruction &, bool &branched);    ///< Rotate Left A
    void execute_push(const std::string &dbg, const CpuInstruction &, bool &branched);   ///< Stack push
    void execute_pop(const std::string &dbg, const CpuInstruction &, bool &branched);    ///< Stack pop
    void execute_scf(const std::string &dbg, const CpuInstruction &, bool &branched);    ///< Set carry flag
    void execute_ccf(const std::string &dbg, const CpuInstruction &, bool &branched);    ///< Compliment carry flag
    void execute_cpl(const std::string &dbg, const CpuInstruction &, bool &branched);    ///< Compliment accumulator
    void execute_daa(const std::string &dbg, const CpuInstruction &, bool &branched);    ///< Decimal Adjust Accumulator
    void execute_bit(const std::string &dbg, const CpuInstruction &, bool &branched);    ///< Test bit
    void execute_set(const std::string &dbg, const CpuInstruction &, bool &branched);    ///< Set bit
    void execute_res(const std::string &dbg, const CpuInstruction &, bool &branched);    ///< Clear bit
    void execute_swap(const std::string &dbg, const CpuInstruction &, bool &branched);   ///< Swap nibbles

    /// Disables interrupt handling by setting IME=0, and cancelling any scheduled effects of the EI instruction (if
    /// any)
    void execute_di(const std::string &dbg, const CpuInstruction &, bool &branched);

    /// Schedules interrupt handling to be enabled after the next machine cycle
    void execute_ei(const std::string &dbg, const CpuInstruction &, bool &branched);

    /// Format operand to string
    std::string to_string(const OperandDescription &op) const;

    u16 decode_address(const OperandDescription &op);
    u8 decode_u8(const OperandDescription &op);
    u16 decode_u16(const OperandDescription &op);

    /// Write register, but interpret HL as indirect
    void write_register_u8(Register reg, u8 value);

    /// Read register, but interpret HL as indirect
    [[nodiscard]] u8 read_register_u8(Register reg) const;

    std::array<CpuInstruction, 256> m_instruction_handlers;
    std::array<CpuInstruction, 256> m_instruction_handlers_cb;
};
}  // namespace bemu::gb
