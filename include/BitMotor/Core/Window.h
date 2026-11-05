#pragma once
#include "BitMotor/Math/Vector.h"

#include <string>
#include <string_view>

// The game's window. Before Run opens it, these do nothing (use Config instead).
//     if (Input::KeyPressed(Key::F11)) Window::ToggleFullscreen();
//     Subscription s = Events::On<WindowResized>([](const WindowResized& e) { ... });
namespace BitMotor {

// Sent when the window's size in pixels changes (resize, fullscreen, moving to another monitor).
struct WindowResized {
    int Width = 0, Height = 0;
};

// Sent when the window gains or loses the keyboard focus (pause the game on false).
struct FocusChanged {
    bool Focused = false;
};

namespace Window {

void SetTitle(std::string_view title);
std::string Title();

// Size in real pixels (bigger than the requested size on high-DPI screens).
Vec2 Size();
// Changes the window size (not in fullscreen).
void SetSize(int width, int height);

// Fullscreen without changing the monitor's mode (borderless).
void SetFullscreen(bool fullscreen);
void ToggleFullscreen();
bool IsFullscreen();

void SetVSync(bool vsync);
bool VSync();

bool Focused();

} // namespace Window

} // namespace BitMotor
