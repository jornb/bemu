#pragma once
#include <memory>

#include "keyboard.hpp"

namespace bemu {
struct X11Keys {
    explicit X11Keys(IKeyReceiver &receiver);
    ~X11Keys();
    void update();
    [[nodiscard]] bool is_key_pressed(Key key) const;

   private:
    struct impl;
    std::unique_ptr<impl> m_impl;
};
}  // namespace bemu::gb
