#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <bemu/utils.hpp>
#include <algorithm>
#include <bemu/gb/memory.hpp>

using namespace bemu;
using namespace bemu::gb;

bool MemoryMap::contains(u16 address) const {
    return std::ranges::any_of(m_regions, [&](const auto &region) { return region->contains(address); });
}

u8 MemoryMap::read(const u16 address) const {
    for (const auto &region : m_regions) {
        if (region->contains(address)) {
            return region->read(address);
        }
    }

    // throw std::runtime_error(fmt::format("Address not mapped (read): 0x{:04x}", address));
    spdlog::error("Unsupported memory address (read) {:04x}", address);
    return 0x00;
}

void MemoryMap::write(const u16 address, const u8 value) {
    for (const auto &region : m_regions) {
        if (region->contains(address)) {
            region->write(address, value);
            return;
        }
    }

    // throw std::runtime_error(fmt::format("Address not mapped (write): 0x{:04x}", address));
    spdlog::error("Unsupported memory address (write) {:04x}", address);
}

MemoryBus::MemoryBus(ICycler *cycler) : m_cycler(cycler) {}

void MemoryBus::add_region(IMemoryRegion &region) { m_map.m_regions.push_back(&region); }

u8 MemoryBus::peek_u8(const u16 address) const { return m_map.read(address); }

u16 MemoryBus::peek_u16(const u16 address) const {
    const auto lo = peek_u8(address);
    const auto hi = peek_u8(address + 1);
    return combine_bytes(hi, lo);
}

u8 MemoryBus::read_u8(const u16 address) const {
    const auto result = peek_u8(address);
    if (m_cycler) {
        m_cycler->add_cycles();
    }
    return result;
}

u16 MemoryBus::read_u16(const u16 address) const {
    const auto lo = read_u8(address);
    const auto hi = read_u8(address + 1);
    return combine_bytes(hi, lo);
}

void MemoryBus::emplace_u8(const u16 address, const u8 value) { m_map.write(address, value); }

void MemoryBus::emplace_u16(const u16 address, const u16 value) {
    auto [hi, lo] = split_bytes(value);
    emplace_u8(address, lo);
    emplace_u8(address + 1, hi);
}

void MemoryBus::write_u8(const u16 address, const u8 value) {
    m_map.write(address, value);
    if (m_cycler) {
        m_cycler->add_cycles();
    }
}

void MemoryBus::write_u16(const u16 address, const u16 value) {
    auto [hi, lo] = split_bytes(value);
    write_u8(address, lo);
    write_u8(address + 1, hi);
}
