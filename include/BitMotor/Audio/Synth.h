#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

// Small building blocks to make sounds and music out of numbers, sample by sample. They run on the audio
// thread (inside Sound::Synth and Music::Procedural functions), so they are tiny and never allocate.
//     float phase = Synth::Advance(voice.Phase[0], Synth::NoteToHz(69), dt);  // 440 Hz
//     float sample = Synth::Square(phase) * Synth::Fall(u);
namespace BitMotor::Synth {

inline constexpr float Tau = 6.28318530718f;

// MIDI note number to frequency: 69 is A4 (440 Hz), 60 is middle C, +12 is an octave up.
inline float NoteToHz(float note) { return 440.0f * std::pow(2.0f, (note - 69.0f) / 12.0f); }

// Moves an oscillator's phase (0..1) forward by `hz` for `dt` seconds and returns it.
inline float Advance(float& phase, float hz, float dt) {
    phase += hz * dt;
    phase -= std::floor(phase);
    return phase;
}

// Waves over a phase in 0..1, from -1 to 1.
inline float Sine(float phase) { return std::sin(phase * Tau); }
inline float Saw(float phase) { return phase * 2.0f - 1.0f; }
// `width`: the part of the cycle that is up (0.5 square, 0.25 or 0.125 the thinner NES pulses).
inline float Square(float phase, float width = 0.5f) { return phase < width ? 1.0f : -1.0f; }
inline float Triangle(float phase) { return 1.0f - 4.0f * std::abs(phase - 0.5f); }

// Fast white noise, -1..1, with its own state (xorshift).
struct Noise {
    std::uint32_t State = 0x9E3779B9u;
    float operator()() {
        State ^= State << 13;
        State ^= State >> 17;
        State ^= State << 5;
        return float(State & 0xFFFFFF) / float(0x7FFFFF) - 1.0f;
    }
};

// One-pole filters. `k` in (0, 1]: 1 lets everything through, near 0 keeps only the lows (LowPass) or
// only the highs (HighPass).
struct LowPass {
    float Y = 0;
    float operator()(float x, float k) { return Y += (x - Y) * k; }
};
struct HighPass {
    LowPass Low;
    float operator()(float x, float k) { return x - Low(x, k); }
};

// Fewer levels and a lower sample rate: the "8-bit" crunch. Holds each value `step` samples.
struct Bitcrush {
    float Held = 0;
    int Counter = 0;
    float operator()(float x, int step, float levels) {
        if (++Counter >= step) {
            Counter = 0;
            Held = levels > 0 ? std::round(x * levels) / levels : x;
        }
        return Held;
    }
};

// Envelopes over u = time / duration (0..1).
// 1 down to 0; `curve` > 1 falls faster at the start (percussive), < 1 holds and then drops.
inline float Fall(float u, float curve = 1.0f) { return std::pow(std::max(0.0f, 1.0f - u), curve); }
// Up during the first `attack` of the sound, then down to 0 at the end.
inline float AttackRelease(float u, float attack) {
    return u < attack ? u / attack : std::max(0.0f, 1.0f - (u - attack) / (1.0f - attack));
}
// Exponential decay over `seconds`: e^(-seconds * speed).
inline float Decay(float seconds, float speed) { return std::exp(-seconds * speed); }

} // namespace BitMotor::Synth
