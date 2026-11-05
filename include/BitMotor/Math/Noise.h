#pragma once
#include <cstdint>

// Smooth 1D value noise in [-1, 1]: nearby x give nearby values. Good for camera shake, flicker and wind.
//     float shakeX = Noise1D(Time::Total() * 20, 1) * trauma;
//     float shakeY = Noise1D(Time::Total() * 20, 2) * trauma; // another seed: an independent curve
namespace BitMotor {

float Noise1D(float x, std::uint32_t seed = 0);

} // namespace BitMotor
