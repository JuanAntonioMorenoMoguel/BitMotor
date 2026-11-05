#include "Printing.h"
#include "Testing.h"

using namespace BitMotor;

TEST(VectorArithmetic) {
    Vec2 a{1, 2}, b{3, 5};
    CHECK_EQ(a + b, Vec2(4, 7));
    CHECK_EQ(b - a, Vec2(2, 3));
    CHECK_EQ(a * b, Vec2(3, 10));
    CHECK_EQ(a * 2.0f, Vec2(2, 4));
    CHECK_EQ(2.0f * a, Vec2(2, 4));
    CHECK_EQ(b / 2.0f, Vec2(1.5f, 2.5f));
    CHECK_EQ(-a, Vec2(-1, -2));
    a += b;
    a *= 0.5f;
    CHECK_EQ(a, Vec2(2, 3.5f));
    CHECK_EQ(Vec2(3), Vec2(3, 3));
}

TEST(VectorsAreConstexprAndConvertible) {
    constexpr Vec3 v{Vec2{1, 2}, 3};
    static_assert(v.Z == 3 && v.XY() == Vec2{1, 2});
    constexpr Vec4 w{v, 4};
    static_assert(w.XYZ() == v && w.W == 4);
    CHECK_EQ(Vec3(1, 2, 3) * Vec3(2), Vec3(2, 4, 6));
    CHECK_EQ(Vec4(1, 2, 3, 4) + Vec4(1), Vec4(2, 3, 4, 5));
}
