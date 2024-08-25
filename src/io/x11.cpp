#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <bemu/io/x11.hpp>
#include <optional>
#include <stdexcept>
#include <unordered_map>

using namespace bemu;

namespace {
constexpr std::optional<Key> to_key(const KeySym sym) {
    switch (sym) {
        // Control keys
        case XK_Return: return Key::Return;
        case XK_space: return Key::Space;
        case XK_BackSpace: return Key::Backspace;

        // Arrow keys
        case XK_Up: return Key::Up;
        case XK_Down: return Key::Down;
        case XK_Left: return Key::Left;
        case XK_Right: return Key::Right;

        // Symbols
        case XK_plus:
        case XK_equal: return Key::Plus;
        case XK_minus:
        case XK_underscore: return Key::Minus;
        case XK_backslash:
        case XK_bar: return Key::Backslash;

        // Digits
        case XK_0: return Key::Number_0;
        case XK_1: return Key::Number_1;
        case XK_2: return Key::Number_2;
        case XK_3: return Key::Number_3;
        case XK_4: return Key::Number_4;
        case XK_5: return Key::Number_5;
        case XK_6: return Key::Number_6;
        case XK_7: return Key::Number_7;
        case XK_8: return Key::Number_8;
        case XK_9: return Key::Number_9;

        // Letters (upper or lower case)
        case XK_A:
        case XK_a: return Key::A;
        case XK_B:
        case XK_b: return Key::B;
        case XK_C:
        case XK_c: return Key::C;
        case XK_D:
        case XK_d: return Key::D;
        case XK_E:
        case XK_e: return Key::E;
        case XK_F:
        case XK_f: return Key::F;
        case XK_G:
        case XK_g: return Key::G;
        case XK_H:
        case XK_h: return Key::H;
        case XK_I:
        case XK_i: return Key::I;
        case XK_J:
        case XK_j: return Key::J;
        case XK_K:
        case XK_k: return Key::K;
        case XK_L:
        case XK_l: return Key::L;
        case XK_M:
        case XK_m: return Key::M;
        case XK_N:
        case XK_n: return Key::N;
        case XK_O:
        case XK_o: return Key::O;
        case XK_P:
        case XK_p: return Key::P;
        case XK_Q:
        case XK_q: return Key::Q;
        case XK_R:
        case XK_r: return Key::R;
        case XK_S:
        case XK_s: return Key::S;
        case XK_T:
        case XK_t: return Key::T;
        case XK_U:
        case XK_u: return Key::U;
        case XK_V:
        case XK_v: return Key::V;
        case XK_W:
        case XK_w: return Key::W;
        case XK_X:
        case XK_x: return Key::X;
        case XK_Y:
        case XK_y: return Key::Y;
        case XK_Z:
        case XK_z: return Key::Z;

        default: return std::nullopt;
    }
}
}  // namespace

struct X11Keys::impl {
    explicit impl(IKeyReceiver &receiver) : m_receiver(receiver), m_display(XOpenDisplay(nullptr)) {
        if (!m_display) {
            throw std::runtime_error("Failed to open display");
        }
    }

    ~impl() {
        if (m_display) {
            XCloseDisplay(m_display);
        }
    }

    void update() {
        // Read keys
        XQueryKeymap(m_display, m_keys);

        for (int i = 0; i < 256; ++i) {
            const auto key_code = static_cast<KeyCode>(i);
            KeySym sym = XKeycodeToKeysym(m_display, key_code, 0);
            if (sym == NoSymbol) continue;
            const auto key = to_key(sym);
            if (!key) continue;

            const bool pressed = m_keys[key_code >> 3] & (1 << (key_code & 7));

            const auto was_pressed = m_key_states.contains(*key) && m_key_states[*key];
            if (pressed && !was_pressed) {
                m_receiver.on_key_pressed(*key);
            } else if (!pressed && was_pressed) {
                m_receiver.on_key_released(*key);
            }

            m_key_states[*key] = pressed;
        }
    }

    [[nodiscard]] bool is_key_pressed(const Key key) const {
        return m_key_states.contains(key) && m_key_states.at(key);
    }

    IKeyReceiver &m_receiver;
    Display *m_display = nullptr;
    std::unordered_map<Key, bool> m_key_states;
    char m_keys[32];
};

X11Keys::X11Keys(IKeyReceiver &receiver) : m_impl(std::make_unique<impl>(receiver)) {}

X11Keys::~X11Keys() = default;

void X11Keys::update() { return m_impl->update(); }

[[nodiscard]] bool X11Keys::is_key_pressed(const Key key) const { return m_impl->is_key_pressed(key); }
