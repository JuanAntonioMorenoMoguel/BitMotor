#pragma once
#include <string>
#include <string_view>

// The physical controls BitMotor knows. Keys are positions on the keyboard (a French AZERTY player
// pressing the key in the place of W gets Key::W), so WASD works on every layout.
namespace BitMotor {

// clang-format off
enum class Key {
    Unknown,
    A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    Space, Enter, Escape, Backspace, Tab,
    Left, Right, Up, Down,
    LeftShift, RightShift, LeftCtrl, RightCtrl, LeftAlt, RightAlt,
    Insert, Delete, Home, End, PageUp, PageDown,
    Minus, Equals, LeftBracket, RightBracket, Backslash, Semicolon, Apostrophe, Grave, Comma, Period, Slash,
    CapsLock,
    Keypad0, Keypad1, Keypad2, Keypad3, Keypad4, Keypad5, Keypad6, Keypad7, Keypad8, Keypad9,
    KeypadEnter, KeypadPlus, KeypadMinus, KeypadMultiply, KeypadDivide, KeypadPeriod,
    Count
};

// Gamepad controls by position (South is A on Xbox, Cross on PlayStation, B on Nintendo).
// Triggers and stick directions also work as buttons, with an analog strength for Input::Axis.
enum class Button {
    South, East, West, North,
    Back, Guide, Start,
    LeftStick, RightStick, LeftShoulder, RightShoulder,
    DpadUp, DpadDown, DpadLeft, DpadRight,
    LeftTrigger, RightTrigger,
    LeftStickUp, LeftStickDown, LeftStickLeft, LeftStickRight,
    RightStickUp, RightStickDown, RightStickLeft, RightStickRight,
    Count
};

enum class MouseButton { Left, Right, Middle, X1, X2, Count };
// clang-format on

// Any of the three, so a binding can mix them: {Key::Space, Button::South, MouseButton::Left}.
struct Control {
    enum class Kind { Key, Button, Mouse };
    Kind Type = Kind::Key;
    int Code = 0;

    constexpr Control(BitMotor::Key key) : Type(Kind::Key), Code(int(key)) {}
    constexpr Control(BitMotor::Button button) : Type(Kind::Button), Code(int(button)) {}
    constexpr Control(MouseButton mouse) : Type(Kind::Mouse), Code(int(mouse)) {}

    constexpr bool IsGamepad() const { return Type == Kind::Button; }
    constexpr bool operator==(const Control&) const = default;
};

// Readable names, also used in the saved controls file: "Space", "Left Shift", "Pad South", "Mouse Left".
std::string Name(Control control);
// The control with that name; false if there is none.
bool ControlFromName(std::string_view name, Control& out);

} // namespace BitMotor
