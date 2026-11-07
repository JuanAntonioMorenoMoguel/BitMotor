#pragma once
#include "BitMotor/Colors/Color.h"
#include "BitMotor/Render/Sprite.h"

#include <span>
#include <vector>

namespace BitMotor::Detail {

// The loop calls them around the frame's Draw: BeginUi reads this frame's input (clicks, navigation with
// its repeat, which moves the focus), EndUi draws the tooltip and remembers what the widgets were.
void BeginUi();
void EndUi();
// Back to the defaults: theme, navigation controls, focus (tests).
void ResetUi();

// A rectangle of the white pixel: every shape is made of these.
void FillSpan(float left, float right, float y, float height, Color color, Layer layer, float depth);

// One row of a filled polygon: pixels [Left, Right) of row Y.
struct Span {
    float Left, Right, Y;
    bool operator==(const Span&) const = default;
};
std::vector<Span> ScanPolygon(std::span<const Vec2> points);

} // namespace BitMotor::Detail
