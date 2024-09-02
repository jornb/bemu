#pragma once
#include <array>
#include <optional>

#include "bus.hpp"
#include "enums.hpp"
#include "types.hpp"
#include "utils.hpp"

namespace bemu::gb {

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

struct Cpu;
struct CpuInstruction;

using instruction_function_t = void (Cpu::*)(const std::string &debug_prefix, const CpuInstruction &instruction);

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

    bool m_int_master_enabled = false;
    bool m_enabling_ime = false;
    u8 m_ie_register = 0x00;
    u8 m_int_flags;

    explicit Cpu(Emulator &emulator);

    u8 peek_u8() const;    ///< Fetch u8 from program counter
    u16 peek_u16() const;  ///< Fetch u16 from program counter, little-endian

    u8 fetch_u8();    ///< Fetch u8 from program counter
    u16 fetch_u16();  ///< Fetch u16 from program counter, little-endian

    /// Read 8-bit "register", using 16-bit registers as address
    u8 read_data_u8(Register reg, bool add_cycles = true);

    /// Write 8-bit "register", using 16-bit registers as address
    void write_data_u8(Register reg, u8 value, bool add_cycles = true);

    void stack_push8(u8 value);
    u8 stack_pop8();
    void stack_push16(u16 value);
    u16 stack_pop16();

    bool step();
    void execute_next_instruction();

    // bool execute_cb(const std::string &debug_prefix);
    //
    // bool execute_ld(const std::string &debug_prefix, u8 opcode);
    // bool execute_ld_d(const std::string &debug_prefix, Register reg);
    // void execute_ld(u16 address, Register reg);                                            ///< from reg into address
    // void execute_ld(Register reg, u16 address);                                            ///< from address into reg
    // bool execute_ld_r8_r8(const std::string &debug_prefix, Register reg1, Register reg2);  ///< from reg into reg
    // // bool execute_ld_r8_hl(const std::string &debug_prefix, Register reg1);  ///< from (HL) into reg
    //
    // bool execute_inc(const std::string &debug_prefix, u8 opcode);
    // bool execute_inc_r8(const std::string &debug_prefix, Register reg);
    // bool execute_inc_r16(const std::string &debug_prefix, Register reg);
    //
    // bool execute_dec(const std::string &debug_prefix, u8 opcode);
    // bool execute_dec_r8(const std::string &debug_prefix, Register reg);
    // bool execute_dec_r16(const std::string &debug_prefix, Register reg);
    //
    // bool execute_xor(const std::string &debug_prefix, u8 opcode);
    // void execute_xor_reg(const std::string &debug_prefix, Register input_reg);
    //
    // bool execute_cp(const std::string &debug_prefix, u8 opcode);
    // void execute_cp(const std::string &debug_prefix, Register reg);
    //
    // bool execute_and(const std::string &debug_prefix, u8 opcode);
    // bool execute_and(const std::string &debug_prefix, Register reg);
    //
    // bool execute_sub(const std::string &debug_prefix, u8 opcode);
    // bool execute_sub(const std::string &debug_prefix, Register reg);
    //
    // bool execute_call(const std::string &debug_prefix, u8 opcode);
    // bool execute_call(const std::string &debug_prefix, Condition condition);

