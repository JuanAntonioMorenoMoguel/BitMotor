#include "BitMotor/UI/Shapes.h"

#include "BitMotor/Math/Functions.h"
#include "Text/FontInternal.h"
#include "UI/UiInternal.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace BitMotor {

namespace Detail {

void FillSpan(float left, float right, float y, float height, Color color, Layer layer, float depth) {
    if (right <= left || height <= 0 || color.A <= 0) return;
    Screen::Draw(Sprite2D{.Region = WhitePixel(),
                          .Position = {left, y},
                          .Scale = {right - left, height},
                          .Origin = Vec2(0, 0),
                          .Color = color,
                          .Layer = layer,
                          .Depth = depth});
}

std::vector<Span> ScanPolygon(std::span<const Vec2> points) {
    std::vector<Span> spans;
    if (points.size() < 3) return spans;
    float top = points[0].Y, bottom = points[0].Y;
    for (Vec2 p : points) {
        top = Min(top, p.Y);
        bottom = Max(bottom, p.Y);
    }
    std::vector<float> crossings;
    // One row per pixel, sampled at the pixel's centre: a pixel is in if its centre is.
    for (float row = std::floor(top); row < bottom; ++row) {
        float y = row + 0.5f;
        crossings.clear();
        for (std::size_t i = 0; i < points.size(); ++i) {
            Vec2 a = points[i], b = points[(i + 1) % points.size()];
            if ((a.Y <= y && b.Y > y) || (b.Y <= y && a.Y > y))
                crossings.push_back(a.X + (y - a.Y) / (b.Y - a.Y) * (b.X - a.X));
        }
        std::sort(crossings.begin(), crossings.end());
        for (std::size_t i = 0; i + 1 < crossings.size(); i += 2) {
            // Pixels whose centre lies between the two crossings.
            float left = std::round(crossings[i]), right = std::round(crossings[i + 1]);
            if (right > left) spans.push_back({left, right, row});
        }
    }
    return spans;
}

} // namespace Detail

namespace UI {

void Rect(BitMotor::Rect area, Color color, Layer layer, float depth) {
    Detail::FillSpan(area.X, area.X + area.Width, area.Y, area.Height, color, layer, depth);
}

void RectOutline(BitMotor::Rect area, Color color, float thickness, Layer layer, float depth) {
    float t = Min(thickness, Min(area.Width, area.Height) * 0.5f);
    if (t <= 0) return;
    Detail::FillSpan(area.X, area.Right(), area.Y, t, color, layer, depth);
    Detail::FillSpan(area.X, area.Right(), area.Bottom() - t, t, color, layer, depth);
    Detail::FillSpan(area.X, area.X + t, area.Y + t, area.Height - 2 * t, color, layer, depth);
    Detail::FillSpan(area.Right() - t, area.Right(), area.Y + t, area.Height - 2 * t, color, layer, depth);
}

void Line(Vec2 from, Vec2 to, Color color, float thickness, Layer layer, float depth) {
    if (thickness <= 0 || color.A <= 0) return;
    Vec2 d = to - from;
    // Straight lines are rectangles; slanted ones a rotated rectangle around the segment.
    const float half = thickness * 0.5f;
    if (d.X == 0 && d.Y == 0) {
        Detail::FillSpan(from.X - half, from.X + half, from.Y - half, thickness, color, layer, depth);
        return;
    }
    if (d.Y == 0) {
        Detail::FillSpan(Min(from.X, to.X), Max(from.X, to.X), from.Y - half, thickness, color, layer, depth);
        return;
    }
    if (d.X == 0) {
        Detail::FillSpan(from.X - half, from.X + half, Min(from.Y, to.Y), Abs(d.Y), color, layer, depth);
        return;
    }
    Screen::Draw(Sprite2D{.Region = Detail::WhitePixel(),
                          .Position = from,
                          .Rotation = AngleOf(d),
                          .Scale = {Length(d), thickness},
                          .Origin = Vec2(0, 0.5f),
                          .Color = color,
                          .Layer = layer,
                          .Depth = depth});
}

void Circle(Vec2 center, float radius, Color color, Layer layer, float depth) {
    if (radius <= 0) return;
    for (float row = std::floor(center.Y - radius); row < center.Y + radius; ++row) {
        float dy = row + 0.5f - center.Y;
        if (Abs(dy) >= radius) continue;
        float half = std::sqrt(radius * radius - dy * dy);
        float left = std::round(center.X - half), right = std::round(center.X + half);
        Detail::FillSpan(left, right, row, 1, color, layer, depth);
    }
}

void CircleOutline(Vec2 center, float radius, Color color, float thickness, Layer layer, float depth) {
    if (radius <= 0 || thickness <= 0) return;
    float inner = Max(radius - thickness, 0.0f);
    for (float row = std::floor(center.Y - radius); row < center.Y + radius; ++row) {
        float dy = row + 0.5f - center.Y;
        if (Abs(dy) >= radius) continue;
        float outer = std::sqrt(radius * radius - dy * dy);
        float left = std::round(center.X - outer), right = std::round(center.X + outer);
        if (Abs(dy) >= inner) {
            Detail::FillSpan(left, right, row, 1, color, layer, depth);
            continue;
        }
        float hole = std::sqrt(inner * inner - dy * dy);
        float holeLeft = std::round(center.X - hole), holeRight = std::round(center.X + hole);
        Detail::FillSpan(left, holeLeft, row, 1, color, layer, depth);
        Detail::FillSpan(holeRight, right, row, 1, color, layer, depth);
    }
}

void Triangle(Vec2 a, Vec2 b, Vec2 c, Color color, Layer layer, float depth) {
    const Vec2 points[3] = {a, b, c};
    Polygon(points, color, layer, depth);
}

void Polygon(std::span<const Vec2> points, Color color, Layer layer, float depth) {
    if (color.A <= 0) return;
    for (const Detail::Span& span : Detail::ScanPolygon(points))
        Detail::FillSpan(span.Left, span.Right, span.Y, 1, color, layer, depth);
}

} // namespace UI

} // namespace BitMotor
