#pragma once
#include "BitMotor/Colors/Color.h"
#include "BitMotor/Colors/Colors.h"
#include "BitMotor/Math/Functions.h"
#include "BitMotor/Math/Random.h"
#include "BitMotor/Math/Vector.h"
#include "BitMotor/Render/Sprite.h"
#include "BitMotor/Render/Texture.h"

#include <cstdint>
#include <vector>

// Particles on the CPU: sparks, dust, smoke, blood. A style says how they look and move; a Particles
// object keeps them, moves them in Update and draws them in Draw.
//     ParticleStyle sparks{.Speed = {40, 90}, .Spread = Tau, .Gravity = {0, 120}, .EndColor = Colors::Red.WithAlpha(0),
//                          .Layer = Layer::Glow};
//     Particles particles;                         // usually a member of the scene or a manager
//     particles.Emit(sparks, hitPoint, 20);        // in Update
//     particles.Update();                          // once per Update (uses Time::Delta: deterministic)
//     particles.Draw();                            // in Draw
namespace BitMotor {

// A range for a random value: each particle gets one in [Min, Max].
struct ParticleRange {
    float Min = 0, Max = 0;
    constexpr ParticleRange() = default;
    constexpr ParticleRange(float value) : Min(value), Max(value) {}
    constexpr ParticleRange(float min, float max) : Min(min), Max(max) {}
    bool operator==(const ParticleRange&) const = default;
};

struct ParticleStyle {
    // What each particle draws, centred. Empty: a 1x1 white pixel, so the sizes below are in pixels.
    BitMotor::Region Region = {};
    // Seconds each particle lives.
    ParticleRange Life = {0.4f, 0.8f};
    // Pixels per second at birth.
    ParticleRange Speed = {20, 60};
    // Radians: the middle of the directions they fly in (-Pi/2 is up), and how wide the cone is
    // (0: a line, Tau: every direction).
    float Direction = -Pi / 2;
    float Spread = Pi / 2;
    // Pixels per second squared (gravity, wind) and how much speed they lose per second (0..).
    Vec2 Gravity = {0, 0};
    float Drag = 0;
    // Scale of the region at birth and at death (with no region: size in pixels).
    ParticleRange StartSize = {2, 3};
    float EndSize = 0;
    // Colour at birth and at death (the alpha fades too).
    BitMotor::Color StartColor = Colors::White;
    BitMotor::Color EndColor = Colors::White.WithAlpha(0);
    // Radians per second, and whether each one starts at a random angle.
    ParticleRange Spin = {0, 0};
    bool RandomAngle = false;
    // Where they are drawn (Glow makes them shine) and their depth in the World layer.
    BitMotor::Layer Layer = Layer::World;
    float Depth = 0;

    bool operator==(const ParticleStyle&) const = default;
};

class Particles {
public:
    // At most `capacity` alive at once (new ones are dropped when full). Same seed, same particles.
    explicit Particles(int capacity = 2048, std::uint64_t seed = 1);

    // Starts `count` particles at `position` (world pixels, or screen pixels in the Hud layer).
    void Emit(const ParticleStyle& style, Vec2 position, int count = 1);
    // Ages and moves them by Time::Delta; the dead ones go.
    void Update();
    // Queues them for this frame (Screen::Draw).
    void Draw() const;
    void Clear();

    int Count() const { return int(Alive.size()); }
    int Capacity() const { return MaxCount; }

private:
    struct Particle {
        Vec2 Position, Velocity;
        float Age = 0, Life = 1, Angle = 0, Spin = 0, Size = 1;
        int Style = 0;
    };
    std::vector<Particle> Alive;
    std::vector<ParticleStyle> Styles;
    BitMotor::Random Random;
    int MaxCount;
};

} // namespace BitMotor
