#pragma once
#include "BitMotor/Colors/Color.h"
#include "BitMotor/Math/Shapes.h"
#include "BitMotor/Math/Vector.h"
#include "BitMotor/Render/Sprite.h"

#include <span>

// Plain shapes in one colour, for interfaces, debug drawing and simple games. They are filled pixel row by
// pixel row, so their edges are as crisp as the sprites around them, and they go into the same frame as the
// sprites (in the Hud by default; any other layer follows the camera and, in the World, the Depth order).
//     UI::Rect({10, 10, 80, 12}, Colors::Navy);
//     UI::RectOutline({10, 10, 80, 12}, Colors::White);
//     UI::Line(a, b, Colors::Red, 2);
//     UI::Circle(enemy.Position, 24, Colors::Red.WithAlpha(0.3f), Layer::World, enemy.Position.Y);
//     UI::Polygon(points, Colors::Lime);
namespace BitMotor::UI {

void Rect(BitMotor::Rect area, Color color, Layer layer = Layer::Hud, float depth = 0);
// A border `thickness` pixels wide just inside the area.
void RectOutline(BitMotor::Rect area, Color color, float thickness = 1, Layer layer = Layer::Hud, float depth = 0);
void Line(Vec2 from, Vec2 to, Color color, float thickness = 1, Layer layer = Layer::Hud, float depth = 0);
void Circle(Vec2 center, float radius, Color color, Layer layer = Layer::Hud, float depth = 0);
// A ring `thickness` pixels wide just inside the radius.
void CircleOutline(Vec2 center, float radius, Color color, float thickness = 1, Layer layer = Layer::Hud,
                   float depth = 0);
void Triangle(Vec2 a, Vec2 b, Vec2 c, Color color, Layer layer = Layer::Hud, float depth = 0);
// Any polygon, convex or not, its points in order (either direction). Crossing edges fill even-odd.
void Polygon(std::span<const Vec2> points, Color color, Layer layer = Layer::Hud, float depth = 0);

} // namespace BitMotor::UI
