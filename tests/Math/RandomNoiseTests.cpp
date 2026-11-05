#include "Printing.h"
#include "Testing.h"

#include <algorithm>
#include <array>
#include <vector>

using namespace BitMotor;

TEST(RandomIsDeterministic) {
    Random a(42), b(42), c(43);
    bool differs = false;
    for (int i = 0; i < 100; ++i) {
        std::uint32_t x = a.Next();
        CHECK_EQ(x, b.Next());
        differs |= x != c.Next();
    }
    CHECK(differs);
    // Known values: the sequence must never change between versions or platforms (replays, daily seeds).
    Random known(12345);
    CHECK_EQ(known.Next(), std::uint32_t(2251339066u));
    CHECK_EQ(known.Next(), std::uint32_t(380381740u));
}

TEST(RandomRanges) {
    Random r(7);
    std::array<int, 4> seen{};
    for (int i = 0; i < 4000; ++i) {
        float u = r.Unit();
        CHECK(u >= 0.0f && u < 1.0f);
        float f = r.Range(-2.0f, 3.0f);
        CHECK(f >= -2.0f && f < 3.0f);
        int n = r.Int(3, 6);
        CHECK(n >= 3 && n <= 6);
        ++seen[std::size_t(n - 3)];
    }
    for (int count : seen) CHECK(count > 800 && count < 1200);
    CHECK_EQ(r.Int(5, 5), 5);
    CHECK_EQ(r.Int(9, 2), 9);
    int full = r.Int(INT32_MIN, INT32_MAX); // whole range must not crash or divide by zero
    (void)full;
    CHECK(!r.Chance(0.0f));
    CHECK(r.Chance(1.0f));
}

TEST(RandomPickShuffleAndDirections) {
    Random r(99);
    std::vector<int> items{1, 2, 3, 4, 5, 6, 7, 8};
    int& picked = r.Pick(items);
    CHECK(picked >= 1 && picked <= 8);
    auto shuffled = items;
    r.Shuffle(shuffled);
    CHECK(shuffled != items);
    std::sort(shuffled.begin(), shuffled.end());
    CHECK(shuffled == items);
    for (int i = 0; i < 100; ++i) {
        CHECK_NEAR(Length(r.Direction()), 1.0, 1e-5);
        CHECK(Length(r.InCircle(3.0f)) <= 3.0f + 1e-5f);
    }
}

TEST(NoiseIsSmoothAndBounded) {
    for (float x = -50.0f; x < 50.0f; x += 0.37f) {
        float n = Noise1D(x, 3);
        CHECK(n >= -1.0f && n <= 1.0f);
        CHECK(Abs(Noise1D(x + 0.001f, 3) - n) < 0.01f);
        CHECK_EQ(Noise1D(x, 3), n);
    }
    CHECK(Noise1D(10.5f, 1) != Noise1D(10.5f, 2));
}
