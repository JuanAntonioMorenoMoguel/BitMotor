#pragma once
#include "BitMotor/Math/Shapes.h"

struct SDL_Window;

namespace BitMotor::Detail {

// The loop gives the window to Window and Screen after creating it (nullptr when it closes).
void AttachDisplay(SDL_Window* window, int logicalWidth, int logicalHeight, bool vsync, bool integerScaling);
void SetFocused(bool focused);
SDL_Window* CurrentWindow();

// Where a logical screen of `logical` size sits inside a window of `window` pixels: as big as fits,
// centred. `integer`: only whole scales (x1, x2...) and whole-pixel corners, unless the window is smaller
// than the logical screen. A zero logical size fills the window.
Rect Letterbox(Vec2 window, Vec2 logical, bool integer = false);
// Letterbox with this game's logical size and Config::IntegerScaling.
Rect ViewportFor(Vec2 window);

} // namespace BitMotor::Detail
