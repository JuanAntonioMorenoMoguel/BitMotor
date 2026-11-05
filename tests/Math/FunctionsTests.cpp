#include "Printing.h"
#include "Testing.h"

using namespace BitMotor;

TEST(FunctionsBasics) {
    static_assert(Min(3, 5) == 3 && Max(3, 5) == 5);
    static_assert(Clamp(7, 0, 5) == 5 && Clamp(-1, 0, 5) == 0 && Clamp(3, 0, 5) == 3);
    static_assert(Abs(-2.5f) == 2.5f && Sign(-4) == -1 && Sign(0.0f) == 0.0f && Sign(9) == 1);
    CHECK_NEAR(Lerp(10.0f, 20.0f, 0.25f), 12.5f, 1e-6);
    CHECK_EQ(Lerp(Vec2(0, 0), Vec2(10, 20), 0.5f), Vec2(5, 10));
    CHECK_NEAR(InverseLerp(10, 20, 15), 0.5, 1e-6);
    CHECK_NEAR(InverseLerp(3, 3, 3), 0.0, 1e-6);
    CHECK_NEAR(Remap(5, 0, 10, 100, 200), 150.0, 1e-4);
    CHECK_NEAR(SmoothStep(-1), 0.0, 1e-6);
    CHECK_NEAR(SmoothStep(0.5f), 0.5, 1e-6);
    CHECK_NEAR(SmoothStep(2), 1.0, 1e-6);
}

TEST(FunctionsApproachNeverOvershoots) {
    CHECK_NEAR(Approach(0.0f, 10.0f, 3.0f), 3.0, 1e-6);
    CHECK_NEAR(Approach(9.0f, 10.0f, 3.0f), 10.0, 1e-6);
    CHECK_NEAR(Approach(10.0f, 0.0f, 4.0f), 6.0, 1e-6);
    CHECK_EQ(Approach(Vec2(0, 0), Vec2(3, 4), 10.0f), Vec2(3, 4));
    Vec2 step = Approach(Vec2(0, 0), Vec2(3, 4), 1.0f);
    CHECK_NEAR(step.X, 0.6, 1e-5);
    CHECK_NEAR(step.Y, 0.8, 1e-5);
}

TEST(FunctionsDampIsFrameRateIndependent) {
    // Two half steps land in the same place as one full step.
    float once = Damp(0.0f, 100.0f, 5.0f, 0.2f);
    float twice = Damp(Damp(0.0f, 100.0f, 5.0f, 0.1f), 100.0f, 5.0f, 0.1f);
    CHECK_NEAR(once, twice, 1e-3);
    CHECK(once > 0 && once < 100);
    CHECK_NEAR(Damp(0.0f, 100.0f, 5.0f, 0.0f), 0.0, 1e-6);
}

TEST(FunctionsWrap) {
    CHECK_NEAR(Wrap(370.0f, 0.0f, 360.0f), 10.0, 1e-4);
    CHECK_NEAR(Wrap(-10.0f, 0.0f, 360.0f), 350.0, 1e-4);
    CHECK_NEAR(Wrap(360.0f, 0.0f, 360.0f), 0.0, 1e-4);
    CHECK_EQ(Wrap(-1, 0, 5), 4);
    CHECK_EQ(Wrap(5, 0, 5), 0);
    CHECK_EQ(Wrap(12, 10, 13), 12);
    CHECK_EQ(Wrap(-7, 0, 3), 2);
}

TEST(FunctionsVectors) {
    CHECK_NEAR(Dot(Vec2(1, 2), Vec2(3, 4)), 11.0, 1e-6);
    CHECK_NEAR(Cross(Vec2(1, 0), Vec2(0, 1)), 1.0, 1e-6);
    CHECK_EQ(Cross(Vec3(1, 0, 0), Vec3(0, 1, 0)), Vec3(0, 0, 1));
    CHECK_NEAR(Length(Vec2(3, 4)), 5.0, 1e-6);
    CHECK_NEAR(Distance(Vec2(1, 1), Vec2(4, 5)), 5.0, 1e-6);
    CHECK_NEAR(Length(Normalize(Vec2(10, -3))), 1.0, 1e-6);
    CHECK_EQ(Normalize(Vec2(0, 0)), Vec2(0, 0));
    CHECK_EQ(Normalize(Vec3(0, 0, 0)), Vec3(0, 0, 0));
    CHECK_NEAR(SegmentDistance(Vec2(5, 3), Vec2(0, 0), Vec2(10, 0)), 3.0, 1e-5);
    CHECK_NEAR(SegmentDistance(Vec2(-3, 4), Vec2(0, 0), Vec2(10, 0)), 5.0, 1e-5);
}

TEST(FunctionsAngles) {
    Vec2 down = Rotate(Vec2(1, 0), Pi / 2);
    CHECK_NEAR(down.X, 0.0, 1e-6);
    CHECK_NEAR(down.Y, 1.0, 1e-6);
    CHECK_NEAR(AngleOf(Vec2(0, 1)), Pi / 2, 1e-6);
    Vec2 v = FromAngle(Pi, 2.0f);
    CHECK_NEAR(v.X, -2.0, 1e-6);
    CHECK_NEAR(v.Y, 0.0, 1e-5);
    CHECK_NEAR(WrapAngle(3 * Pi / 2), -Pi / 2, 1e-5);
    CHECK_NEAR(WrapAngle(-3 * Pi / 2), Pi / 2, 1e-5);
    CHECK_NEAR(AngleDifference(Pi - 0.1f, -Pi + 0.1f), 0.2, 1e-5);
}
