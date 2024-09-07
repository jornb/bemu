#pragma once

#include <array>

#include "types.hpp"

namespace bemu::gb {

/// Blob of contiguous data
template <size_t Begin, typename TStruct>
struct MemoryRegion {
    bool contains(const u16 address) const { return Begin <= address && address < Begin + sizeof(TStruct); }

    [[nodiscard]] u8 read_memory(const u16 address) const {
        auto self = static_cast<const TStruct*>(this);
        const auto ptr = reinterpret_cast<const u8*>(self);
        return ptr[address - Begin];
    }

    void write_memory(const u16 address, const u8 value) {
        auto self = static_cast<TStruct*>(this);
        const auto ptr = reinterpret_cast<u8*>(self);
        ptr[address - Begin] = value;
    }
};

/// Blob of contiguous data
template <size_t Begin, size_t End>
struct RAM {
    bool contains(const u16 address) const { return Begin <= address && address <= End; }

    [[nodiscard]] u8 read(const u16 address) const { return m_data.at(address - Begin); }

    void write(const u16 address, const u8 value) { m_data.at(address - Begin) = value; }

    const u8* data() { return m_data.data(); }

   private:
    std::array<u8, End - Begin + 1> m_data;
};

// TODO: Add support for 0xFF70 in game boy color mode. Swappable banks 1-7.
struct WRAM {
    bool contains(u16 address) const {
        return m_fixed.contains(address) || switchable().contains(address) || address == 0xFF70;
    }

    [[nodiscard]] u8 read(const u16 address) const {
        if (address == 0xFF70) {
            return m_selected_bank;
        }

        if (m_fixed.contains(address)) {
            return m_fixed.read(address);
        }

        return switchable().read(address);
    }
    void write(const u16 address, const u8 value) {
        if (address == 0xFF70) {
            m_selected_bank = value;
            return;
        }

        if (m_fixed.contains(address)) {
            m_fixed.write(address, value);
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

   private:
    RAM<0xC000, 0xCFFF> m_fixed{};
    std::array<RAM<0xD000, 0xDFFF>, 7> m_switchable{};
    u8 m_selected_bank = 1;
};

}  // namespace bemu::gb