    void execute_noop(const std::string &dbg, const CpuInstruction &instruction);
    void execute_stop(const std::string &dbg, const CpuInstruction &instruction);
    void execute_halt(const std::string &dbg, const CpuInstruction &instruction);
    void execute_ld(const std::string &dbg, const CpuInstruction &instruction);    ///< Load
    void execute_inc(const std::string &dbg, const CpuInstruction &instruction);   ///< Increment
    void execute_dec(const std::string &dbg, const CpuInstruction &instruction);   ///< Decrement
    void execute_add(const std::string &dbg, const CpuInstruction &instruction);   ///< Add
    void execute_adc(const std::string &dbg, const CpuInstruction &instruction);   ///< Add with carry
    void execute_sub(const std::string &dbg, const CpuInstruction &instruction);   ///< Sutract
    void execute_sbc(const std::string &dbg, const CpuInstruction &instruction);   ///< Subtract with carry
    void execute_cp(const std::string &dbg, const CpuInstruction &instruction);    ///< Compare
    void execute_and(const std::string &dbg, const CpuInstruction &instruction);   ///< Bitwise AND
    void execute_or(const std::string &dbg, const CpuInstruction &instruction);    ///< Bitwise OR
    void execute_xor(const std::string &dbg, const CpuInstruction &instruction);   ///< Bitwise XOR
    void execute_jp(const std::string &dbg, const CpuInstruction &instruction);    ///< Absolute jump
    void execute_call(const std::string &dbg, const CpuInstruction &instruction);  ///< Call function
    void execute_rst(const std::string &dbg, const CpuInstruction &instruction);   ///< Restart/call function
    void execute_ret(const std::string &dbg, const CpuInstruction &instruction);   ///< Return from function
    void execute_reti(const std::string &dbg, const CpuInstruction &instruction);  ///< Return from interrupt
    void execute_rrc(const std::string &dbg, const CpuInstruction &instruction);   ///< Rotate Right Circular
    void execute_rrca(const std::string &dbg, const CpuInstruction &instruction);  ///< Rotate Right Circular A
    void execute_rr(const std::string &dbg, const CpuInstruction &instruction);    ///< Rotate Right
    void execute_rra(const std::string &dbg, const CpuInstruction &instruction);   ///< Rotate Right A
    void execute_rlc(const std::string &dbg, const CpuInstruction &instruction);   ///< Rotate Left Circular
    void execute_rlca(const std::string &dbg, const CpuInstruction &instruction);  ///< Rotate Left Circular A
    void execute_rl(const std::string &dbg, const CpuInstruction &instruction);    ///< Rotate Left
    void execute_rla(const std::string &dbg, const CpuInstruction &instruction);   ///< Rotate Left A
    void execute_sla(const std::string &dbg, const CpuInstruction &instruction);   ///< Shift Left Arithmetic
    void execute_push(const std::string &dbg, const CpuInstruction &instruction);  ///< Stack push
    void execute_pop(const std::string &dbg, const CpuInstruction &instruction);   ///< Stack pop
    void execute_scf(const std::string &dbg, const CpuInstruction &instruction);   ///< Set carry flag
    void execute_ccf(const std::string &dbg, const CpuInstruction &instruction);   ///< Compliment carry flag
    void execute_cpl(const std::string &dbg, const CpuInstruction &instruction);   ///< Compliment accumulator
    void execute_daa(const std::string &dbg, const CpuInstruction &instruction);   ///< Decimal Adjust Accumulator
    void execute_bit(const std::string &dbg, const CpuInstruction &instruction);   ///< Test bit
    void execute_set(const std::string &dbg, const CpuInstruction &instruction);   ///< Set bit
    void execute_res(const std::string &dbg, const CpuInstruction &instruction);   ///< Clear bit

    /// Disables interrupt handling by setting IME=0, and cancelling any scheduled effects of the EI instruction (if
    /// any)
    void execute_di(const std::string &dbg, const CpuInstruction &instruction);

    /// Schedules interrupt handling to be enabled after the next machine cycle
    void execute_ei(const std::string &dbg, const CpuInstruction &instruction);

    /// Format operand to string
    std::string to_string(const OperandDescription &op) const;

    u16 decode_address(const OperandDescription &op);
    u8 decode_u8(const OperandDescription &op);
    u16 decode_u16(const OperandDescription &op);

    /// Write register, but interpret HL as indirect
    void write_register_u8(Register reg, u8 value);

    std::array<CpuInstruction, 256> m_instruction_handlers;
    std::array<CpuInstruction, 256> m_instruction_handlers_cb;
};
}  // namespace bemu::gb
