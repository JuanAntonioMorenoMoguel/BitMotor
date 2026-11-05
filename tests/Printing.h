#pragma once
// How CHECK_EQ prints BitMotor types when a test fails.
#include <BitMotor/BitMotor.h>

#include <ostream>

namespace BitMotor {
inline std::ostream& operator<<(std::ostream& s, Vec2 v) { return s << "{" << v.X << ", " << v.Y << "}"; }
inline std::ostream& operator<<(std::ostream& s, Vec3 v) {
    return s << "{" << v.X << ", " << v.Y << ", " << v.Z << "}";
}
inline std::ostream& operator<<(std::ostream& s, Vec4 v) {
    return s << "{" << v.X << ", " << v.Y << ", " << v.Z << ", " << v.W << "}";
}
inline std::ostream& operator<<(std::ostream& s, const Rect& r) {
    return s << "Rect{" << r.X << ", " << r.Y << ", " << r.Width << ", " << r.Height << "}";
}
inline std::ostream& operator<<(std::ostream& s, Color c) { return s << c.ToHex(); }
} // namespace BitMotor
