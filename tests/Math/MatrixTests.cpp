#include "Printing.h"
#include "Testing.h"

using namespace BitMotor;

namespace {
void CheckNear(Vec2 a, Vec2 b) {
    CHECK_NEAR(a.X, b.X, 1e-4);
    CHECK_NEAR(a.Y, b.Y, 1e-4);
}
} // namespace

TEST(MatrixTransformsCombineRightToLeft) {
    CheckNear(Mat4::Translate({10, 20, 0}).TransformPoint(Vec2(1, 2)), {11, 22});
    CheckNear(Mat4::Scale({2, 3, 1}).TransformPoint(Vec2(1, 2)), {2, 6});
    CheckNear(Mat4::RotateZ(Pi / 2).TransformPoint(Vec2(1, 0)), {0, 1});
    // Scale first, then rotate, then move.
    Mat4 world = Mat4::Translate({100, 0, 0}) * Mat4::RotateZ(Pi / 2) * Mat4::Scale({2, 2, 1});
    CheckNear(world.TransformPoint(Vec2(1, 0)), {100, 2});
    CHECK(Mat4::Identity() * world == world);
}

TEST(MatrixOrthoMapsPixelsToClipSpace) {
    Mat4 projection = Mat4::Ortho(0, 1280, 720, 0);
    CheckNear(projection.TransformPoint(Vec2(0, 0)), {-1, 1});
    CheckNear(projection.TransformPoint(Vec2(1280, 720)), {1, -1});
    CheckNear(projection.TransformPoint(Vec2(640, 360)), {0, 0});
    CHECK_NEAR(projection.TransformPoint(Vec3(0, 0, -1)).Z, 0.0, 1e-6);
    CHECK_NEAR(projection.TransformPoint(Vec3(0, 0, 1)).Z, 1.0, 1e-6);
}

TEST(MatrixInverseUndoesTheTransform) {
    Mat4 world = Mat4::Translate({30, -12, 5}) * Mat4::RotateZ(0.7f) * Mat4::Scale({3, 0.5f, 2});
    Mat4 back = world * world.Inverse();
    for (int i = 0; i < 16; ++i) CHECK_NEAR(back.M[i], Mat4::Identity().M[i], 1e-5);
    CheckNear(world.Inverse().TransformPoint(world.TransformPoint(Vec2(7, -4))), {7, -4});
    // Not invertible: identity instead of garbage.
    CHECK(Mat4::Scale({0, 1, 1}).Inverse() == Mat4::Identity());
}
