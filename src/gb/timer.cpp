#include <bemu/gb/timer.hpp>

using namespace bemu::gb;

u8 Timer::read(u16 address) const {
    address -= 0xFF04;

    const auto ptr = reinterpret_cast<const u8*>(this);
    return ptr[address];
}

void Timer::write(u16 address, const u8 value) {
    address -= 0xFF04;

    const auto ptr = reinterpret_cast<u8*>(this);
    ptr[address] = value;
}
