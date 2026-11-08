#include "Printing.h"
#include "Testing.h"

#include <BitMotor/BitMotor.h>

using namespace BitMotor;

namespace {
void CheckVec(Vec3 a, Vec3 b, float epsilon = 1e-4f) {
    CHECK_NEAR(a.X, b.X, epsilon);
    CHECK_NEAR(a.Y, b.Y, epsilon);
    CHECK_NEAR(a.Z, b.Z, epsilon);
}
} // namespace

TEST(QuatRotatesVectors) {
    const Quat left = Quat::FromAxisAngle({0, 1, 0}, Pi / 2); // a quarter turn around Y
    CheckVec(left * Vec3(0, 0, -1), {-1, 0, 0});              // forward turns to the left
    CheckVec(Quat::FromAxisAngle({0, 0, 1}, Pi / 2) * Vec3(1, 0, 0), {0, 1, 0});
    CheckVec((left * left) * Vec3(1, 0, 0), {-1, 0, 0});
    CheckVec(left.Conjugate() * (left * Vec3(1, 2, 3)), {1, 2, 3});
    CheckVec(Quat::Identity() * Vec3(4, 5, 6), {4, 5, 6});
}

TEST(QuatMatchesItsMatrix) {
    const Quat q = Quat::FromEuler(0.3f, 1.1f, -0.4f);
    const Vec3 v{0.5f, -2, 3};
    CheckVec(Mat4::Rotate(q).TransformPoint(v), q * v);
    CheckVec(Mat4::RotateY(0.7f).TransformPoint(v), Quat::FromAxisAngle({0, 1, 0}, 0.7f) * v);
    CheckVec(Mat4::RotateX(0.7f).TransformPoint(v), Quat::FromAxisAngle({1, 0, 0}, 0.7f) * v);
    const Mat4 t = Mat4::Transform({1, 2, 3}, q, {2, 2, 2});
    CheckVec(t.TransformPoint(v), q * (v * 2.0f) + Vec3(1, 2, 3));
    CheckVec(t.TransformDirection({1, 0, 0}), q * Vec3(2, 0, 0));
}

TEST(QuatSlerpGoesTheShortWay) {
    const Quat a = Quat::Identity(), b = Quat::FromAxisAngle({0, 1, 0}, Pi / 2);
    const Quat half = Quat::Slerp(a, b, 0.5f);
    CheckVec(half * Vec3(1, 0, 0), Quat::FromAxisAngle({0, 1, 0}, Pi / 4) * Vec3(1, 0, 0));
    const Quat negated{-b.X, -b.Y, -b.Z, -b.W}; // the same rotation
    CheckVec(Quat::Slerp(a, negated, 0.5f) * Vec3(1, 0, 0), half * Vec3(1, 0, 0));
    CheckVec(Quat::Slerp(a, b, 1) * Vec3(0, 0, 1), b * Vec3(0, 0, 1));
}

TEST(QuatLookRotationFacesTheDirection) {
    for (Vec3 d : {Vec3(1, 0, 0), Vec3(0, 0, 1), Vec3(1, 1, -1), Vec3(0, -1, 0.01f)}) {
        const Quat q = Quat::LookRotation(d);
        CheckVec(q * Vec3(0, 0, -1), Normalize(d));
    }
}

TEST(PerspectiveAndLookAtPutPointsOnScreen) {
    const Mat4 view = Mat4::LookAt({0, 0, 5}, {0, 0, 0});
    CheckVec(view.TransformPoint({0, 0, 0}), {0, 0, -5}); // in front of the camera, down -Z
    const Mat4 projection = Mat4::Perspective(Pi / 2, 1, 1, 10);
    const Mat4 both = projection * view;
    CheckVec(both.TransformPoint({0, 0, 4}), {0, 0, 0});        // on the near plane: depth 0
    CHECK_NEAR(both.TransformPoint({0, 0, -5}).Z, 1.0f, 1e-4f); // on the far plane: depth 1
    CHECK_NEAR(both.TransformPoint({1, 0, 4}).X, 1.0f, 1e-4f);  // the right edge at 90 degrees
    CHECK_NEAR(both.TransformPoint({0, 1, 4}).Y, 1.0f, 1e-4f);  // up is up
}
