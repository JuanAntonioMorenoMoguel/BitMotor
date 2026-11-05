#include "BitMotor/Math/Ease.h"

#include "BitMotor/Math/Functions.h"

#include <cmath>

// Formulas from easings.net, with t clamped to [0, 1] so every curve starts at 0 and ends at 1 exactly.
namespace BitMotor::Ease {

namespace {
constexpr float Back = 1.70158f;
constexpr float BackInOut = Back * 1.525f;
float Clamped(float t) { return Clamp(t, 0.0f, 1.0f); }
} // namespace

float Linear(float t) { return Clamped(t); }

float InSine(float t) { return 1.0f - std::cos(Clamped(t) * Pi / 2.0f); }
float OutSine(float t) { return std::sin(Clamped(t) * Pi / 2.0f); }
float InOutSine(float t) { return -(std::cos(Pi * Clamped(t)) - 1.0f) / 2.0f; }

float InQuad(float t) {
    t = Clamped(t);
    return t * t;
}
float OutQuad(float t) {
    t = 1.0f - Clamped(t);
    return 1.0f - t * t;
}
float InOutQuad(float t) {
    t = Clamped(t);
    return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
}

float InCubic(float t) {
    t = Clamped(t);
    return t * t * t;
}
float OutCubic(float t) {
    t = 1.0f - Clamped(t);
    return 1.0f - t * t * t;
}
float InOutCubic(float t) {
    t = Clamped(t);
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

float InExpo(float t) {
    t = Clamped(t);
    return t <= 0.0f ? 0.0f : std::pow(2.0f, 10.0f * t - 10.0f);
}
float OutExpo(float t) {
    t = Clamped(t);
    return t >= 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);
}
float InOutExpo(float t) {
    t = Clamped(t);
    if (t <= 0.0f || t >= 1.0f) return t;
    return t < 0.5f ? std::pow(2.0f, 20.0f * t - 10.0f) / 2.0f : (2.0f - std::pow(2.0f, -20.0f * t + 10.0f)) / 2.0f;
}

float InBack(float t) {
    t = Clamped(t);
    return (Back + 1.0f) * t * t * t - Back * t * t;
}
float OutBack(float t) {
    t = Clamped(t) - 1.0f;
    return 1.0f + (Back + 1.0f) * t * t * t + Back * t * t;
}
float InOutBack(float t) {
    t = Clamped(t);
    if (t < 0.5f) return (std::pow(2.0f * t, 2.0f) * ((BackInOut + 1.0f) * 2.0f * t - BackInOut)) / 2.0f;
    return (std::pow(2.0f * t - 2.0f, 2.0f) * ((BackInOut + 1.0f) * (t * 2.0f - 2.0f) + BackInOut) + 2.0f) / 2.0f;
}

float InElastic(float t) {
    t = Clamped(t);
    if (t <= 0.0f || t >= 1.0f) return t;
    return -std::pow(2.0f, 10.0f * t - 10.0f) * std::sin((t * 10.0f - 10.75f) * (Tau / 3.0f));
}
float OutElastic(float t) {
    t = Clamped(t);
    if (t <= 0.0f || t >= 1.0f) return t;
    return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * (Tau / 3.0f)) + 1.0f;
}
float InOutElastic(float t) {
    t = Clamped(t);
    if (t <= 0.0f || t >= 1.0f) return t;
    constexpr float c = Tau / 4.5f;
    if (t < 0.5f) return -(std::pow(2.0f, 20.0f * t - 10.0f) * std::sin((20.0f * t - 11.125f) * c)) / 2.0f;
    return (std::pow(2.0f, -20.0f * t + 10.0f) * std::sin((20.0f * t - 11.125f) * c)) / 2.0f + 1.0f;
}

float OutBounce(float t) {
    t = Clamped(t);
    constexpr float n = 7.5625f, d = 2.75f;
    if (t < 1.0f / d) return n * t * t;
    if (t < 2.0f / d) {
        t -= 1.5f / d;
        return n * t * t + 0.75f;
    }
    if (t < 2.5f / d) {
        t -= 2.25f / d;
        return n * t * t + 0.9375f;
    }
    t -= 2.625f / d;
    return n * t * t + 0.984375f;
}
float InBounce(float t) { return 1.0f - OutBounce(1.0f - Clamped(t)); }
float InOutBounce(float t) {
    t = Clamped(t);
    return t < 0.5f ? (1.0f - OutBounce(1.0f - 2.0f * t)) / 2.0f : (1.0f + OutBounce(2.0f * t - 1.0f)) / 2.0f;
}

} // namespace BitMotor::Ease
