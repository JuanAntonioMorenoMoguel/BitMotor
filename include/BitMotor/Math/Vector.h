#pragma once

// 2, 3 and 4 component vectors of floats. Plain data with the usual operators.
//     Vec2 velocity{3, 4};
//     position += velocity * Time::Delta();
//     Vec2 tint = Vec2{1, 2} * Vec2{3, 4}; // component by component: {3, 8}
namespace BitMotor {

struct Vec2 {
    float X = 0, Y = 0;

    constexpr Vec2() = default;
    constexpr Vec2(float x, float y) : X(x), Y(y) {}
    // Same value in both components: Vec2(1) == Vec2{1, 1}.
    constexpr explicit Vec2(float both) : X(both), Y(both) {}

    constexpr Vec2 operator-() const { return {-X, -Y}; }
    constexpr Vec2 operator+(Vec2 o) const { return {X + o.X, Y + o.Y}; }
    constexpr Vec2 operator-(Vec2 o) const { return {X - o.X, Y - o.Y}; }
    constexpr Vec2 operator*(Vec2 o) const { return {X * o.X, Y * o.Y}; }
    constexpr Vec2 operator/(Vec2 o) const { return {X / o.X, Y / o.Y}; }
    constexpr Vec2 operator*(float s) const { return {X * s, Y * s}; }
    constexpr Vec2 operator/(float s) const { return {X / s, Y / s}; }
    constexpr Vec2& operator+=(Vec2 o) { return *this = *this + o; }
    constexpr Vec2& operator-=(Vec2 o) { return *this = *this - o; }
    constexpr Vec2& operator*=(Vec2 o) { return *this = *this * o; }
    constexpr Vec2& operator/=(Vec2 o) { return *this = *this / o; }
    constexpr Vec2& operator*=(float s) { return *this = *this * s; }
    constexpr Vec2& operator/=(float s) { return *this = *this / s; }
    constexpr bool operator==(const Vec2&) const = default;
};

struct Vec3 {
    float X = 0, Y = 0, Z = 0;

    constexpr Vec3() = default;
    constexpr Vec3(float x, float y, float z) : X(x), Y(y), Z(z) {}
    constexpr Vec3(Vec2 xy, float z) : X(xy.X), Y(xy.Y), Z(z) {}
    constexpr explicit Vec3(float all) : X(all), Y(all), Z(all) {}

    constexpr Vec2 XY() const { return {X, Y}; }

    constexpr Vec3 operator-() const { return {-X, -Y, -Z}; }
    constexpr Vec3 operator+(Vec3 o) const { return {X + o.X, Y + o.Y, Z + o.Z}; }
    constexpr Vec3 operator-(Vec3 o) const { return {X - o.X, Y - o.Y, Z - o.Z}; }
    constexpr Vec3 operator*(Vec3 o) const { return {X * o.X, Y * o.Y, Z * o.Z}; }
    constexpr Vec3 operator/(Vec3 o) const { return {X / o.X, Y / o.Y, Z / o.Z}; }
    constexpr Vec3 operator*(float s) const { return {X * s, Y * s, Z * s}; }
    constexpr Vec3 operator/(float s) const { return {X / s, Y / s, Z / s}; }
    constexpr Vec3& operator+=(Vec3 o) { return *this = *this + o; }
    constexpr Vec3& operator-=(Vec3 o) { return *this = *this - o; }
    constexpr Vec3& operator*=(Vec3 o) { return *this = *this * o; }
    constexpr Vec3& operator/=(Vec3 o) { return *this = *this / o; }
    constexpr Vec3& operator*=(float s) { return *this = *this * s; }
    constexpr Vec3& operator/=(float s) { return *this = *this / s; }
    constexpr bool operator==(const Vec3&) const = default;
};

struct Vec4 {
    float X = 0, Y = 0, Z = 0, W = 0;

    constexpr Vec4() = default;
    constexpr Vec4(float x, float y, float z, float w) : X(x), Y(y), Z(z), W(w) {}
    constexpr Vec4(Vec3 xyz, float w) : X(xyz.X), Y(xyz.Y), Z(xyz.Z), W(w) {}
    constexpr Vec4(Vec2 xy, float z, float w) : X(xy.X), Y(xy.Y), Z(z), W(w) {}
    constexpr explicit Vec4(float all) : X(all), Y(all), Z(all), W(all) {}

    constexpr Vec2 XY() const { return {X, Y}; }
    constexpr Vec3 XYZ() const { return {X, Y, Z}; }

    constexpr Vec4 operator-() const { return {-X, -Y, -Z, -W}; }
    constexpr Vec4 operator+(Vec4 o) const { return {X + o.X, Y + o.Y, Z + o.Z, W + o.W}; }
    constexpr Vec4 operator-(Vec4 o) const { return {X - o.X, Y - o.Y, Z - o.Z, W - o.W}; }
    constexpr Vec4 operator*(Vec4 o) const { return {X * o.X, Y * o.Y, Z * o.Z, W * o.W}; }
    constexpr Vec4 operator/(Vec4 o) const { return {X / o.X, Y / o.Y, Z / o.Z, W / o.W}; }
    constexpr Vec4 operator*(float s) const { return {X * s, Y * s, Z * s, W * s}; }
    constexpr Vec4 operator/(float s) const { return {X / s, Y / s, Z / s, W / s}; }
    constexpr Vec4& operator+=(Vec4 o) { return *this = *this + o; }
    constexpr Vec4& operator-=(Vec4 o) { return *this = *this - o; }
    constexpr Vec4& operator*=(Vec4 o) { return *this = *this * o; }
    constexpr Vec4& operator/=(Vec4 o) { return *this = *this / o; }
    constexpr Vec4& operator*=(float s) { return *this = *this * s; }
    constexpr Vec4& operator/=(float s) { return *this = *this / s; }
    constexpr bool operator==(const Vec4&) const = default;
};

constexpr Vec2 operator*(float s, Vec2 v) { return v * s; }
constexpr Vec3 operator*(float s, Vec3 v) { return v * s; }
constexpr Vec4 operator*(float s, Vec4 v) { return v * s; }

} // namespace BitMotor
