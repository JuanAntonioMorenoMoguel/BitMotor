#include "BitMotor/Math/Noise.h"

#include <cmath>

namespace BitMotor {

namespace {
float Hash(int i, std::uint32_t seed) {
    std::uint32_t h = std::uint32_t(i) * 374761393u + seed * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return float(h & 0xFFFFFF) / float(0xFFFFFF) * 2.0f - 1.0f;
}
} // namespace

float Noise1D(float x, std::uint32_t seed) {
    float cell = std::floor(x);
    int i = int(cell);
    float f = x - cell;
    float u = f * f * (3.0f - 2.0f * f);
    return Hash(i, seed) * (1.0f - u) + Hash(i + 1, seed) * u;
}

} // namespace BitMotor
