#include "Printing.h"
#include "Testing.h"

using namespace BitMotor;

TEST(RectEdgesAndContains) {
    Rect r{10, 20, 100, 50};
    CHECK_NEAR(r.Right(), 110, 1e-6);
    CHECK_NEAR(r.Bottom(), 70, 1e-6);
    CHECK_EQ(r.Center(), Vec2(60, 45));
    CHECK(r.Contains(Vec2(10, 20)));
    CHECK(!r.Contains(Vec2(110, 45))); // right edge is outside
    CHECK(r.Contains(Rect{20, 30, 10, 10}));
    CHECK_EQ(Rect::FromCenter({50, 50}, {20, 10}), Rect(40, 45, 20, 10));
    CHECK_EQ(Rect::FromCorners({30, 5}, {10, 25}), Rect(10, 5, 20, 20));
}

TEST(RectOverlapsAndIntersection) {
    Rect a{0, 0, 10, 10};
    CHECK(a.Overlaps(Rect{5, 5, 10, 10}));
    CHECK(!a.Overlaps(Rect{10, 0, 10, 10})); // touching only
    CHECK_EQ(a.Intersection(Rect{5, 5, 10, 10}), Rect(5, 5, 5, 5));
    CHECK_EQ(a.Intersection(Rect{20, 20, 5, 5}), Rect());
    CHECK_EQ(a.Expand(2), Rect(-2, -2, 14, 14));
    CHECK_EQ(a.Expand(-10), Rect(5, 5, 0, 0));
    CHECK_EQ(a.Clamp({-5, 15}), Vec2(0, 10));
}

TEST(CircleTests) {
    Circle c{{0, 0}, 5};
    CHECK(c.Contains({3, 4}));
    CHECK(!c.Contains({4, 4}));
    CHECK(c.Overlaps(Circle{{9, 0}, 5}));
    CHECK(!c.Overlaps(Circle{{10, 0}, 5}));
    CHECK(c.Overlaps(Rect{4, -1, 10, 2}));
    CHECK(!c.Overlaps(Rect{4, 4, 10, 10})); // the corner (4, 4) is ~5.66 away
    CHECK_EQ(c.Bounds(), Rect(-5, -5, 10, 10));
}
