#include "BitMotor/Render/Particles.h"

#include "BitMotor/Core/Time.h"

#include <algorithm>

namespace BitMotor {

namespace {

// The default particle: one white pixel, shared by every Particles.
const Region& WhitePixel() {
    static const Region pixel = [] {
        const std::uint8_t white[4] = {255, 255, 255, 255};
        return Texture::FromPixels(1, 1, white).Full();
    }();
    return pixel;
}

float Pick(BitMotor::Random& random, ParticleRange range) {
    return range.Max > range.Min ? random.Range(range.Min, range.Max) : range.Min;
}

} // namespace

Particles::Particles(int capacity, std::uint64_t seed) : Random(seed), MaxCount(std::max(capacity, 1)) {
    Alive.reserve(std::size_t(std::min(MaxCount, 4096)));
}

void Particles::Emit(const ParticleStyle& style, Vec2 position, int count) {
    if (count <= 0 || Count() >= MaxCount) return;
    auto found = std::find(Styles.begin(), Styles.end(), style);
    int index = int(found - Styles.begin());
    if (found == Styles.end()) Styles.push_back(style);
    for (int i = 0; i < count && Count() < MaxCount; ++i) {
        Particle p;
        p.Position = position;
        float angle = style.Direction + (Random.Unit() - 0.5f) * style.Spread;
        p.Velocity = FromAngle(angle, Pick(Random, style.Speed));
        p.Life = Max(Pick(Random, style.Life), 0.001f);
        p.Spin = Pick(Random, style.Spin);
        p.Angle = style.RandomAngle ? Random.Range(0, Tau) : 0.0f;
        p.Size = Pick(Random, style.StartSize);
        p.Style = index;
        Alive.push_back(p);
    }
}

void Particles::Update() {
    const float dt = Time::Delta();
    for (std::size_t i = 0; i < Alive.size();) {
        Particle& p = Alive[i];
        p.Age += dt;
        if (p.Age >= p.Life) {
            p = Alive.back(); // order does not matter, and it stays deterministic
            Alive.pop_back();
            continue;
        }
        const ParticleStyle& style = Styles[std::size_t(p.Style)];
        p.Velocity += style.Gravity * dt;
        if (style.Drag > 0) p.Velocity = p.Velocity * Max(0.0f, 1.0f - style.Drag * dt);
        p.Position += p.Velocity * dt;
        p.Angle += p.Spin * dt;
        ++i;
    }
    if (Alive.empty()) Styles.clear();
}

void Particles::Draw() const {
    for (const Particle& p : Alive) {
        const ParticleStyle& style = Styles[std::size_t(p.Style)];
        float t = p.Age / p.Life;
        float size = Lerp(p.Size, style.EndSize, t);
        if (size <= 0) continue;
        Screen::Draw(Sprite2D{.Region = style.Region ? style.Region : WhitePixel(),
                              .Position = p.Position,
                              .Rotation = p.Angle,
                              .Scale = {size, size},
                              .Origin = Vec2(0.5f, 0.5f),
                              .Color = style.StartColor.Lerp(style.EndColor, t),
                              .Layer = style.Layer,
                              .Depth = style.Depth});
    }
}

void Particles::Clear() {
    Alive.clear();
    Styles.clear();
}

} // namespace BitMotor
