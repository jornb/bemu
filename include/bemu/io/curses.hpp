#pragma once
#include <chrono>
#include <unordered_map>

#include "keyboard.hpp"

namespace bemu {
/// Track key presses and render to terminal using ncurses
///
/// When pressing and holding X, getch() will first return 'X', then next frame ERR. Then after about 500ms, 'X' again
/// every 50ms or so.
///
/// Keys are taken from ncurses KEY_DOWN, KEY_x, etc..
struct CursesKeys {
    explicit CursesKeys(IKeyReceiver &receiver) : m_receiver(receiver) {}

    void update();

    [[nodiscard]] bool is_key_pressed(Key key) const;

   private:
    struct Entry {
        bool m_held = false;
        bool m_first = false;
        std::chrono::high_resolution_clock::time_point m_timestamp;
    };

    IKeyReceiver &m_receiver;
    std::unordered_map<Key, Entry> m_last_key_press;
    std::chrono::milliseconds m_timeout_first{60};
    std::chrono::milliseconds m_timeout_second{60};
};
}  // namespace bemu::gb
