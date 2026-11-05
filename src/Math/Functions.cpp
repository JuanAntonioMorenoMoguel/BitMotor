#include "BitMotor/Math/Functions.h"

namespace BitMotor {

Vec2 Approach(Vec2 current, Vec2 target, float step) {
    Vec2 delta = target - current;
    float length = Length(delta);
    if (length <= step || length < 1e-6f) return target;
    return current + delta / length * step;
}

float Wrap(float value, float low, float high) {
    float range = high - low;
    if (range <= 0.0f) return low;
    float wrapped = std::fmod(value - low, range);
    if (wrapped < 0.0f) wrapped += range;
    // fmod of a tiny negative number plus range can round up to exactly `range`.
    if (wrapped >= range) wrapped = 0.0f;
    return low + wrapped;
}

int Wrap(int value, int low, int high) {
    int range = high - low;
    if (range <= 0) return low;
    int wrapped = (value - low) % range;
    if (wrapped < 0) wrapped += range;
    return low + wrapped;
}

Vec2 Normalize(Vec2 v) {
    float length = Length(v);
    return length > 1e-6f ? v / length : Vec2{};
}

Vec3 Normalize(Vec3 v) {
    float length = Length(v);
    return length > 1e-6f ? v / length : Vec3{};
}

Vec2 Rotate(Vec2 v, float angle) {
    float c = std::cos(angle), s = std::sin(angle);
    return {v.X * c - v.Y * s, v.X * s + v.Y * c};
}

float WrapAngle(float angle) { return Wrap(angle, -Pi, Pi); }

float SegmentDistance(Vec2 point, Vec2 a, Vec2 b) {
    Vec2 ab = b - a;
    float t = Clamp(Dot(point - a, ab) / Max(Dot(ab, ab), 1e-12f), 0.0f, 1.0f);
    return Distance(point, a + ab * t);
}

} // namespace BitMotor
