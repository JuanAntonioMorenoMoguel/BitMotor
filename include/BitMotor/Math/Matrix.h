#pragma once
#include "BitMotor/Math/Vector.h"

// 4x4 matrix of floats, column-major (the layout GPUs expect): M[column * 4 + row].
// Transforms are combined right to left, as in math: (Translate * Rotate) * point rotates first.
//     Mat4 projection = Mat4::Ortho(0, 1280, 720, 0); // pixels with +Y down
//     Mat4 world = Mat4::Translate({x, y, 0}) * Mat4::RotateZ(angle) * Mat4::Scale({2, 2, 1});
//     Vec2 onScreen = (projection * world).TransformPoint(Vec2{0, 0});
namespace BitMotor {

struct Mat4 {
    float M[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

    static constexpr Mat4 Identity() { return {}; }
    static Mat4 Translate(Vec3 offset);
    static Mat4 Scale(Vec3 factor);
    // Rotation around Z: the only one 2D needs.
    static Mat4 RotateZ(float angle);
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

    // Inverse matrix. A matrix that cannot be inverted returns the identity.
    Mat4 Inverse() const;

    bool operator==(const Mat4&) const = default;
};

} // namespace BitMotor
