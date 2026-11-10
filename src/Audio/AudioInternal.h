#pragma once
#include "BitMotor/Audio/Music.h"
#include "BitMotor/Audio/Sound.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

// The mixer. Everything below is shared by the main thread and the audio thread, behind one lock: the audio
// thread takes it once per block, the main thread for each call.
namespace BitMotor::Detail {

inline constexpr int MixRate = 48000;
inline constexpr int MixVoices = 64;

struct SoundData {
    // Decoded files and baked sounds: stereo, interleaved, at MixRate.
    std::vector<float> Samples;
    // Synthesized sounds.
    SoundFunction Function;
    float Duration = 0;
    int MaxVoices = 8;

    std::size_t Frames() const { return Samples.size() / 2; }
};

// Something that makes music, one block at a time. It runs on the audio thread.
struct MusicSource {
    virtual ~MusicSource() = default;
    // Adds `frames` stereo frames to `out` (interleaved). false once it has nothing more to play.
    virtual bool Render(float* out, int frames) = 0;
    // For Beat and Pulse.
    double Bpm = 120;
    // Songs only (audio thread, under the lock). Channels muted with Music::Mute: set by the mixer before
    // each Render.
    std::uint64_t Muted = 0;
    // Where the song is, and a channel's peak in the last block.
    virtual SongPosition CurrentPosition() const { return {}; }
    virtual float Level(int) const { return 0; }
    // A single note (Music::PlayNote): starts its release.
    virtual void Release() {}
};

// Decodes WAV, FLAC, MP3 or OGG into stereo at MixRate. nullopt (and the reason in `error`) if it cannot.
std::optional<std::vector<float>> DecodeAudio(std::span<const std::uint8_t> bytes, std::string* error = nullptr);
// A source that decodes as it plays. nullptr (and `error`) if the bytes are not a known format.
std::unique_ptr<MusicSource> MakeFileSource(std::vector<std::uint8_t> bytes, bool loop, std::string* error = nullptr);
// A song from `order` (a position in song.Order) and `row`. `passes`: how many times it goes through Order
// (from Loop after the first) before it ends; -1 forever (with `loop`).
std::unique_ptr<MusicSource> MakeSongSource(const Song& song, bool loop, int order = 0, int row = 0, int passes = -1);
// One note of an instrument, played until it is released and has faded (Music::PlayNote).
std::unique_ptr<MusicSource> MakeNoteSource(const Instrument& instrument, float note, float volume);
std::unique_ptr<MusicSource> MakeProceduralSource(MusicFunction function, double bpm);

// Adds what is playing to `out` (stereo, interleaved, starting at 0). Called by the device, or by
// UpdateAudio when there is none.
void MixAudio(float* out, int frames);

// Main thread, once per frame: the volumes from Settings, freeing finished sounds and, without a device,
// mixing `realDt` seconds in silence so that sounds end and Music::Beat advances.
void UpdateAudio(double realDt);
// Opens the audio device (false: none, everything plays in silence) and closes it.
bool StartAudio();
void StopAudio();
bool AudioDeviceOpen();
// Forgets every sound, the music and the volumes (tests).
void ResetAudio();

// Used by Sound and Music.
void PlaySound(const std::shared_ptr<SoundData>& sound, float volume, float pitch, float pan);
void StopSound(const SoundData* sound);
void PlayMusic(std::unique_ptr<MusicSource> source, float fade);
// A note as a MIDI number: "C4" (60), "F#3", "Bb2", or the number itself ("60"). nullopt if it is not one.
// (The same as BitMotor::ParseNote.)
std::optional<float> ParseNote(std::string_view text);
// The first `seconds` of a song as samples (stereo, interleaved, at MixRate), without the mixer: for tests.
std::vector<float> RenderSong(const Song& song, double seconds);
int ActiveVoices();

} // namespace BitMotor::Detail
