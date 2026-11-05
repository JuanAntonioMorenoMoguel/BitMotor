#include "BitMotor/Math/Shapes.h"

#include "BitMotor/Math/Functions.h"

namespace BitMotor {

Rect Rect::FromCorners(Vec2 a, Vec2 b) {
    Vec2 low{Min(a.X, b.X), Min(a.Y, b.Y)};
    Vec2 high{Max(a.X, b.X), Max(a.Y, b.Y)};
    return {low, high - low};
}

Rect Rect::Intersection(const Rect& r) const {
    if (!Overlaps(r)) return {};
    return FromCorners({Max(X, r.X), Max(Y, r.Y)}, {Min(Right(), r.Right()), Min(Bottom(), r.Bottom())});
}

Rect Rect::Expand(float amount) const { return Expand(Vec2(amount)); }

Rect Rect::Expand(Vec2 amount) const {
    Vec2 size{Max(Width + amount.X * 2, 0.0f), Max(Height + amount.Y * 2, 0.0f)};
    return FromCenter(Center(), size);
}

Vec2 Rect::Clamp(Vec2 p) const { return {BitMotor::Clamp(p.X, X, Right()), BitMotor::Clamp(p.Y, Y, Bottom())}; }

bool Circle::Contains(Vec2 p) const { return LengthSquared(p - Center) <= Radius * Radius; }

bool Circle::Overlaps(const Circle& other) const {
    float radii = Radius + other.Radius;
    return LengthSquared(other.Center - Center) < radii * radii;
}

bool Circle::Overlaps(const Rect& rect) const {
    if (rect.Width <= 0 || rect.Height <= 0) return false;
    return LengthSquared(rect.Clamp(Center) - Center) < Radius * Radius;
}

} // namespace BitMotor
