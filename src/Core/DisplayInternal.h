#pragma once
#include "BitMotor/Math/Shapes.h"

struct SDL_Window;
struct SDL_Renderer;

namespace BitMotor::Detail {

// The loop gives the window to Window and Screen after creating it (nullptr when it closes).
void AttachDisplay(SDL_Window* window, SDL_Renderer* renderer, int logicalWidth, int logicalHeight, bool vsync);
void SetFocused(bool focused);
SDL_Renderer* CurrentRenderer();

// Where a logical screen of `logical` size sits inside a window of `window` pixels: as big as fits,
// centred. A zero logical size fills the window.
Rect Letterbox(Vec2 window, Vec2 logical);

} // namespace BitMotor::Detail
