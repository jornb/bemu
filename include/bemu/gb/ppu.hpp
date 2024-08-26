
#pragma once

#include "lcd.hpp"
#include "types.hpp"

namespace bemu::gb {

struct Ppu {
    Lcd m_lcd;

    void tick();
};

}  // namespace bemu::gb
