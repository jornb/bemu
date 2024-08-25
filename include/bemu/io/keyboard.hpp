#pragma once

namespace bemu {
/// Keyboard keys across curses/x11/etc.
enum class Key {
    Return,
    Space,
    Backspace,
    Up,
    Down,
    Left,
    Right,
    Plus,
    Minus,
    Backslash,
    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,
    Number_0,
    Number_1,
    Number_2,
    Number_3,
    Number_4,
    Number_5,
    Number_6,
    Number_7,
    Number_8,
    Number_9
};

struct IKeyReceiver {
    virtual ~IKeyReceiver() = default;
    virtual void on_key_pressed(Key key) = 0;
    virtual void on_key_released(Key key) = 0;
};
}  // namespace bemu::gb