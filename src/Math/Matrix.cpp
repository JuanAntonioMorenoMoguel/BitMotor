#include "BitMotor/Math/Matrix.h"

#include "BitMotor/Math/Functions.h"

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

Mat4 Mat4::RotateX(float angle) {
    float c = std::cos(angle), s = std::sin(angle);
    Mat4 m;
    m.M[5] = c;
    m.M[6] = s;
    m.M[9] = -s;
    m.M[10] = c;
    return m;
}

Mat4 Mat4::RotateY(float angle) {
    float c = std::cos(angle), s = std::sin(angle);
    Mat4 m;
    m.M[0] = c;
    m.M[2] = -s;
    m.M[8] = s;
    m.M[10] = c;
    return m;
}

Mat4 Mat4::Rotate(Quat q) {
    q = q.Normalized();
    const float x = q.X, y = q.Y, z = q.Z, w = q.W;
    Mat4 m;
    m.M[0] = 1 - 2 * (y * y + z * z);
    m.M[1] = 2 * (x * y + z * w);
    m.M[2] = 2 * (x * z - y * w);
    m.M[4] = 2 * (x * y - z * w);
    m.M[5] = 1 - 2 * (x * x + z * z);
    m.M[6] = 2 * (y * z + x * w);
    m.M[8] = 2 * (x * z + y * w);
    m.M[9] = 2 * (y * z - x * w);
    m.M[10] = 1 - 2 * (x * x + y * y);
    return m;
}

Mat4 Mat4::Transform(Vec3 position, Quat rotation, Vec3 scale) {
    Mat4 m = Rotate(rotation);
    for (int row = 0; row < 3; ++row) {
        m.At(row, 0) *= scale.X;
        m.At(row, 1) *= scale.Y;
        m.At(row, 2) *= scale.Z;
    }
    m.M[12] = position.X;
    m.M[13] = position.Y;
    m.M[14] = position.Z;
    return m;
}

Mat4 Mat4::Perspective(float fovY, float aspect, float near, float far) {
    const float f = 1.0f / std::tan(fovY * 0.5f);
    Mat4 m;
    m.M[0] = f / aspect;
    m.M[5] = f;
    m.M[10] = far / (near - far);
    m.M[11] = -1;
    m.M[14] = near * far / (near - far);
    m.M[15] = 0;
    return m;
}

Mat4 Mat4::LookAt(Vec3 eye, Vec3 target, Vec3 up) {
    Vec3 forward = Normalize(target - eye);
    Vec3 right = Normalize(Cross(forward, up));
    if (Length(right) < 1e-6f) right = Normalize(Cross(forward, Vec3(0, 0, 1))); // looking straight up or down
    const Vec3 trueUp = Cross(right, forward);
    Mat4 m;
    m.At(0, 0) = right.X, m.At(0, 1) = right.Y, m.At(0, 2) = right.Z;
    m.At(1, 0) = trueUp.X, m.At(1, 1) = trueUp.Y, m.At(1, 2) = trueUp.Z;
    m.At(2, 0) = -forward.X, m.At(2, 1) = -forward.Y, m.At(2, 2) = -forward.Z;
    m.At(0, 3) = -Dot(right, eye);
    m.At(1, 3) = -Dot(trueUp, eye);
    m.At(2, 3) = Dot(forward, eye);
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

Vec3 Mat4::TransformDirection(Vec3 direction) const { return (*this * Vec4(direction, 0.0f)).XYZ(); }

// ---------------------------------------------------------------- Quat

Quat Quat::FromAxisAngle(Vec3 axis, float angle) {
    const Vec3 a = Normalize(axis);
    const float s = std::sin(angle * 0.5f);
    return {a.X * s, a.Y * s, a.Z * s, std::cos(angle * 0.5f)};
}

Quat Quat::FromEuler(float pitch, float yaw, float roll) {
    return FromAxisAngle({0, 1, 0}, yaw) * FromAxisAngle({1, 0, 0}, pitch) * FromAxisAngle({0, 0, 1}, roll);
}

Quat Quat::LookRotation(Vec3 direction, Vec3 up) {
    // The camera-style basis (right, up, back) as a rotation matrix, then to a quaternion.
    const Vec3 back = Normalize(-direction);
    if (Length(back) < 1e-6f) return {};
    Vec3 right = Normalize(Cross(up, back));
    if (Length(right) < 1e-6f) right = Normalize(Cross(Vec3(0, 0, 1), back));
    const Vec3 trueUp = Cross(back, right);
    const float m00 = right.X, m11 = trueUp.Y, m22 = back.Z;
    const float trace = m00 + m11 + m22;
    Quat q;
    if (trace > 0) {
        float s = std::sqrt(trace + 1.0f) * 2;
        q = {(trueUp.Z - back.Y) / s, (back.X - right.Z) / s, (right.Y - trueUp.X) / s, 0.25f * s};
    } else if (m00 > m11 && m00 > m22) {
        float s = std::sqrt(1.0f + m00 - m11 - m22) * 2;
        q = {0.25f * s, (trueUp.X + right.Y) / s, (back.X + right.Z) / s, (trueUp.Z - back.Y) / s};
    } else if (m11 > m22) {
        float s = std::sqrt(1.0f + m11 - m00 - m22) * 2;
        q = {(trueUp.X + right.Y) / s, 0.25f * s, (back.Y + trueUp.Z) / s, (back.X - right.Z) / s};
    } else {
        float s = std::sqrt(1.0f + m22 - m00 - m11) * 2;
        q = {(back.X + right.Z) / s, (back.Y + trueUp.Z) / s, 0.25f * s, (right.Y - trueUp.X) / s};
    }
    return q.Normalized();
}

Quat Quat::Slerp(Quat a, Quat b, float t) {
    float cosine = a.X * b.X + a.Y * b.Y + a.Z * b.Z + a.W * b.W;
    if (cosine < 0) { // the short way round
        b = {-b.X, -b.Y, -b.Z, -b.W};
        cosine = -cosine;
    }
    float wa = 1 - t, wb = t;
    if (cosine < 0.9995f) {
        const float angle = std::acos(cosine), sine = std::sin(angle);
        wa = std::sin((1 - t) * angle) / sine;
        wb = std::sin(t * angle) / sine;
    }
    return Quat{a.X * wa + b.X * wb, a.Y * wa + b.Y * wb, a.Z * wa + b.Z * wb, a.W * wa + b.W * wb}.Normalized();
}

Quat Quat::operator*(Quat o) const {
    return {W * o.X + X * o.W + Y * o.Z - Z * o.Y, W * o.Y - X * o.Z + Y * o.W + Z * o.X,
            W * o.Z + X * o.Y - Y * o.X + Z * o.W, W * o.W - X * o.X - Y * o.Y - Z * o.Z};
}

Vec3 Quat::operator*(Vec3 v) const {
    const Vec3 u{X, Y, Z};
    const Vec3 t = Cross(u, v) * 2.0f;
    return v + t * W + Cross(u, t);
}

Quat Quat::Normalized() const {
    const float length = std::sqrt(X * X + Y * Y + Z * Z + W * W);
    if (length < 1e-9f) return {};
    return {X / length, Y / length, Z / length, W / length};
}

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
