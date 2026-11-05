#include "BitMotor/Math/Random.h"

#include "BitMotor/Math/Functions.h"

#include <cmath>

namespace BitMotor {

void Random::Seed(std::uint64_t seed, std::uint64_t stream) {
    State = 0;
    Increment = (stream << 1u) | 1u;
    Next();
    State += seed;
    Next();
}

std::uint32_t Random::Next() {
    std::uint64_t old = State;
    State = old * 6364136223846793005ULL + Increment;
    auto shifted = std::uint32_t(((old >> 18u) ^ old) >> 27u);
    auto rotation = std::uint32_t(old >> 59u);
    return (shifted >> rotation) | (shifted << ((~rotation + 1u) & 31u));
}

int Random::Int(int low, int high) {
    if (high <= low) return low;
    // Width of the range as unsigned; 0 means the whole 32-bit range.
    std::uint32_t range = std::uint32_t(std::int64_t(high) - std::int64_t(low) + 1);
    if (range == 0) return int(std::int64_t(low) + Next());
    // Reject the few values that would make some results more likely than others.
    std::uint32_t threshold = (0u - range) % range;
    std::uint32_t value = Next();
    while (value < threshold) value = Next();
    return int(std::int64_t(low) + value % range);
}

Vec2 Random::Direction() { return FromAngle(Range(0.0f, Tau)); }

Vec2 Random::InCircle(float radius) { return Direction() * (std::sqrt(Unit()) * radius); }

} // namespace BitMotor
