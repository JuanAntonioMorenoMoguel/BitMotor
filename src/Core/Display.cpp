// Window and Screen: both work on the window the loop opened.
#include "BitMotor/Core/Screen.h"
#include "BitMotor/Core/Settings.h"
#include "BitMotor/Core/Window.h"

#include "BitMotor/Core/Data.h"
#include "BitMotor/Core/Log.h"
#include "Core/DisplayInternal.h"
#include "Render/GpuInternal.h"
#include "Render/RendererInternal.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <format>

namespace BitMotor {

namespace {
SDL_Window* TheWindow = nullptr;
Vec2 LogicalSize;
bool VSyncOn = true;
bool IntegerScaling = true;
bool NativeResolution = false;
bool HasFocus = true;
} // namespace

namespace Detail {

void AttachDisplay(SDL_Window* window, int logicalWidth, int logicalHeight, bool vsync, bool integerScaling,
                   bool nativeResolution) {
    TheWindow = window;
    NativeResolution = nativeResolution;
    IntegerScaling = integerScaling;
    LogicalSize = {float(std::max(logicalWidth, 0)), float(std::max(logicalHeight, 0))};
    VSyncOn = vsync;
    HasFocus = true;
}

void SetFocused(bool focused) { HasFocus = focused; }

SDL_Window* CurrentWindow() { return TheWindow; }

Rect Letterbox(Vec2 window, Vec2 logical, bool integer) {
    if (logical.X <= 0 || logical.Y <= 0 || window.X <= 0 || window.Y <= 0) return {{0, 0}, window};
    float scale = std::min(window.X / logical.X, window.Y / logical.Y);
    if (integer && scale >= 1) {
        scale = std::floor(scale);
        Vec2 size = logical * scale;
        Vec2 corner = (window - size) * 0.5f;
        return {{std::floor(corner.X), std::floor(corner.Y)}, size};
    }
    Vec2 size = logical * scale;
    return {(window - size) * 0.5f, size};
}

Rect ViewportFor(Vec2 window) { return Letterbox(window, LogicalSize, IntegerScaling); }

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
    Detail::CurrentSettings().Fullscreen = fullscreen; // Settings::Save keeps the player's choice
}

void ToggleFullscreen() { SetFullscreen(!IsFullscreen()); }

bool IsFullscreen() { return TheWindow && (SDL_GetWindowFlags(TheWindow) & SDL_WINDOW_FULLSCREEN) != 0; }

void SetVSync(bool vsync) {
    VSyncOn = vsync;
    Detail::CurrentSettings().VSync = vsync;
    Detail::SetRendererVSync(vsync);
}

bool VSync() { return VSyncOn; }

bool Focused() { return HasFocus; }

} // namespace Window

namespace Screen {

Vec2 Size() { return LogicalSize.X > 0 && LogicalSize.Y > 0 ? LogicalSize : Window::Size(); }
float Width() { return Size().X; }
float Height() { return Size().Y; }
Vec2 Center() { return Size() * 0.5f; }

void Clear(Color color) { Detail::ClearScreen(color); }

void Screenshot(const std::filesystem::path& path) {
    std::filesystem::path file = path;
    if (file.empty()) {
        SDL_Time now = 0;
        SDL_DateTime t{};
        if (SDL_GetCurrentTime(&now) && SDL_TimeToDateTime(now, &t, true))
            file = std::format("Screenshots/{:04}-{:02}-{:02}_{:02}-{:02}-{:02}.png", t.year, t.month, t.day, t.hour,
                               t.minute, t.second);
        else
            file = "Screenshots/Screenshot.png";
    }
    Detail::RequestScreenshot(Detail::SaveFilePath(file));
}

Rect Viewport() { return Detail::ViewportFor(Window::Size()); }

Vec2 PixelSize() {
    const Vec2 logical = Size();
    if (!NativeResolution || !Detail::ActiveRenderer()) return logical;
    const Rect view = Viewport();
    if (view.Width < 1 || view.Height < 1) return logical; // no window, or minimised
    return {std::round(view.Width), std::round(view.Height)};
}

void SetIntegerScaling(bool integer) { BitMotor::IntegerScaling = integer; }
bool IntegerScaling() { return BitMotor::IntegerScaling; }

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
