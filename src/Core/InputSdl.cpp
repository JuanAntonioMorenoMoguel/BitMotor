// SDL's events -> Input (Input.cpp keeps the state). Also owns the gamepads.
#include "BitMotor/Core/Events.h"
#include "BitMotor/Core/Input.h"
#include "BitMotor/Core/Log.h"
#include "BitMotor/Core/Screen.h"
#include "BitMotor/Core/Settings.h"
#include "Core/DisplayInternal.h"
#include "Core/InputInternal.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace BitMotor {

namespace {

std::vector<SDL_Gamepad*> Pads;
SDL_Gamepad* ActivePad = nullptr;

Key FromScancode(SDL_Scancode code) {
    switch (code) {
    case SDL_SCANCODE_A:
        return Key::A;
    case SDL_SCANCODE_B:
        return Key::B;
    case SDL_SCANCODE_C:
        return Key::C;
    case SDL_SCANCODE_D:
        return Key::D;
    case SDL_SCANCODE_E:
        return Key::E;
    case SDL_SCANCODE_F:
        return Key::F;
    case SDL_SCANCODE_G:
        return Key::G;
    case SDL_SCANCODE_H:
        return Key::H;
    case SDL_SCANCODE_I:
        return Key::I;
    case SDL_SCANCODE_J:
        return Key::J;
    case SDL_SCANCODE_K:
        return Key::K;
    case SDL_SCANCODE_L:
        return Key::L;
    case SDL_SCANCODE_M:
        return Key::M;
    case SDL_SCANCODE_N:
        return Key::N;
    case SDL_SCANCODE_O:
        return Key::O;
    case SDL_SCANCODE_P:
        return Key::P;
    case SDL_SCANCODE_Q:
        return Key::Q;
    case SDL_SCANCODE_R:
        return Key::R;
    case SDL_SCANCODE_S:
        return Key::S;
    case SDL_SCANCODE_T:
        return Key::T;
    case SDL_SCANCODE_U:
        return Key::U;
    case SDL_SCANCODE_V:
        return Key::V;
    case SDL_SCANCODE_W:
        return Key::W;
    case SDL_SCANCODE_X:
        return Key::X;
    case SDL_SCANCODE_Y:
        return Key::Y;
    case SDL_SCANCODE_Z:
        return Key::Z;
    case SDL_SCANCODE_0:
        return Key::Num0;
    case SDL_SCANCODE_1:
        return Key::Num1;
    case SDL_SCANCODE_2:
        return Key::Num2;
    case SDL_SCANCODE_3:
        return Key::Num3;
    case SDL_SCANCODE_4:
        return Key::Num4;
    case SDL_SCANCODE_5:
        return Key::Num5;
    case SDL_SCANCODE_6:
        return Key::Num6;
    case SDL_SCANCODE_7:
        return Key::Num7;
    case SDL_SCANCODE_8:
        return Key::Num8;
    case SDL_SCANCODE_9:
        return Key::Num9;
    case SDL_SCANCODE_F1:
        return Key::F1;
    case SDL_SCANCODE_F2:
        return Key::F2;
    case SDL_SCANCODE_F3:
        return Key::F3;
    case SDL_SCANCODE_F4:
        return Key::F4;
    case SDL_SCANCODE_F5:
        return Key::F5;
    case SDL_SCANCODE_F6:
        return Key::F6;
    case SDL_SCANCODE_F7:
        return Key::F7;
    case SDL_SCANCODE_F8:
        return Key::F8;
    case SDL_SCANCODE_F9:
        return Key::F9;
    case SDL_SCANCODE_F10:
        return Key::F10;
    case SDL_SCANCODE_F11:
        return Key::F11;
    case SDL_SCANCODE_F12:
        return Key::F12;
    case SDL_SCANCODE_SPACE:
        return Key::Space;
    case SDL_SCANCODE_RETURN:
        return Key::Enter;
    case SDL_SCANCODE_ESCAPE:
        return Key::Escape;
    case SDL_SCANCODE_BACKSPACE:
        return Key::Backspace;
    case SDL_SCANCODE_TAB:
        return Key::Tab;
    case SDL_SCANCODE_LEFT:
        return Key::Left;
    case SDL_SCANCODE_RIGHT:
        return Key::Right;
    case SDL_SCANCODE_UP:
        return Key::Up;
    case SDL_SCANCODE_DOWN:
        return Key::Down;
    case SDL_SCANCODE_LSHIFT:
        return Key::LeftShift;
    case SDL_SCANCODE_RSHIFT:
        return Key::RightShift;
    case SDL_SCANCODE_LCTRL:
        return Key::LeftCtrl;
    case SDL_SCANCODE_RCTRL:
        return Key::RightCtrl;
    case SDL_SCANCODE_LALT:
        return Key::LeftAlt;
    case SDL_SCANCODE_RALT:
        return Key::RightAlt;
    case SDL_SCANCODE_INSERT:
        return Key::Insert;
    case SDL_SCANCODE_DELETE:
        return Key::Delete;
    case SDL_SCANCODE_HOME:
        return Key::Home;
    case SDL_SCANCODE_END:
        return Key::End;
    case SDL_SCANCODE_PAGEUP:
        return Key::PageUp;
    case SDL_SCANCODE_PAGEDOWN:
        return Key::PageDown;
    case SDL_SCANCODE_MINUS:
        return Key::Minus;
    case SDL_SCANCODE_EQUALS:
        return Key::Equals;
    case SDL_SCANCODE_LEFTBRACKET:
        return Key::LeftBracket;
    case SDL_SCANCODE_RIGHTBRACKET:
        return Key::RightBracket;
    case SDL_SCANCODE_BACKSLASH:
        return Key::Backslash;
    case SDL_SCANCODE_SEMICOLON:
        return Key::Semicolon;
    case SDL_SCANCODE_APOSTROPHE:
        return Key::Apostrophe;
    case SDL_SCANCODE_GRAVE:
        return Key::Grave;
    case SDL_SCANCODE_COMMA:
        return Key::Comma;
    case SDL_SCANCODE_PERIOD:
        return Key::Period;
    case SDL_SCANCODE_SLASH:
        return Key::Slash;
    case SDL_SCANCODE_CAPSLOCK:
        return Key::CapsLock;
    case SDL_SCANCODE_KP_0:
        return Key::Keypad0;
    case SDL_SCANCODE_KP_1:
        return Key::Keypad1;
    case SDL_SCANCODE_KP_2:
        return Key::Keypad2;
    case SDL_SCANCODE_KP_3:
        return Key::Keypad3;
    case SDL_SCANCODE_KP_4:
        return Key::Keypad4;
    case SDL_SCANCODE_KP_5:
        return Key::Keypad5;
    case SDL_SCANCODE_KP_6:
        return Key::Keypad6;
    case SDL_SCANCODE_KP_7:
        return Key::Keypad7;
    case SDL_SCANCODE_KP_8:
        return Key::Keypad8;
    case SDL_SCANCODE_KP_9:
        return Key::Keypad9;
    case SDL_SCANCODE_KP_ENTER:
        return Key::KeypadEnter;
    case SDL_SCANCODE_KP_PLUS:
        return Key::KeypadPlus;
    case SDL_SCANCODE_KP_MINUS:
        return Key::KeypadMinus;
    case SDL_SCANCODE_KP_MULTIPLY:
        return Key::KeypadMultiply;
    case SDL_SCANCODE_KP_DIVIDE:
        return Key::KeypadDivide;
    case SDL_SCANCODE_KP_PERIOD:
        return Key::KeypadPeriod;
    default:
        return Key::Unknown;
    }
}

bool FromPadButton(Uint8 button, Button& out) {
    switch (button) {
    case SDL_GAMEPAD_BUTTON_SOUTH:
        out = Button::South;
        return true;
    case SDL_GAMEPAD_BUTTON_EAST:
        out = Button::East;
        return true;
    case SDL_GAMEPAD_BUTTON_WEST:
        out = Button::West;
        return true;
    case SDL_GAMEPAD_BUTTON_NORTH:
        out = Button::North;
        return true;
    case SDL_GAMEPAD_BUTTON_BACK:
        out = Button::Back;
        return true;
    case SDL_GAMEPAD_BUTTON_GUIDE:
        out = Button::Guide;
        return true;
    case SDL_GAMEPAD_BUTTON_START:
        out = Button::Start;
        return true;
    case SDL_GAMEPAD_BUTTON_LEFT_STICK:
        out = Button::LeftStick;
        return true;
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
        out = Button::RightStick;
        return true;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
        out = Button::LeftShoulder;
        return true;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
        out = Button::RightShoulder;
        return true;
    case SDL_GAMEPAD_BUTTON_DPAD_UP:
        out = Button::DpadUp;
        return true;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
        out = Button::DpadDown;
        return true;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
        out = Button::DpadLeft;
        return true;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
        out = Button::DpadRight;
        return true;
    default:
        return false;
    }
}

bool FromPadAxis(Uint8 axis, Detail::PadAxis& out) {
    switch (axis) {
    case SDL_GAMEPAD_AXIS_LEFTX:
        out = Detail::PadAxis::LeftX;
        return true;
    case SDL_GAMEPAD_AXIS_LEFTY:
        out = Detail::PadAxis::LeftY;
        return true;
    case SDL_GAMEPAD_AXIS_RIGHTX:
        out = Detail::PadAxis::RightX;
        return true;
    case SDL_GAMEPAD_AXIS_RIGHTY:
        out = Detail::PadAxis::RightY;
        return true;
    case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
        out = Detail::PadAxis::LeftTrigger;
        return true;
    case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
        out = Detail::PadAxis::RightTrigger;
        return true;
    default:
        return false;
    }
}

bool FromMouseButton(Uint8 button, MouseButton& out) {
    switch (button) {
    case SDL_BUTTON_LEFT:
        out = MouseButton::Left;
        return true;
    case SDL_BUTTON_RIGHT:
        out = MouseButton::Right;
        return true;
    case SDL_BUTTON_MIDDLE:
        out = MouseButton::Middle;
        return true;
    case SDL_BUTTON_X1:
        out = MouseButton::X1;
        return true;
    case SDL_BUTTON_X2:
        out = MouseButton::X2;
        return true;
    default:
        return false;
    }
}

// SDL gives the mouse in window points; Screen works in pixels.
Vec2 MouseToLogical(float x, float y) {
    SDL_Window* window = Detail::CurrentWindow();
    float density = window ? SDL_GetWindowPixelDensity(window) : 1.0f;
    if (density <= 0) density = 1;
    return Screen::ToLogical(Vec2{x, y} * density);
}

void UsePad(SDL_JoystickID id) {
    if (SDL_Gamepad* pad = SDL_GetGamepadFromID(id)) ActivePad = pad;
}

void OpenPad(SDL_JoystickID id) {
    if (SDL_GetGamepadFromID(id)) return; // already open
    SDL_Gamepad* pad = SDL_OpenGamepad(id);
    if (!pad) {
        Log::Warn("Cannot open gamepad: {}", SDL_GetError());
        return;
    }
    Pads.push_back(pad);
    ActivePad = pad;
    Detail::InputPadConnected(true);
    const char* name = SDL_GetGamepadName(pad);
    Log::Info("Gamepad connected: {}", name ? name : "?");
    Events::Emit(GamepadConnected{name ? name : ""});
}

void ClosePad(SDL_JoystickID id) {
    SDL_Gamepad* pad = SDL_GetGamepadFromID(id);
    if (!pad) return;
    const char* found = SDL_GetGamepadName(pad);
    std::string name = found ? found : "";
    std::erase(Pads, pad);
    SDL_CloseGamepad(pad);
    if (ActivePad == pad) ActivePad = Pads.empty() ? nullptr : Pads.back();
    Detail::InputPadConnected(!Pads.empty());
    Log::Info("Gamepad disconnected: {}", name);
    Events::Emit(GamepadDisconnected{name});
}

} // namespace

