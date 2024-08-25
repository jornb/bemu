#include <ncurses.h>

#include <bemu/io/curses.hpp>
#include <optional>

using namespace bemu;

namespace {
constexpr std::optional<Key> to_key(const int ch) {
    if (ch >= 'a' && ch <= 'z') {
        return static_cast<Key>(static_cast<int>(Key::A) + (ch - 'a'));
    }
    if (ch >= 'A' && ch <= 'Z') {
        return static_cast<Key>(static_cast<int>(Key::A) + (ch - 'A'));
    }
    if (ch >= '0' && ch <= '9') {
        return static_cast<Key>(static_cast<int>(Key::Number_0) + (ch - '0'));
    }
    if (ch == KEY_UP) {
        return Key::Up;
    }
    if (ch == KEY_DOWN) {
        return Key::Down;
    }
    if (ch == KEY_LEFT) {
        return Key::Left;
    }
    if (ch == KEY_RIGHT) {
        return Key::Right;
    }
    if (ch == KEY_BACKSPACE || ch == 127) {
        return Key::Backspace;
    }
    if (ch == KEY_ENTER || ch == '\n' || ch == '\r') {
        return Key::Return;
    }
    if (ch == ' ') {
        return Key::Space;
    }
    if (ch == '+') {
        return Key::Plus;
    }
    if (ch == '-') {
        return Key::Minus;
    }
    if (ch == '\\') {
        return Key::Backslash;
    }
    return std::nullopt;
}
}  // namespace

void CursesKeys::update() {
    const auto now = std::chrono::high_resolution_clock::now();

    // Drain events
    int ch;
    while ((ch = getch()) != ERR) {
        const auto key = to_key(ch);
        if (!key) continue;

        if (!m_last_key_press[*key].m_held) {
            m_last_key_press[*key].m_first = true;
            m_receiver.on_key_pressed(*key);
        } else {
            m_last_key_press[*key].m_first = false;
        }
        m_last_key_press[*key].m_held = true;
        m_last_key_press[*key].m_timestamp = now;
    }

    // Check for released keys
    for (auto &[key, entry] : m_last_key_press) {
        if (entry.m_held && now - entry.m_timestamp > (entry.m_first ? m_timeout_first : m_timeout_second)) {
            entry.m_held = false;
            m_receiver.on_key_released(key);
        }
    }
}

bool CursesKeys::is_key_pressed(const Key key) const {
    return m_last_key_press.contains(key) && m_last_key_press.at(key).m_held;
}
