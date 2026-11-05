#include "BitMotor/Core/Time.h"

#include "Core/TimeInternal.h"

#include <algorithm>
#include <cmath>

namespace BitMotor {

namespace {

struct State {
    int TickRate = 60;
    double Step = 1.0 / 60.0;
    double Accumulator = 0;
    double HitStopLeft = 0;
    float Scale = 1;
    float Delta = 0, RealDelta = 0, FrameDelta = 0, Alpha = 1;
    double Total = 0, RealTotal = 0;
    std::uint64_t Frame = 0, Tick = 0;
};
State Now;

} // namespace

namespace Detail {

void ResetTime(int tickRate) {
    Now = State{};
    Now.TickRate = std::max(tickRate, 0);
    Now.Step = Now.TickRate > 0 ? 1.0 / Now.TickRate : 0.0;
}

int BeginFrame(double realDt) {
    realDt = std::clamp(realDt, 0.0, 0.25);
    ++Now.Frame;
    Now.RealDelta = float(realDt);
    Now.RealTotal += realDt;

    // Real time left for the game after the hit stop takes its part.
    double running = realDt;
    if (Now.HitStopLeft > 0) {
        double frozen = std::min(Now.HitStopLeft, realDt);
        Now.HitStopLeft -= frozen;
        running -= frozen;
    }
    double gameDt = running * Now.Scale;
    Now.FrameDelta = float(gameDt);

    if (Now.TickRate == 0) {
        Now.Alpha = 1;
        return gameDt > 0 ? 1 : 0;
    }
    Now.Accumulator = std::min(Now.Accumulator + gameDt, Now.Step * MaxUpdatesPerFrame);
    // A tiny tolerance: 60 frames of exactly 1/60 s must give exactly 60 Updates despite rounding.
    int updates = int(std::floor((Now.Accumulator + 1e-7) / Now.Step));
    updates = std::min(updates, MaxUpdatesPerFrame);
    Now.Accumulator = std::max(Now.Accumulator - updates * Now.Step, 0.0);
    Now.Alpha = float(std::clamp(Now.Accumulator / Now.Step, 0.0, 1.0));
    return updates;
}

void BeginUpdate() {
    Now.Delta = Now.TickRate > 0 ? float(Now.Step) : Now.FrameDelta;
    Now.Total += Now.TickRate > 0 ? Now.Step : double(Now.FrameDelta);
    ++Now.Tick;
}

void BeginDraw() { Now.Delta = Now.FrameDelta; }

} // namespace Detail

namespace Time {

float Delta() { return Now.Delta; }
float RealDelta() { return Now.RealDelta; }
double Total() { return Now.Total; }
double RealTotal() { return Now.RealTotal; }
std::uint64_t Frame() { return Now.Frame; }
std::uint64_t Tick() { return Now.Tick; }

void SetScale(float scale) { Now.Scale = scale > 0 && std::isfinite(scale) ? scale : 0.0f; }
float Scale() { return Now.Scale; }

void HitStop(float seconds) { Now.HitStopLeft = std::max(Now.HitStopLeft, double(seconds)); }
bool InHitStop() { return Now.HitStopLeft > 0; }

float Alpha() { return Now.Alpha; }

} // namespace Time

bool Timer::Update() { return Update(Time::Delta()); }

bool Timer::Update(float dt) {
    if (Done()) return false;
    Elapsed += dt;
    return Done();
}

} // namespace BitMotor
