#pragma once
#include <cstddef>

#include "../types.hpp"

namespace bemu::gb {
struct ICycler {
    virtual ~ICycler() = default;

    virtual size_t get_tick_count() const = 0;
    virtual void add_cycles() = 0;
};

struct ICycled {
    virtual ~ICycled() = default;

    virtual void dot_tick() {}
    virtual void cycle_tick() {}
};

struct IMemoryRegion {
    virtual ~IMemoryRegion() = default;

    [[nodiscard]] virtual bool contains(u16 address) const = 0;
    [[nodiscard]] virtual u8 read(u16 address) const = 0;
    virtual void write(u16 address, u8 value) = 0;
};
}  // namespace bemu::gb
