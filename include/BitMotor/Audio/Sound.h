#pragma once
#include "BitMotor/Audio/Synth.h"

#include <array>
#include <filesystem>
#include <functional>
#include <memory>

// Sound effects: from files (WAV, FLAC, MP3 or OGG) or made by a function, sample by sample.
//     Sound jump = Sound::Load("sounds/jump.wav");              // relative paths: Files::AssetPath
//     Sound blip = Sound::Synth(0.12f, [](Voice& v, float u, float dt) {
//         return Synth::Square(v.Advance(0, 880, dt)) * Synth::Fall(u) * 0.25f;
//     });
//     jump.Play();
//     blip.Play(0.8f, Random.Range(0.9f, 1.1f), -0.5f);        // volume, pitch, pan (left -1 .. right 1)
//
// Each sound plays at most MaxVoices copies at once (the oldest one stops for a new one), so a hundred
// coins picked up in one frame do not deafen anyone. Everything is mixed on the audio thread: synthesized
// sounds call their function there, so it must only use its Voice (and its own captured constants).
// The volumes are Settings::MasterVolume and SoundVolume times Audio::SetVolume. Without an audio device
// (a server, a CI machine) sounds play in silence, and nothing else changes.
namespace BitMotor {

// The state of one copy of a synthesized sound while it plays. The function is free to use the phases,
// filters and noise as it likes.
struct Voice {
    // Seconds since it started, and the sound's duration.
    float Time = 0, Duration = 1;
    // As given to Play.
    float Volume = 1, Pitch = 1;
    std::array<float, 4> Phase{};
    std::array<Synth::LowPass, 2> Filter{};
    Synth::Noise Noise;
    float Extra = 0;

    // Advances oscillator `index` (0..3) at `hz` times the voice's Pitch, and returns its phase.
    float Advance(int index, float hz, float dt) {
        return Synth::Advance(Phase[std::size_t(index) & 3], hz * Pitch, dt);
    }
};

// One sample (-1..1) of a synthesized sound. `u` is Time / Duration (0..1), `dt` one sample in seconds.
using SoundFunction = std::function<float(Voice& voice, float u, float dt)>;

namespace Detail {
struct SoundData;
}

class Sound {
public:
    // An empty sound: Play does nothing.
    Sound() = default;

    // WAV, FLAC, MP3 or OGG Vorbis, decoded once into memory. Empty (and a log error) if it cannot be read.
    static Sound Load(const std::filesystem::path& path, int maxVoices = 8);
    // From encoded bytes (any of those formats) already in memory.
    static Sound FromMemory(const void* data, std::size_t bytes, int maxVoices = 8);
    // A sound made by `function`, `duration` seconds long, computed live each time it plays.
    static Sound Synth(float duration, SoundFunction function, int maxVoices = 5);

    // The synthesized sound computed once into samples (cheaper to play many times; every copy sounds the
    // same, and Pitch then changes the speed too). A sound from a file is returned as it is.
    Sound Bake() const;

    // Plays one copy. Volume 0..1 (more is allowed), pitch 1 normal (2 an octave up, faster), pan -1..1.
    void Play(float volume = 1, float pitch = 1, float pan = 0) const;
    // Stops every copy of this sound that is playing.
    void Stop() const;

    void SetMaxVoices(int voices) const;
    int MaxVoices() const;
    // Seconds.
    float Duration() const;

    explicit operator bool() const { return Data != nullptr; }
    bool operator==(const Sound& other) const { return Data == other.Data; }

    // Internal: the samples or function behind this sound.
    const std::shared_ptr<Detail::SoundData>& Internal() const { return Data; }

private:
    std::shared_ptr<Detail::SoundData> Data;
};

// The three volumes: everything, the music, the sound effects.
enum class Bus { Master, Music, Sound };

namespace Audio {

// The game's own volume of a bus (0..1, default 1), on top of the player's Settings: for ducking the music
// under a dialogue, or muting effects in a cutscene.
void SetVolume(Bus bus, float volume);
float Volume(Bus bus);
// Paused: the music sounds muffled, as if behind a door (sound effects, like the menu's, still play).
void SetPaused(bool paused);
bool IsPaused();
// Stops every sound effect (not the music).
void StopSounds();
// There is an audio device: false means everything plays in silence.
bool Available();
// Samples per second of the mix (48000).
int SampleRate();

} // namespace Audio

} // namespace BitMotor
