#include "Testing.h"

#include "Core/TimeInternal.h"

#include <BitMotor/BitMotor.h>

using namespace BitMotor;

namespace {
// Runs `frames` frames of `seconds` each and returns how many Updates ran.
int RunFrames(int frames, double seconds) {
    int updates = 0;
    for (int i = 0; i < frames; ++i) {
        int n = Detail::BeginFrame(seconds);
        for (int u = 0; u < n; ++u) Detail::BeginUpdate();
        Detail::BeginDraw();
        updates += n;
    }
    return updates;
}
} // namespace

TEST(TimeFixedStepIsExact) {
    Detail::ResetTime(60);
    CHECK_EQ(RunFrames(60, 1.0 / 60.0), 60); // no drift from rounding
    CHECK_EQ(Time::Tick(), std::uint64_t(60));
    CHECK_EQ(Time::Frame(), std::uint64_t(60));
    CHECK_NEAR(Time::Total(), 1.0, 1e-9);
    Detail::BeginUpdate();
    CHECK_NEAR(Time::Delta(), 1.0 / 60.0, 1e-7); // inside Update: always the fixed step

    Detail::ResetTime(120);
    CHECK_EQ(RunFrames(60, 1.0 / 60.0), 120); // two Updates per 60 Hz frame
    Detail::ResetTime(60);
    CHECK_EQ(RunFrames(144, 1.0 / 144.0), 60); // fast monitor: some frames without Update
}

TEST(TimeAlphaAndCatchUpLimit) {
    Detail::ResetTime(10);
    CHECK_EQ(Detail::BeginFrame(0.15), 1);
    CHECK_NEAR(Time::Alpha(), 0.5, 1e-5); // half way to the next Update
    Detail::ResetTime(60);
    CHECK_EQ(Detail::BeginFrame(5.0), Detail::MaxUpdatesPerFrame); // a long hang never runs 300 Updates
    CHECK_NEAR(Time::RealDelta(), 0.25, 1e-6);                     // and the real delta is capped
}

TEST(TimeScaleAndHitStop) {
    Detail::ResetTime(60);
    Time::SetScale(0.5f);
    CHECK_EQ(RunFrames(60, 1.0 / 60.0), 30); // half speed: half the Updates, same Delta
    CHECK_NEAR(Time::Total(), 0.5, 1e-9);
    CHECK_NEAR(Time::RealTotal(), 1.0, 1e-9);
    Detail::BeginDraw();
    CHECK_NEAR(Time::Delta(), 0.5 / 60.0, 1e-7); // inside Draw: scaled frame time
    Time::SetScale(-3);
    CHECK_EQ(Time::Scale(), 0.0f);

    // Powers of two keep the arithmetic exact.
    Detail::ResetTime(64);
    Time::HitStop(0.125f);
    Time::HitStop(0.05f); // the longest one stays
    CHECK(Time::InHitStop());
    CHECK_EQ(RunFrames(8, 1.0 / 64.0), 0); // 0.125 s frozen
    CHECK(!Time::InHitStop());
    CHECK_EQ(RunFrames(8, 1.0 / 64.0), 8);
    Detail::ResetTime(60);
}

TEST(TimeVariableStep) {
    Detail::ResetTime(0);
    CHECK_EQ(Detail::BeginFrame(0.02), 1);
    Detail::BeginUpdate();
    CHECK_NEAR(Time::Delta(), 0.02, 1e-7);
    CHECK_NEAR(Time::Alpha(), 1.0, 1e-7);
    Time::SetScale(0);
    CHECK_EQ(Detail::BeginFrame(0.02), 0); // paused: no Update
    Detail::ResetTime(60);
}

TEST(TimerCountsDown) {
    Timer timer{1.0f};
    CHECK(!timer.Done());
    CHECK(!timer.Update(0.4f));
    CHECK_NEAR(timer.Progress(), 0.4, 1e-6);
    CHECK_NEAR(timer.Remaining(), 0.6, 1e-6);
    CHECK(timer.Update(0.7f)); // true on the call that finishes
    CHECK(timer.Done());
    CHECK(!timer.Update(0.1f)); // and only that one
    CHECK_NEAR(timer.Progress(), 1.0, 1e-6);
    timer.Start(2.0f);
    CHECK(!timer.Done());
    timer.Finish();
    CHECK(timer.Done());
    CHECK(Timer{}.Done()); // zero duration
}
