#pragma once
#include "screen.hpp"

namespace bemu {
struct IEmulator {
    virtual ~IEmulator() = default;

    virtual Screen& get_screen() = 0;
    virtual const Screen& get_screen() const = 0;
};
}  // namespace bemu
