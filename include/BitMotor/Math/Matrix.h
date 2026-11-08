#pragma once
#include "BitMotor/Math/Vector.h"

// 4x4 matrix of floats, column-major (the layout GPUs expect): M[column * 4 + row].
// Transforms are combined right to left, as in math: (Translate * Rotate) * point rotates first.
//     Mat4 projection = Mat4::Ortho(0, 1280, 720, 0); // pixels with +Y down
//     Mat4 world = Mat4::Translate({x, y, 0}) * Mat4::RotateZ(angle) * Mat4::Scale({2, 2, 1});
//     Vec2 onScreen = (projection * world).TransformPoint(Vec2{0, 0});
//
// 3D: right-handed, +Y up, the camera looks down -Z. Rotations are quaternions (Quat).
//     Quat turn = Quat::FromAxisAngle({0, 1, 0}, Pi / 2);              // a quarter turn left around Y
//     Mat4 world = Mat4::Transform(position, turn, {1, 1, 1});
//     Mat4 camera = Mat4::Perspective(ToRadians(60), 16.0f / 9.0f, 0.1f, 100) * Mat4::LookAt(eye, target);
namespace BitMotor {

// A rotation in 3D: a unit quaternion (X, Y, Z, W). Combine with *: (a * b) rotates by b, then by a.
struct Quat {
    float X = 0, Y = 0, Z = 0, W = 1;

    static constexpr Quat Identity() { return {}; }
    // `angle` radians around `axis` (any length but zero), counter-clockwise looking down the axis.
    static Quat FromAxisAngle(Vec3 axis, float angle);
    // Radians around X (pitch), then Y (yaw), then Z (roll), as Euler(pitch, yaw, roll) = Y * X * Z.
    static Quat FromEuler(float pitch, float yaw, float roll = 0);
    // The rotation that turns -Z (forward) towards `direction`, keeping `up` as close to up as it can.
    static Quat LookRotation(Vec3 direction, Vec3 up = {0, 1, 0});
    // The shortest way from `a` to `b`, at `t` (0..1).
    static Quat Slerp(Quat a, Quat b, float t);

    Quat operator*(Quat other) const;
    // `v` rotated.
    Vec3 operator*(Vec3 v) const;
    Quat Conjugate() const { return {-X, -Y, -Z, W}; } // the inverse of a unit quaternion
    Quat Normalized() const;

    bool operator==(const Quat&) const = default;
};

struct Mat4 {
    float M[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

    static constexpr Mat4 Identity() { return {}; }
    static Mat4 Translate(Vec3 offset);
    static Mat4 Scale(Vec3 factor);
    // Rotation around Z: the only one 2D needs.
    static Mat4 RotateZ(float angle);
    static Mat4 RotateX(float angle);
    static Mat4 RotateY(float angle);
    static Mat4 Rotate(Quat rotation);
    // Scale, then rotate, then move: where an object is.
    static Mat4 Transform(Vec3 position, Quat rotation, Vec3 scale = {1, 1, 1});
    // Perspective projection to clip space with depth in [0, 1], `fovY` radians from top to bottom.
    static Mat4 Perspective(float fovY, float aspect, float near, float far);
    // The view matrix of a camera at `eye` looking at `target`.
    static Mat4 LookAt(Vec3 eye, Vec3 target, Vec3 up = {0, 1, 0});
    // Orthographic projection to clip space with depth in [0, 1] (Vulkan / SDL_GPU convention).
    // Ortho(0, width, height, 0) gives pixel coordinates with +Y down.
    static Mat4 Ortho(float left, float right, float bottom, float top, float near = -1.0f, float far = 1.0f);

    float& At(int row, int column) { return M[column * 4 + row]; }
    float At(int row, int column) const { return M[column * 4 + row]; }

    Mat4 operator*(const Mat4& other) const;
    Vec4 operator*(Vec4 v) const;
    // Transforms a point (w = 1) and divides by w.
    Vec3 TransformPoint(Vec3 point) const;
    Vec2 TransformPoint(Vec2 point) const;
    // Transforms a direction (w = 0): rotation and scale, no movement.
    Vec3 TransformDirection(Vec3 direction) const;

    // Inverse matrix. A matrix that cannot be inverted returns the identity.
    Mat4 Inverse() const;

    bool operator==(const Mat4&) const = default;
};

} // namespace BitMotor
