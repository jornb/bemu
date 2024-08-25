#include <spdlog/fmt/fmt.h>

#include <bemu/gb/cpu.hpp>
#include <magic_enum.hpp>
#include <stdexcept>

using namespace bemu::gb;

bemu::u8 CpuRegisters::get_u8(RegisterType type) {
    switch (type) {
        case RegisterType::A:
            return a;
        case RegisterType::F:
            return f;
        case RegisterType::B:
            return b;
        case RegisterType::C:
            return c;
        case RegisterType::D:
            return d;
        case RegisterType::E:
            return e;
        case RegisterType::H:
            return h;
        case RegisterType::L:
            return l;
        default:
            throw std::runtime_error(fmt::format("Unknown CPU register {}", magic_enum::enum_name(type)));
    }
}

void CpuRegisters::set_u8(RegisterType type, u8 value) {
    switch (type) {
        case RegisterType::A:
            a = value;
        case RegisterType::F:
            f = value;
        case RegisterType::B:
            b = value;
        case RegisterType::C:
            c = value;
        case RegisterType::D:
            d = value;
        case RegisterType::E:
            e = value;
        case RegisterType::H:
            h = value;
        case RegisterType::L:
            l = value;
        default:
            throw std::runtime_error(fmt::format("Unknown CPU register {}", magic_enum::enum_name(type)));
    }
}
