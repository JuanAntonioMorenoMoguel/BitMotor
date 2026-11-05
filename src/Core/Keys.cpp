#include "BitMotor/Core/Keys.h"

#include <array>

namespace BitMotor {

namespace {

// Same order as the enums.
constexpr std::array<std::string_view, 98> KeyNames = {
    "Unknown",   "A",          "B",
    "C",         "D",          "E",
    "F",         "G",          "H",
    "I",         "J",          "K",
    "L",         "M",          "N",
    "O",         "P",          "Q",
    "R",         "S",          "T",
    "U",         "V",          "W",
    "X",         "Y",          "Z",
    "0",         "1",          "2",
    "3",         "4",          "5",
    "6",         "7",          "8",
    "9",         "F1",         "F2",
    "F3",        "F4",         "F5",
    "F6",        "F7",         "F8",
    "F9",        "F10",        "F11",
    "F12",       "Space",      "Enter",
    "Escape",    "Backspace",  "Tab",
    "Left",      "Right",      "Up",
    "Down",      "Left Shift", "Right Shift",
    "Left Ctrl", "Right Ctrl", "Left Alt",
    "Right Alt", "Insert",     "Delete",
    "Home",      "End",        "Page Up",
    "Page Down", "-",          "=",
    "[",         "]",          "\\",
    ";",         "'",          "`",
    ",",         ".",          "/",
    "Caps Lock", "Keypad 0",   "Keypad 1",
    "Keypad 2",  "Keypad 3",   "Keypad 4",
    "Keypad 5",  "Keypad 6",   "Keypad 7",
    "Keypad 8",  "Keypad 9",   "Keypad Enter",
    "Keypad +",  "Keypad -",   "Keypad *",
    "Keypad /",  "Keypad .",
};

constexpr std::array<std::string_view, 25> ButtonNames = {
    "Pad South",
    "Pad East",
    "Pad West",
    "Pad North",
    "Pad Back",
    "Pad Guide",
    "Pad Start",
    "Pad Left Stick",
    "Pad Right Stick",
    "Pad Left Shoulder",
    "Pad Right Shoulder",
    "Pad Dpad Up",
    "Pad Dpad Down",
    "Pad Dpad Left",
    "Pad Dpad Right",
    "Pad Left Trigger",
    "Pad Right Trigger",
    "Pad Left Stick Up",
    "Pad Left Stick Down",
    "Pad Left Stick Left",
    "Pad Left Stick Right",
    "Pad Right Stick Up",
    "Pad Right Stick Down",
    "Pad Right Stick Left",
    "Pad Right Stick Right",
};

constexpr std::array<std::string_view, 5> MouseNames = {
    "Mouse Left", "Mouse Right", "Mouse Middle", "Mouse X1", "Mouse X2",
};

static_assert(KeyNames.size() == std::size_t(Key::Count));
static_assert(ButtonNames.size() == std::size_t(Button::Count));
static_assert(MouseNames.size() == std::size_t(MouseButton::Count));

} // namespace

std::string Name(Control control) {
    std::size_t code = std::size_t(control.Code);
    switch (control.Type) {
    case Control::Kind::Key:
        return code < KeyNames.size() ? std::string(KeyNames[code]) : "?";
    case Control::Kind::Button:
        return code < ButtonNames.size() ? std::string(ButtonNames[code]) : "?";
    case Control::Kind::Mouse:
        return code < MouseNames.size() ? std::string(MouseNames[code]) : "?";
    }
    return "?";
}

bool ControlFromName(std::string_view name, Control& out) {
    for (std::size_t i = 1; i < KeyNames.size(); ++i)
        if (KeyNames[i] == name) return out = Key(i), true;
    for (std::size_t i = 0; i < ButtonNames.size(); ++i)
        if (ButtonNames[i] == name) return out = Button(i), true;
    for (std::size_t i = 0; i < MouseNames.size(); ++i)
        if (MouseNames[i] == name) return out = MouseButton(i), true;
    return false;
}

} // namespace BitMotor
