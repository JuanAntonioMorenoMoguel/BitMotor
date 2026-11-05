// Window and Screen: both work on the window the loop opened.
#include "BitMotor/Core/Screen.h"
#include "BitMotor/Core/Window.h"

#include "Core/DisplayInternal.h"

#include <SDL3/SDL.h>

#include <algorithm>

namespace BitMotor {

namespace {
SDL_Window* TheWindow = nullptr;
SDL_Renderer* Renderer = nullptr;
Vec2 LogicalSize;
bool VSyncOn = true;
bool HasFocus = true;
} // namespace

namespace Detail {

void AttachDisplay(SDL_Window* window, SDL_Renderer* renderer, int logicalWidth, int logicalHeight, bool vsync) {
    TheWindow = window;
    Renderer = renderer;
    LogicalSize = {float(std::max(logicalWidth, 0)), float(std::max(logicalHeight, 0))};
    VSyncOn = vsync;
    HasFocus = true;
}

void SetFocused(bool focused) { HasFocus = focused; }

SDL_Renderer* CurrentRenderer() { return Renderer; }
SDL_Window* CurrentWindow() { return TheWindow; }

Rect Letterbox(Vec2 window, Vec2 logical) {
    if (logical.X <= 0 || logical.Y <= 0 || window.X <= 0 || window.Y <= 0) return {{0, 0}, window};
    float scale = std::min(window.X / logical.X, window.Y / logical.Y);
    Vec2 size = logical * scale;
    return {(window - size) * 0.5f, size};
}

} // namespace Detail

namespace Window {

void SetTitle(std::string_view title) {
    if (TheWindow) SDL_SetWindowTitle(TheWindow, std::string(title).c_str());
}

std::string Title() { return TheWindow ? SDL_GetWindowTitle(TheWindow) : ""; }

Vec2 Size() {
    int width = 0, height = 0;
    if (TheWindow) SDL_GetWindowSizeInPixels(TheWindow, &width, &height);
    return {float(width), float(height)};
}

void SetSize(int width, int height) {
    if (TheWindow && !IsFullscreen()) SDL_SetWindowSize(TheWindow, width, height);
}

void SetFullscreen(bool fullscreen) {
    if (TheWindow) SDL_SetWindowFullscreen(TheWindow, fullscreen);
}

void ToggleFullscreen() { SetFullscreen(!IsFullscreen()); }

bool IsFullscreen() { return TheWindow && (SDL_GetWindowFlags(TheWindow) & SDL_WINDOW_FULLSCREEN) != 0; }

void SetVSync(bool vsync) {
    VSyncOn = vsync;
    if (Renderer) SDL_SetRenderVSync(Renderer, vsync ? 1 : 0);
}

bool VSync() { return VSyncOn; }

bool Focused() { return HasFocus; }

} // namespace Window

namespace Screen {

Vec2 Size() { return LogicalSize.X > 0 && LogicalSize.Y > 0 ? LogicalSize : Window::Size(); }
float Width() { return Size().X; }
float Height() { return Size().Y; }
Vec2 Center() { return Size() * 0.5f; }

void Clear(Color color) {
    if (!Renderer) return;
    SDL_SetRenderDrawColorFloat(Renderer, color.R, color.G, color.B, color.A);
    SDL_RenderClear(Renderer);
}

Rect Viewport() { return Detail::Letterbox(Window::Size(), Size()); }

Vec2 ToLogical(Vec2 windowPixel) {
    Rect view = Viewport();
    if (view.Width <= 0 || view.Height <= 0) return windowPixel;
    return (windowPixel - view.Position()) * (Size() / view.Size());
}

Vec2 ToWindow(Vec2 logical) {
    Rect view = Viewport();
    Vec2 size = Size();
    if (size.X <= 0 || size.Y <= 0) return logical;
    return view.Position() + logical * (view.Size() / size);
}

} // namespace Screen

} // namespace BitMotor
