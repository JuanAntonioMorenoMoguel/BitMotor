#include "BitMotor/Math/Matrix.h"

#include <cmath>

namespace BitMotor {

Mat4 Mat4::Translate(Vec3 offset) {
    Mat4 m;
    m.M[12] = offset.X;
    m.M[13] = offset.Y;
    m.M[14] = offset.Z;
    return m;
}

Mat4 Mat4::Scale(Vec3 factor) {
    Mat4 m;
    m.M[0] = factor.X;
    m.M[5] = factor.Y;
    m.M[10] = factor.Z;
    return m;
}

Mat4 Mat4::RotateZ(float angle) {
    float c = std::cos(angle), s = std::sin(angle);
    Mat4 m;
    m.M[0] = c;
    m.M[1] = s;
    m.M[4] = -s;
    m.M[5] = c;
    return m;
}

Mat4 Mat4::Ortho(float left, float right, float bottom, float top, float near, float far) {
    Mat4 m;
    m.M[0] = 2.0f / (right - left);
    m.M[5] = 2.0f / (top - bottom);
    m.M[10] = 1.0f / (far - near);
    m.M[12] = -(right + left) / (right - left);
    m.M[13] = -(top + bottom) / (top - bottom);
    m.M[14] = -near / (far - near);
    return m;
}

Mat4 Mat4::operator*(const Mat4& other) const {
    Mat4 result;
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) sum += At(row, k) * other.At(k, column);
            result.At(row, column) = sum;
        }
    return result;
}

Vec4 Mat4::operator*(Vec4 v) const {
    return {M[0] * v.X + M[4] * v.Y + M[8] * v.Z + M[12] * v.W, M[1] * v.X + M[5] * v.Y + M[9] * v.Z + M[13] * v.W,
            M[2] * v.X + M[6] * v.Y + M[10] * v.Z + M[14] * v.W, M[3] * v.X + M[7] * v.Y + M[11] * v.Z + M[15] * v.W};
}

Vec3 Mat4::TransformPoint(Vec3 point) const {
    Vec4 r = *this * Vec4(point, 1.0f);
    return r.W != 0.0f ? r.XYZ() / r.W : r.XYZ();
}

Vec2 Mat4::TransformPoint(Vec2 point) const { return TransformPoint(Vec3(point, 0.0f)).XY(); }

Mat4 Mat4::Inverse() const {
    // Cofactor expansion (the classic MESA gluInvertMatrix layout).
    const float* m = M;
    float inv[16];
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] +
             m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] -
             m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] +
             m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] -
              m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] -
             m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] +
             m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] -
             m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] +
              m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] +
             m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] -
             m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] +
              m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] -
              m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] -
             m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] +
             m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] -
              m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] -
              m[8] * m[2] * m[5];

    float determinant = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (std::abs(determinant) < 1e-12f) return Identity();
    Mat4 result;
    for (int i = 0; i < 16; ++i) result.M[i] = inv[i] / determinant;
    return result;
}

} // namespace BitMotor
