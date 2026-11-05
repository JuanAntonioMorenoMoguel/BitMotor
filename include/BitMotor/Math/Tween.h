#pragma once
#include "BitMotor/Math/Ease.h"
#include "BitMotor/Math/Functions.h"

// A value that goes from `From` to `To` in `Duration` seconds following an easing curve.
// Works with float, Vec2, Vec3, Vec4, Color and any type with a Lerp(a, b, t).
//     Tween<Vec2> slide{{-200, 300}, {100, 300}, 0.5f, Ease::OutCubic};
//     slide.Update(Time::Delta());
//     panelPosition = slide.Value();
//     if (slide.Done()) ...
namespace BitMotor {

template <class T> struct Tween {
    T From{}, To{};
    float Duration = 1.0f;
    Ease::Function Easing = Ease::Linear;
    float Elapsed = 0.0f;

    constexpr Tween() = default;
    constexpr Tween(T from, T to, float duration, Ease::Function easing = Ease::Linear)
        : From(from), To(to), Duration(duration), Easing(easing) {}

    // Starts again from the beginning.
    void Start() { Elapsed = 0.0f; }
    // Starts again towards a new value, from the current one (no jumps).
    void Retarget(T to) {
        From = Value();
        To = to;
        Elapsed = 0.0f;
    }
    void Update(float dt) { Elapsed = Min(Elapsed + dt, Duration); }

    // Linear progress in [0, 1]; a zero duration is done at once.
    float Progress() const { return Duration > 0.0f ? Clamp(Elapsed / Duration, 0.0f, 1.0f) : 1.0f; }
    bool Done() const { return Progress() >= 1.0f; }
    T Value() const { return Lerp(From, To, Easing ? Easing(Progress()) : Progress()); }
};

} // namespace BitMotor
