#pragma once
#include "BitMotor/Audio/Synth.h"
#include "BitMotor/Core/Fields.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Background music, one piece at a time (a new one fades in while the old one fades out):
//     Music::Play("music/title.ogg");             // WAV, FLAC, MP3, OGG (streamed) or a .bitmusic song
//     Music::Play("music/level1.bitmusic");       // made with BitMusic, played by BitMotor's synthesizer
//     Music::Procedural([](MusicState& m, float dt) { ... return sample; }, 0.5f, 120);
//     Music::SetParam(0, danger);                 // a procedural function reads it in m.Params[0]
//     Music::Stop(1.0f);                          // fading out over a second
//     Music::Play(song, 0, true, 2, 8);           // a song from Order position 2, row 8 (an editor's cursor)
//     SongPosition at = Music::Position();        // the row of the song that is sounding
//     Music::Mute(3);  float level = Music::Level(0);   // song channels, for an editor's mixer
//     int id = Music::PlayNote(song.Instruments["bass"], 48); Music::ReleaseNote(id);   // try an instrument
//     float beat = Music::Pulse();                // 1 on every beat, then falls: things can bounce with it
// The volume is Settings::MasterVolume and MusicVolume times Audio::SetVolume(Bus::Music).
namespace BitMotor {

// What a procedural music function keeps between samples. It runs on the audio thread.
struct MusicState {
    // Set from the game with Music::SetParam.
    std::array<float, 8> Params{};
    // Free for the function: oscillator phases, counters...
    std::array<float, 16> Memory{};
    Synth::Noise Noise;
    // Samples since this music started, at SampleRate per second.
    std::uint64_t Sample = 0;
    float SampleRate = 48000;

    double Seconds() const { return double(Sample) / double(SampleRate); }
    double Beat(double bpm) const { return Seconds() * bpm / 60.0; }
};

// One sample (-1..1) of procedural music; `dt` is one sample in seconds.
using MusicFunction = std::function<float(MusicState& state, float dt)>;

// ---------------------------------------------------------------- the BitMusic format (.bitmusic)
//
// A song for BitMotor's synthesizer, as BitMusic saves it (BitMusic imports and exports other formats too;
// BitMotor only reads this one). docs/BitMusic.md is the full description of the format and of how it sounds.
// It is JSON with these structs' fields:
//     {
//       "Format": "bitmusic", "Version": 1, "Bpm": 120, "Rows": 4, "Beats": 4, "Volume": 0.8, "Loop": 0,
//       "Instruments": {
//         "bass": {"Wave": "triangle", "Volume": 0.6, "Attack": 0.005, "Decay": 0.2, "Sustain": 0.5, "Release": 0.1},
//         "kick": {"Wave": "sine", "Decay": 0.15, "Sustain": 0, "Sweep": -24, "SweepTime": 0.08}
//       },
//       "Patterns": {
//         "intro": {"Length": 16, "Tracks": [{"Instrument": "bass", "Notes": ["C2", "", "", "", "G1", "", "off"]}]}
//       },
//       "Order": ["intro", "intro"]
//     }
// Rows: rows per beat (4: each row is a sixteenth note in 4/4). A track plays one note at a time; each entry
// of Notes is one row: a note ("C4", "F#3", "Bb2", or a MIDI number "60") starts a note, ":0.5" after it
// sets its volume ("C4:0.5"); "off" releases it; "" (or "---", ".") keeps what was playing.
// Track N of every pattern is the same channel, so a note goes on into the next pattern.
// Loop: the position in Order the song goes back to when it ends (-1: it stops). Beats: beats per bar, for
// editors and for exporting to other formats; it does not change the sound.

// One entry of a track's Notes, read as the synthesizer reads it:
//     NoteEntry::Parse("C4:0.5")  ->  {On, 60, 0.5};   NoteEntry::Parse("off")  ->  {Off}
//     NoteEntry{NoteEntry::Kind::On, 61}.ToString()  ->  "C#4"
struct NoteEntry {
    enum class Kind { Keep, On, Off };
    Kind Type = Kind::Keep;
    // For On: the MIDI note (60 is C4) and its volume.
    float Note = 0;
    float Volume = 1;

