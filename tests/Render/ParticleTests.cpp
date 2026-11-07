#include "Printing.h"
#include "Testing.h"

#include "BitMotor/Render/Particles.h"
#include "Core/TimeInternal.h"
#include "Render/SpritesInternal.h"

using namespace BitMotor;

namespace {

void Step(Particles& particles, int updates) {
    for (int i = 0; i < updates; ++i) {
        Detail::BeginUpdate();
        particles.Update();
    }
}

} // namespace

TEST(ParticlesLiveTheirLife) {
    Detail::ResetTime(60);
    Particles particles;
    ParticleStyle style{.Life = 0.5f, .Speed = 60, .Direction = 0, .Spread = 0};
    particles.Emit(style, {10, 10}, 5);
    CHECK_EQ(particles.Count(), 5);
    Step(particles, 15); // 0.25 s
    CHECK_EQ(particles.Count(), 5);
    Step(particles, 16); // past 0.5 s
    CHECK_EQ(particles.Count(), 0);
}

TEST(ParticlesMoveWithSpeedGravityAndDrag) {
    Detail::ResetTime(60);
    Particles particles;
    // Straight right at 60 px/s, no gravity: 1 px per update.
    particles.Emit(ParticleStyle{.Life = 10, .Speed = 60, .Direction = 0, .Spread = 0, .StartSize = 4, .EndSize = 4},
                   {0, 0});
    Step(particles, 30);
    Detail::ResetSprites();
    particles.Draw();
    CHECK_EQ(Detail::SpriteQueue().size(), std::size_t(1));
    CHECK_NEAR(Detail::SpriteQueue()[0].Instance.X - CurrentCamera().ToScreen({0, 0}).X, 30.0f, 1e-3f);
    CHECK_EQ(Detail::SpriteQueue()[0].Instance.Width, 4.0f);

    // Falling: gravity pulls down.
    Particles falling;
    falling.Emit(ParticleStyle{.Life = 10, .Speed = 0, .Gravity = {0, 100}}, {0, 0});
    Particles dragged;
    dragged.Emit(ParticleStyle{.Life = 10, .Speed = 100, .Direction = 0, .Spread = 0, .Drag = 3}, {0, 0});
    Particles free;
    free.Emit(ParticleStyle{.Life = 10, .Speed = 100, .Direction = 0, .Spread = 0}, {0, 0});
    Step(falling, 60);
    Step(dragged, 60);
    Step(free, 60);
    Detail::ResetSprites();
    falling.Draw();
    dragged.Draw();
    free.Draw();
    auto& queue = Detail::SpriteQueue();
    Vec2 origin = CurrentCamera().ToScreen({0, 0});
    CHECK(queue[0].Instance.Y - origin.Y > 40);                             // about 50 px after a second
    CHECK(queue[1].Instance.X - origin.X < queue[2].Instance.X - origin.X); // drag slows it
}

TEST(ParticlesAreDeterministic) {
    ParticleStyle smoke{.Speed = {10, 50}, .Spread = Tau, .Spin = {-2, 2}, .RandomAngle = true};
    auto run = [&](std::uint64_t seed) {
        Detail::ResetTime(60);
        Particles particles(100, seed);
        particles.Emit(smoke, {50, 50}, 20);
        Step(particles, 10);
        Detail::ResetSprites();
        particles.Draw();
        std::vector<float> xs;
        for (auto& s : Detail::SpriteQueue()) xs.push_back(s.Instance.X + s.Instance.Rotation);
        return xs;
    };
    CHECK(run(3) == run(3));
    CHECK(run(3) != run(4));
}

TEST(ParticlesStopAtCapacityAndFade) {
    Detail::ResetTime(60);
    Particles particles(10);
    ParticleStyle style{.Life = 1, .StartColor = Colors::Red, .EndColor = Colors::Blue};
    particles.Emit(style, {0, 0}, 25);
    CHECK_EQ(particles.Count(), 10);
    CHECK_EQ(particles.Capacity(), 10);
    Step(particles, 30); // halfway
    Detail::ResetSprites();
    particles.Draw();
    CHECK_NEAR(Detail::SpriteQueue()[0].Instance.R, 0.5f, 0.02f);
    CHECK_NEAR(Detail::SpriteQueue()[0].Instance.B, 0.5f, 0.02f);
    particles.Clear();
    CHECK_EQ(particles.Count(), 0);
    Detail::ResetSprites();
}
