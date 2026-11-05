#pragma once
#include <cstdint>

// Game time.
//     position += velocity * Time::Delta();
//     Time::HitStop(0.08f);       // freeze the action a moment on a big hit
//     Time::SetScale(0.3f);       // slow motion
//     Vec2 drawn = Lerp(previous, current, Time::Alpha()); // smooth drawing between fixed updates
//
// With a TickRate (the default), every Update sees the same Delta: 1 / TickRate. The scale and hit stop
// change how many Updates run per second, never their Delta, so the simulation stays deterministic.
namespace BitMotor::Time {

// Seconds of game time this step covers: the fixed step inside Update, the scaled frame time inside Draw.
// 0 during a hit stop.
float Delta();
// Real seconds since the previous frame, without scale or hit stop (menus, UI animations).
// Capped at 0.25 s so a pause in the debugger does not make everything jump.
float RealDelta();
// Seconds of game time since Start (sum of every Update's Delta).
double Total();
// Real seconds since Start.
double RealTotal();
// Frames drawn since Start.
std::uint64_t Frame();
// Updates run since Start (fixed steps: what replays count).
std::uint64_t Tick();

// Speed of game time: 1 normal, 0.5 half speed, 0 paused. Negative values count as 0.
void SetScale(float scale);
float Scale();

// Freezes game time for `seconds` of real time (Updates stop). Several calls keep the longest one.
void HitStop(float seconds);
bool InHitStop();

// How far the current frame is between the last Update and the next one, from 0 to 1. Always 1 with
// TickRate 0.
float Alpha();

} // namespace BitMotor::Time

namespace BitMotor {

// Countdown for cooldowns, spawns and delays.
//     Timer cooldown{0.5f};
//     if (cooldown.Update()) Shoot();   // true once, on the Update it finishes
//     if (cooldown.Done()) ...
//     float fill = cooldown.Progress(); // 0..1 for a bar
struct Timer {
    float Duration = 0;
    float Elapsed = 0;

    constexpr Timer() = default;
    // Starts running at once.
    constexpr explicit Timer(float duration) : Duration(duration) {}

    // Starts again, optionally with a new duration.
    void Start() { Elapsed = 0; }
    void Start(float duration) {
        Duration = duration;
        Elapsed = 0;
    }
    // Ends it now (Done is true).
    void Finish() { Elapsed = Duration; }
    // Advances by Time::Delta() (or by `dt`). Returns true only on the call that finishes it.
    bool Update();
    bool Update(float dt);

    bool Done() const { return Elapsed >= Duration; }
    float Progress() const { return Duration > 0 ? (Elapsed < Duration ? Elapsed / Duration : 1.0f) : 1.0f; }
    float Remaining() const { return Elapsed < Duration ? Duration - Elapsed : 0.0f; }
};

} // namespace BitMotor
