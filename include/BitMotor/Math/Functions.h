#pragma once
#include "BitMotor/Math/Vector.h"

#include <cmath>

// Everyday math for games. Angles are in radians; in screen space +Y points down, so a positive angle
// turns clockwise on screen.
//     float speed = Approach(speed, maxSpeed, acceleration * Time::Delta());
//     camera = Damp(camera, player, 8, Time::Delta()); // smooth follow, same feel at any frame rate
//     Vec2 aim = Normalize(mouse - position);
namespace BitMotor {

inline constexpr float Pi = 3.14159265358979323846f;
inline constexpr float Tau = 6.28318530717958647692f;

template <class T> constexpr T Min(T a, T b) { return b < a ? b : a; }
template <class T> constexpr T Max(T a, T b) { return a < b ? b : a; }
template <class T> constexpr T Clamp(T value, T low, T high) { return value < low ? low : high < value ? high : value; }
template <class T> constexpr T Abs(T value) { return value < T(0) ? -value : value; }
// -1, 0 or 1.
template <class T> constexpr T Sign(T value) { return T((T(0) < value) - (value < T(0))); }

// a when t = 0, b when t = 1 (t is not clamped). Works with floats and vectors.
template <class T> constexpr T Lerp(T a, T b, float t) { return a + (b - a) * t; }
// Where `value` falls between a and b: 0 at a, 1 at b. 0 when a == b.
constexpr float InverseLerp(float a, float b, float value) { return a == b ? 0.0f : (value - a) / (b - a); }
// Maps `value` from [inLow, inHigh] to [outLow, outHigh] (not clamped).
constexpr float Remap(float value, float inLow, float inHigh, float outLow, float outHigh) {
    return Lerp(outLow, outHigh, InverseLerp(inLow, inHigh, value));
}
// Smooth 0..1 curve (Hermite) for t in [0, 1]; clamps t.
constexpr float SmoothStep(float t) {
    t = Clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// Moves `current` towards `target` by at most `step`, without overshooting.
constexpr float Approach(float current, float target, float step) {
    return current < target ? Min(current + step, target) : Max(current - step, target);
}
Vec2 Approach(Vec2 current, Vec2 target, float step);

// Exponential smoothing independent of the frame rate: higher speed reaches the target sooner.
template <class T> T Damp(T current, T target, float speed, float dt) {
    return Lerp(target, current, std::exp(-speed * dt));
}

// Wraps `value` into [low, high): Wrap(370, 0, 360) == 10, Wrap(-1, 0, 5) == 4.
float Wrap(float value, float low, float high);
int Wrap(int value, int low, int high);

// ---------------------------------------------------------------- vectors
constexpr float Dot(Vec2 a, Vec2 b) { return a.X * b.X + a.Y * b.Y; }
constexpr float Dot(Vec3 a, Vec3 b) { return a.X * b.X + a.Y * b.Y + a.Z * b.Z; }
constexpr float Dot(Vec4 a, Vec4 b) { return a.X * b.X + a.Y * b.Y + a.Z * b.Z + a.W * b.W; }
// 2D cross product: the Z of the 3D one. Positive when b is clockwise from a on screen.
constexpr float Cross(Vec2 a, Vec2 b) { return a.X * b.Y - a.Y * b.X; }
constexpr Vec3 Cross(Vec3 a, Vec3 b) { return {a.Y * b.Z - a.Z * b.Y, a.Z * b.X - a.X * b.Z, a.X * b.Y - a.Y * b.X}; }

constexpr float LengthSquared(Vec2 v) { return Dot(v, v); }
constexpr float LengthSquared(Vec3 v) { return Dot(v, v); }
inline float Length(Vec2 v) { return std::sqrt(Dot(v, v)); }
inline float Length(Vec3 v) { return std::sqrt(Dot(v, v)); }
inline float Length(Vec4 v) { return std::sqrt(Dot(v, v)); }
inline float Distance(Vec2 a, Vec2 b) { return Length(b - a); }
inline float Distance(Vec3 a, Vec3 b) { return Length(b - a); }

// Same direction with length 1. A zero (or almost zero) vector returns {0, 0}, never NaN.
Vec2 Normalize(Vec2 v);
Vec3 Normalize(Vec3 v);

// Rotates `v` by `angle` radians.
Vec2 Rotate(Vec2 v, float angle);
// Angle of the vector: 0 points right (+X), Pi/2 points down (+Y).
inline float AngleOf(Vec2 v) { return std::atan2(v.Y, v.X); }
// Vector of the given length pointing at `angle`.
inline Vec2 FromAngle(float angle, float length = 1.0f) { return {std::cos(angle) * length, std::sin(angle) * length}; }
// Wraps an angle into [-Pi, Pi).
float WrapAngle(float angle);
// Shortest signed turn that goes from angle `from` to angle `to`, in [-Pi, Pi).
inline float AngleDifference(float from, float to) { return WrapAngle(to - from); }

// Closest distance from `point` to the segment a-b.
float SegmentDistance(Vec2 point, Vec2 a, Vec2 b);

} // namespace BitMotor
