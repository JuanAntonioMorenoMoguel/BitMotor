#pragma once
#include "BitMotor/Colors/Color.h"
#include "BitMotor/Math/Shapes.h"
#include "BitMotor/Math/Vector.h"

// The drawing surface, in logical pixels (Config::Width x Config::Height). When the window has another
// shape, the image is scaled as much as fits and centred, with black bars on the sides.
//     Screen::Clear(Colors::Navy);
//     Vec2 middle = Screen::Center();
//     Vec2 mouse = Screen::ToLogical(windowPixel);
namespace BitMotor::Screen {

float Width();
float Height();
Vec2 Size();
Vec2 Center();

// Fills the screen with a colour. The loop already clears to black before each Draw.
void Clear(Color color);

// Window pixels -> logical pixels, and back. Points on the black bars fall outside 0..Size().
Vec2 ToLogical(Vec2 windowPixel);
Vec2 ToWindow(Vec2 logical);
// Where the logical screen sits inside the window, in window pixels.
Rect Viewport();

} // namespace BitMotor::Screen
