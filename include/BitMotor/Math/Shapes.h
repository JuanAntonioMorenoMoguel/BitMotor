#pragma once
#include "BitMotor/Math/Vector.h"

// Rectangles and circles for hit tests, layouts and culling. Screen convention: +Y points down, so
// Top() is the smallest Y.
//     Rect button{20, 20, 120, 32};
//     if (button.Contains(Input::Mouse())) ...
//     if (Circle{bullet, 4}.Overlaps(enemy.Bounds())) ...
namespace BitMotor {

struct Rect {
    float X = 0, Y = 0, Width = 0, Height = 0;

    constexpr Rect() = default;
    constexpr Rect(float x, float y, float width, float height) : X(x), Y(y), Width(width), Height(height) {}
    constexpr Rect(Vec2 position, Vec2 size) : X(position.X), Y(position.Y), Width(size.X), Height(size.Y) {}
    static constexpr Rect FromCenter(Vec2 center, Vec2 size) { return {center - size * 0.5f, size}; }
    // Smallest rectangle that contains both points, in any order.
    static Rect FromCorners(Vec2 a, Vec2 b);

    constexpr float Left() const { return X; }
    constexpr float Right() const { return X + Width; }
    constexpr float Top() const { return Y; }
    constexpr float Bottom() const { return Y + Height; }
    constexpr Vec2 Position() const { return {X, Y}; }
    constexpr Vec2 Size() const { return {Width, Height}; }
    constexpr Vec2 Center() const { return {X + Width * 0.5f, Y + Height * 0.5f}; }

    // The left and top edges are inside, the right and bottom ones are not: tiles never share a point.
    constexpr bool Contains(Vec2 p) const { return p.X >= X && p.X < X + Width && p.Y >= Y && p.Y < Y + Height; }
    constexpr bool Contains(const Rect& r) const {
        return r.X >= X && r.Y >= Y && r.Right() <= Right() && r.Bottom() <= Bottom();
    }
    // Touching edges do not count as overlapping.
    constexpr bool Overlaps(const Rect& r) const {
        return X < r.Right() && r.X < Right() && Y < r.Bottom() && r.Y < Bottom();
    }
    // The shared area; an empty Rect (0 size) if they do not overlap.
    Rect Intersection(const Rect& r) const;
    // Grows `amount` on every side (negative shrinks, never below 0 size).
    Rect Expand(float amount) const;
    Rect Expand(Vec2 amount) const;
    // Closest point inside the rectangle (the point itself if it is inside).
    Vec2 Clamp(Vec2 p) const;

    constexpr bool operator==(const Rect&) const = default;
};

struct Circle {
    Vec2 Center;
    float Radius = 0;

    bool Contains(Vec2 p) const;
    bool Overlaps(const Circle& other) const;
    bool Overlaps(const Rect& rect) const;
    Rect Bounds() const { return Rect::FromCenter(Center, Vec2(Radius * 2)); }

    bool operator==(const Circle&) const = default;
};

} // namespace BitMotor