namespace Detail {

void StartInput() {
    ResetInput();
    Pads.clear();
    ActivePad = nullptr;
}

void StopInput() {
    for (SDL_Gamepad* pad : Pads) SDL_CloseGamepad(pad);
    Pads.clear();
    ActivePad = nullptr;
}

void HandleInputEvent(const SDL_Event& event) {
    switch (event.type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        if (!event.key.repeat)
            InputKey(FromScancode(event.key.scancode), event.type == SDL_EVENT_KEY_DOWN);
        else if (event.type == SDL_EVENT_KEY_DOWN)
            InputKeyRepeat(FromScancode(event.key.scancode));
        break;
    case SDL_EVENT_TEXT_INPUT:
        if (event.text.text) InputText(event.text.text);
        break;
    case SDL_EVENT_MOUSE_MOTION:
        if (!Input::MouseForced()) InputMouseMove(MouseToLogical(event.motion.x, event.motion.y));
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        MouseButton button = MouseButton::Left;
        if (!Input::MouseForced() && FromMouseButton(event.button.button, button))
            InputMouseButton(button, event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
        break;
    }
    case SDL_EVENT_MOUSE_WHEEL: {
        float amount = event.wheel.y;
        if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) amount = -amount;
        InputMouseWheel(amount);
        break;
    }
    case SDL_EVENT_GAMEPAD_ADDED:
        OpenPad(event.gdevice.which);
        break;
    case SDL_EVENT_GAMEPAD_REMOVED:
        ClosePad(event.gdevice.which);
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP: {
        UsePad(event.gbutton.which);
        Button button = Button::South;
        if (FromPadButton(event.gbutton.button, button))
            InputPadButton(button, event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
        break;
    }
    case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
        PadAxis axis = PadAxis::LeftX;
        if (!FromPadAxis(event.gaxis.axis, axis)) break;
        float value = std::max(float(event.gaxis.value) / 32767.0f, -1.0f);
        if (std::abs(value) > StickDeadZone) UsePad(event.gaxis.which);
        InputPadAxis(axis, value);
        break;
    }
    default:
        break;
    }
}

void SetTextInput(bool on) {
    SDL_Window* window = CurrentWindow();
    if (!window || SDL_TextInputActive(window) == on) return;
    if (on)
        SDL_StartTextInput(window);
    else
        SDL_StopTextInput(window);
}

std::string ClipboardText() {
    if (!CurrentWindow()) return {};
    char* text = SDL_GetClipboardText();
    std::string out = text ? text : "";
    SDL_free(text);
    return out;
}

void SetClipboardText(const std::string& text) {
    if (CurrentWindow()) SDL_SetClipboardText(text.c_str());
}

} // namespace Detail

namespace Input {

void Rumble(float low, float high, float seconds) {
    if (!ActivePad || seconds <= 0) return;
    // The player's Vibration setting scales both motors (0: no rumble at all).
    const float scale = std::clamp(Settings::Get().Vibration, 0.0f, 1.0f);
    if (scale <= 0) return;
    auto strength = [scale](float v) { return Uint16(std::clamp(v, 0.0f, 1.0f) * scale * 65535.0f); };
    SDL_RumbleGamepad(ActivePad, strength(low), strength(high), Uint32(seconds * 1000.0f));
}

} // namespace Input

} // namespace BitMotor
