#pragma once
#include <vector>

#include "../types.hpp"
#include "interfaces.hpp"

namespace bemu::gb {
template <u16 Begin, u16 End, u8 Value = 0x00>
struct NoopRegion final : IMemoryRegion {
    static_assert(Begin < End, "Begin must be less than End");

    [[nodiscard]] bool contains(const u16 address) const override { return Begin <= address && address <= End; }
    [[nodiscard]] u8 read(const u16 address) const override { return Value; }
    void write(u16 address, u8 value) override {}
};

struct MemoryMap : IMemoryRegion {
    std::vector<IMemoryRegion*> m_regions;

    [[nodiscard]] bool contains(u16 address) const override;
    [[nodiscard]] u8 read(u16 address) const override;
    void write(u16 address, u8 value) override;
};

struct MemoryBus {
    explicit MemoryBus(ICycler* cycler = nullptr);

    void add_region(IMemoryRegion& region);

    [[nodiscard]] u8 peek_u8(u16 address) const;
    [[nodiscard]] u16 peek_u16(u16 address) const;
    [[nodiscard]] u8 read_u8(u16 address) const;
    [[nodiscard]] u16 read_u16(u16 address) const;
    void emplace_u8(u16 address, u8 value);
    void emplace_u16(u16 address, u16 value);
    void write_u8(u16 address, u8 value);
    void write_u16(u16 address, u16 value);

   private:
    ICycler* m_cycler = nullptr;
    MemoryMap m_map;
};
}  // namespace bemu::gb
