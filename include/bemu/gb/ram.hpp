#pragma once
#include <array>
#include <span>
#include <stdexcept>

#include "../types.hpp"
#include "interfaces.hpp"

namespace bemu::gb {
/// Blob of contiguous data mapped to memory, starting at address Begin
///
/// \warning The struct TStruct must be packed (no padding bytes) and have an alignment of 1. For example:
///     \code cpp
///     #pragma pack(push, 1)
///     struct SerialRegisters {
///         u8 m_data;
///         u8 control;
///     };
///     #pragma pack(pop)
///     \endcode
/// Note that the requires clause requires(alignof(TStruct) == 1) does not guarantee that the struct is packed, so you
/// must still ensure that yourself.
template <size_t Begin, typename TStruct>
    requires(alignof(TStruct) == 1)
struct MemoryRegion : IMemoryRegion {
    [[nodiscard]] bool contains(const u16 address) const override {
        return Begin <= address && address < Begin + sizeof(TStruct);
    }

    [[nodiscard]] u8 read(const u16 address) const override {
        if (address < Begin || address >= Begin + sizeof(TStruct)) {
            throw std::runtime_error("Invalid memory access");
        }
        const auto ptr = reinterpret_cast<const u8*>(&m_data);
        return ptr[address - Begin];
    }

    void write(const u16 address, const u8 value) override {
        if (address < Begin || address >= Begin + sizeof(TStruct)) {
            throw std::runtime_error("Invalid memory access");
        }
        const auto ptr = reinterpret_cast<u8*>(&m_data);
        ptr[address - Begin] = value;
    }

    std::span<u8> data() { return {reinterpret_cast<u8*>(&m_data), sizeof(TStruct)}; }

    void serialize(auto& ar) { ar(data()); }

    TStruct m_data;
};

/// Blob of contiguous data
template <size_t Begin, size_t End>
struct RAM : IMemoryRegion {
    constexpr static size_t first_address = Begin;

    [[nodiscard]] bool contains(const u16 address) const override { return Begin <= address && address <= End; }

    [[nodiscard]] u8 read(const u16 address) const override { return m_data.at(address - Begin); }

    void write(const u16 address, const u8 value) override { m_data.at(address - Begin) = value; }

    std::span<u8> data() { return m_data; }

    void serialize(auto& ar) { ar(m_data); }

   private:
    std::array<u8, End - Begin + 1> m_data;
};

struct WRAM : IMemoryRegion {
    bool contains(const u16 address) const override { return switchable().contains(address) || address == 0xFF70; }

    [[nodiscard]] u8 read(const u16 address) const override {
        if (address == 0xFF70) {
            return m_selected_bank;
        }

        return switchable().read(address);
    }
    void write(const u16 address, const u8 value) override {
        if (address == 0xFF70) {
            m_selected_bank = value;
            return;
        }

        switchable().write(address, value);
    }

    RAM<0xD000, 0xDFFF>& switchable() {
        // Writing 0 maps bank 1 instead
        auto s = m_selected_bank & 0b111;
        if (s == 0) s = 1;

        return m_switchable.at(s);
    }

    [[nodiscard]] const RAM<0xD000, 0xDFFF>& switchable() const {
        // Writing 0 maps bank 1 instead
        auto s = m_selected_bank & 0b111;
        if (s == 0) s = 1;

        return m_switchable.at(s);
    }

    void serialize(auto& ar) {
        for (auto& bank : m_switchable) {
            bank.serialize(ar);
        }

        ar(m_selected_bank);
    }

   private:
    std::array<RAM<0xD000, 0xDFFF>, 7> m_switchable{};
    u8 m_selected_bank = 1;
};

}  // namespace bemu::gb
