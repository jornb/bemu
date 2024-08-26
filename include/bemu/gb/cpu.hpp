#pragma once
#include "bus.hpp"
#include "enums.hpp"
#include "types.hpp"
#include "utils.hpp"

namespace bemu::gb {

enum FlagConditionType { None, Z, NZ, C, NC };

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

    [[nodiscard]] u16 get_HL() const { return get_u16(RegisterType::HL); }
    void set_HL(const u16 value) { set_u16(RegisterType::HL, value); }

    [[nodiscard]] bool check_flags(FlagConditionType condition) const;

    [[nodiscard]] u8 get_u8(RegisterType type) const;
    void set_u8(RegisterType type, u8 value);

    [[nodiscard]] u16 get_u16(RegisterType type) const;
    void set_u16(RegisterType type, u16 value);
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

    u8 fetch_u8();
    u16 fetch_u16();  ///< Little-endian

    void stack_push8(u8 value);
    u8 stack_pop8();
    void stack_push16(u16 value);
    u16 stack_pop16();

    bool step();
    void execute_next_instruction();

    bool execute_ld(const std::string &debug_prefix, u8 opcode);
    bool execute_ld_d(const std::string &debug_prefix, RegisterType reg);
    void execute_ld(u16 address, RegisterType reg);         ///< from reg into address
    void execute_ld(RegisterType reg, u16 address);         ///< from address into reg
    bool execute_ld_r8_r8(const std::string &debug_prefix, RegisterType reg1, RegisterType reg2);  ///< from reg into reg
    // bool execute_ld_r8_hl(const std::string &debug_prefix, RegisterType reg1);  ///< from (HL) into reg

    bool execute_inc(const std::string &debug_prefix, u8 opcode);
    bool execute_inc_r8(const std::string &debug_prefix, RegisterType reg);
    bool execute_inc_r16(const std::string &debug_prefix, RegisterType reg);

    bool execute_dec(const std::string &debug_prefix, u8 opcode);
    bool execute_dec_r8(const std::string &debug_prefix, RegisterType reg);
    bool execute_dec_r16(const std::string &debug_prefix, RegisterType reg);

    bool execute_xor(const std::string &debug_prefix, u8 opcode);
    void execute_xor_reg(const std::string &debug_prefix, RegisterType input_reg);

    bool execute_cp(const std::string &debug_prefix, u8 opcode);
    void execute_cp(const std::string &debug_prefix, RegisterType reg);

    bool execute_and(const std::string &debug_prefix, u8 opcode);
    bool execute_and(const std::string &debug_prefix, RegisterType reg);

    bool execute_sub(const std::string &debug_prefix, u8 opcode);
    bool execute_sub(const std::string &debug_prefix, RegisterType reg);

    bool execute_jp(const std::string &debug_prefix, u8 opcode);
    bool execute_jp_a16(const std::string &debug_prefix, FlagConditionType condition);
    bool execute_jr_e8(const std::string &debug_prefix, FlagConditionType condition);
    bool execute_jp(FlagConditionType condition, u16 address);

    bool execute_call(const std::string &debug_prefix, u8 opcode);
    bool execute_call(const std::string &debug_prefix, FlagConditionType condition);
};
}  // namespace bemu::gb