    // nullopt if the text is not a valid entry (it sounds as "" and the song warns when it is loaded).
    static std::optional<NoteEntry> Parse(std::string_view text);
    // "" (Keep), "off", or the note with ":volume" when the volume is not 1: "C#4", "C4:0.5", "61.5".
    std::string ToString() const;
};

// A note's MIDI number: "C4" (60), "A4" (69), "F#3", "Bb2", octaves -1..9, or the number itself ("60", "61.5";
// 0..127). nullopt if it is not one.
std::optional<float> ParseNote(std::string_view text);
// The name of a MIDI note with sharps: 60 "C4", 61 "C#4", -1..9 octaves; a fraction or a note out of 0..127
// is written as a number ("61.5").
std::string NoteName(float note);

struct Instrument {
    // "sine", "square", "saw", "triangle" or "noise".
    std::string Wave = "square";
    float Volume = 0.5f;
    // The square's duty cycle (0.5, 0.25, 0.125...).
    float Width = 0.5f;
    // The envelope, in seconds; Sustain is a level 0..1 held while the note is on.
    float Attack = 0.005f, Decay = 0.1f, Sustain = 0.7f, Release = 0.1f;
    // One-pole low-pass: 1 off, smaller is darker.
    float LowPass = 1;
    // Bitcrush: levels (0 off, 8 is crunchy) and samples each value is held.
    int Crush = 0;
    int Downsample = 1;
    // -1 left .. 1 right.
    float Pan = 0;
    // Pitch slide from the note's start, in semitones over SweepTime seconds (a kick: -24 over 0.08).
    float Sweep = 0, SweepTime = 0.1f;
    // Semitones added to every note.
    float Transpose = 0;
};
BITMOTOR_FIELDS(Instrument, Wave, Volume, Width, Attack, Decay, Sustain, Release, LowPass, Crush, Downsample, Pan,
                Sweep, SweepTime, Transpose)

struct SongTrack {
    std::string Instrument;
    std::vector<std::string> Notes;
};
BITMOTOR_FIELDS(SongTrack, Instrument, Notes)

struct Pattern {
    int Length = 16;
    std::vector<SongTrack> Tracks;
};
BITMOTOR_FIELDS(Pattern, Length, Tracks)

struct Song {
    std::string Format = "bitmusic";
    int Version = 1;
    float Bpm = 120;
    int Rows = 4;
    // Beats per bar (4 for 4/4, 3 for 3/4). Only for editors and other formats: the sound does not use it.
    int Beats = 4;
    float Volume = 1;
    int Loop = 0;
    std::map<std::string, BitMotor::Instrument> Instruments;
    std::map<std::string, BitMotor::Pattern> Patterns;
    std::vector<std::string> Order;

    // Reads a .bitmusic file (relative paths: Files::AssetPath). nullopt if it cannot be read (logged).
    static std::optional<Song> Load(const std::filesystem::path& path);
    static std::optional<Song> Parse(std::string_view json, std::string_view name = "song");
    // Writes it as JSON (relative paths: Files::SavePath).
    bool Save(const std::filesystem::path& path) const;
    // Seconds of one pass through Order.
    double Duration() const;
    // The song as samples, without the mixer or the player's volumes: stereo, interleaved, at
    // Audio::SampleRate() (48000), exactly as Music::Play makes it. `passes` 1 plays Order once; 2 or more
    // then plays it again from Loop that many times more minus one (with Loop -1 it is once anyway). After the
    // last pass the notes are released and it ends when they have faded. At most an hour.
    std::vector<float> Render(int passes = 1) const;
};
BITMOTOR_FIELDS(Song, Format, Version, Bpm, Rows, Beats, Volume, Loop, Instruments, Patterns, Order)

// ---------------------------------------------------------------- playing

// Where a song is playing: the position in Order (as in the file, counting entries that do not exist) and the
// row of that pattern that is sounding. Order is -1 when no song is playing.
struct SongPosition {
    int Order = -1;
    int Row = 0;
};

namespace Music {

// A file: .bitmusic songs are synthesized, anything else is decoded (WAV, FLAC, MP3, OGG) as it plays.
// `fade`: seconds of crossfade with what was playing. `loop` false plays it once. false if it cannot be read.
bool Play(const std::filesystem::path& path, float fade = 0.5f, bool loop = true);
// A song can start at a position in Order and a row of that pattern (from 0, 0 if they are out of range).
void Play(const Song& song, float fade = 0.5f, bool loop = true, int order = 0, int row = 0);
// Music made by a function. `bpm` is only for Beat and Pulse.
void Procedural(MusicFunction function, float fade = 0.5f, double bpm = 120);
void Stop(float fade = 0.5f);
// Something is playing (and not fading out).
bool IsPlaying();

// ---- for editors: songs played with Music::Play

// The row of the song that is sounding now (Order -1 when what plays is not a song).
SongPosition Position();
// Silences a channel of the songs (track N of every pattern is channel N; 0..63) without stopping it: it goes
// on playing in silence, so unmuting it is seamless. It stays muted for the next songs too.
void Mute(int channel, bool muted = true);
bool Muted(int channel);
// How loud a channel of the current song is: its peak in the last few milliseconds (0 when muted or silent).
float Level(int channel);
// Plays one note of an instrument at once, over the music and on its bus: to try instruments, or to play them
// from the keyboard. Up to 16 at a time (a new one takes over the oldest). Returns an id for ReleaseNote,
// which starts its Release; ReleaseNotes releases all of them.
int PlayNote(const Instrument& instrument, float note, float volume = 1);
void ReleaseNote(int id);
void ReleaseNotes();

// Parameters for procedural music (MusicState::Params, 8 of them).
void SetParam(int index, float value);
float Param(int index);

// Beats since the current music started, at its tempo: a song's Bpm, Procedural's bpm, or SetTempo for
// files (120 by default).
double Beat();
// 1 on each beat, falling quickly to 0 before the next.
float Pulse();
void SetTempo(double bpm);

} // namespace Music

} // namespace BitMotor
