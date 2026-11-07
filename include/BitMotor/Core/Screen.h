#pragma once
#include "BitMotor/Colors/Color.h"
#include "BitMotor/Math/Shapes.h"
#include "BitMotor/Math/Vector.h"

#include <filesystem>

// The drawing surface, in logical pixels (Config::Width x Config::Height). When the window has another
// shape, the image is scaled as much as fits and centred, with black bars on the sides (by whole
// steps, x2, x3..., unless Config::IntegerScaling is false).
//     Screen::Clear(Colors::Navy);
//     Vec2 middle = Screen::Center();
//     Vec2 mouse = Screen::ToLogical(windowPixel);
//     if (Input::KeyPressed(Key::F12)) Screen::Screenshot();
namespace BitMotor::Screen {

float Width();
float Height();
Vec2 Size();
Vec2 Center();

// Fills the screen with a colour, covering everything drawn before it this frame. The loop already
// clears to black before each Draw.
void Clear(Color color);

// Saves this frame as a PNG (or BMP if the name ends in .bmp) when it finishes drawing, at the logical
// resolution. Relative paths go to Files::SavePath; with no path, "Screenshots/<date>_<time>.png".
// `--screenshot file.png` on the command line saves the last frame of a `--frames N` run.
void Screenshot(const std::filesystem::path& path = {});

// Window pixels -> logical pixels, and back. Points on the black bars fall outside 0..Size().
Vec2 ToLogical(Vec2 windowPixel);
Vec2 ToWindow(Vec2 logical);
// Where the logical screen sits inside the window, in window pixels.
Rect Viewport();

// Scaling by whole steps (black bars, every logical pixel the same size) or filling as much as fits.
// Starts as Config::IntegerScaling; an options menu can change it while the game runs.
void SetIntegerScaling(bool integer);
bool IntegerScaling();

} // namespace BitMotor::Screen
