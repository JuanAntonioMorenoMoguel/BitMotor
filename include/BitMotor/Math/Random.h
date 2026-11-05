#pragma once
#include "BitMotor/Math/Vector.h"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <utility>

// Random numbers with PCG32: the same seed gives the same sequence on every platform and compiler,
// which is what daily seeds, replays and lockstep need.
//     Random random(dailySeed);
//     int damage = random.Int(3, 6);            // 3, 4, 5 or 6
//     if (random.Chance(0.1f)) DropLoot();
//     Enemy& target = random.Pick(enemies);
namespace BitMotor {

class Random {
public:
    explicit Random(std::uint64_t seed = 0x853c49e6748fea9bULL) { Seed(seed); }

    // Restarts the sequence. Different `stream` values give independent sequences for the same seed.
    void Seed(std::uint64_t seed, std::uint64_t stream = 0xda3e39cb94b95bdbULL);

    // Next raw 32-bit number.
    std::uint32_t Next();
    // Uniform in [0, 1).
    float Unit() { return float(Next() >> 8) * (1.0f / 16777216.0f); }
    // Uniform in [low, high).
    float Range(float low, float high) { return low + (high - low) * Unit(); }
    // Uniform integer in [low, high], both included, without modulo bias.
    int Int(int low, int high);
    // true with probability `probability` (0..1).
    bool Chance(float probability = 0.5f) { return Unit() < probability; }
    // Random direction of length 1.
    Vec2 Direction();
    // Uniform point inside a circle of the given radius centred on 0.
    Vec2 InCircle(float radius = 1.0f);

    // A random element of a vector, array or span. It must not be empty.
    template <class C> auto& Pick(C&& items) { return items[Index(std::size(items))]; }
    // Shuffles in place (Fisher-Yates).
    template <class C> void Shuffle(C& items) {
        for (std::size_t i = std::size(items); i > 1; --i) {
            using std::swap;
            swap(items[i - 1], items[Index(i)]);
        }
    }

private:
    std::size_t Index(std::size_t count) { return std::size_t(Int(0, int(count) - 1)); }

    std::uint64_t State = 0, Increment = 0;
};

} // namespace BitMotor
