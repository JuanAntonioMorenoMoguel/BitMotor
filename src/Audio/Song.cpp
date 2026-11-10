// The BitMusic format (.bitmusic): reading, writing and the synthesizer that plays it.
// docs/BitMusic.md describes the format and the synthesizer exactly; keep both in step.
#include "Audio/AudioInternal.h"

#include "BitMotor/Core/Data.h"
#include "BitMotor/Core/Files.h"
#include "BitMotor/Core/Log.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>

namespace BitMotor {

namespace Detail {

namespace {

enum class Wave { Sine, Square, Saw, Triangle, Noise };

std::optional<Wave> ParseWave(std::string_view name) {
    if (name == "sine") return Wave::Sine;
    if (name == "square") return Wave::Square;
    if (name == "saw") return Wave::Saw;
    if (name == "triangle") return Wave::Triangle;
    if (name == "noise") return Wave::Noise;
    return std::nullopt;
}

using Event = NoteEntry;

std::string_view Trim(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.remove_prefix(1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.remove_suffix(1);
    return text;
}

std::optional<float> ParseFloat(std::string_view text) {
    float value = 0;
    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc() || end != text.data() + text.size()) return std::nullopt;
    return value;
}

// One entry of Notes. nullopt if it is not valid.
std::optional<Event> ParseEvent(std::string_view text) {
    text = Trim(text);
    if (text.empty() || text == "---" || text == ".") return Event{};
    if (text == "off") return Event{Event::Kind::Off};
    Event event{Event::Kind::On};
    if (std::size_t colon = text.find(':'); colon != std::string_view::npos) {
        std::optional<float> volume = ParseFloat(Trim(text.substr(colon + 1)));
        if (!volume || *volume < 0) return std::nullopt;
        event.Volume = *volume;
        text = Trim(text.substr(0, colon));
    }
    std::optional<float> note = ParseNote(text);
    if (!note) return std::nullopt;
    event.Note = *note;
    return event;
}

struct CompiledInstrument {
    Instrument Settings;
    Wave Shape = Wave::Square;
};

struct CompiledTrack {
    int Instrument = -1;
    std::vector<Event> Events;
};

struct CompiledPattern {
    int Length = 0;
    std::vector<CompiledTrack> Tracks;
};

CompiledInstrument CompileInstrument(const Instrument& instrument) {
    CompiledInstrument compiled;
    compiled.Settings = instrument;
    compiled.Settings.Attack = std::max(0.0f, instrument.Attack);
    compiled.Settings.Decay = std::max(0.0f, instrument.Decay);
    compiled.Settings.Sustain = std::clamp(instrument.Sustain, 0.0f, 1.0f);
    compiled.Settings.Release = std::max(0.0f, instrument.Release);
    compiled.Shape = ParseWave(instrument.Wave).value_or(Wave::Square);
    return compiled;
}

// One channel of the song: the note it plays and its envelope.
struct Channel {
    const CompiledInstrument* Instrument = nullptr;
    float Note = 0, Velocity = 1;
    float Phase = 0, Time = 0;
    bool Sounding = false, Releasing = false;
    float Level = 0, ReleaseFrom = 0, ReleaseTime = 0;
    float Held = 0; // the noise value
    Synth::LowPass Filter;
    Synth::Bitcrush Crush;
    Synth::Noise Noise;
    // The loudest sample since the mixer last asked (Peak), and the one it was told (Meter).
    float Peak = 0, Meter = 0;

    void Start(const CompiledInstrument* instrument, float note, float velocity) {
        Instrument = instrument;
        Note = note;
        Velocity = velocity;
        Phase = 0;
        Time = 0;
        Sounding = true;
        Releasing = false;
        Filter = {};
        Crush = {};
    }

    void Release() {
        if (!Sounding || Releasing) return;
        Releasing = true;
        ReleaseFrom = Level;
        ReleaseTime = 0;
    }

    // The envelope at Time (and ReleaseTime), 0..1.
    float Envelope() const {
        const BitMotor::Instrument& in = Instrument->Settings;
        if (Releasing) return in.Release > 0 ? ReleaseFrom * std::max(0.0f, 1.0f - ReleaseTime / in.Release) : 0.0f;
        if (Time < in.Attack) return Time / in.Attack;
        if (Time < in.Attack + in.Decay) return 1.0f + (in.Sustain - 1.0f) * (Time - in.Attack) / in.Decay;
        return in.Sustain;
    }

    // One sample, left and right. A muted channel plays the same but adds nothing.
    void Render(float& left, float& right, float dt, bool muted = false) {
        if (!Sounding) return;
        const BitMotor::Instrument& in = Instrument->Settings;
        Level = Envelope();
        if (Releasing && Level <= 0) {
            Sounding = false;
            return;
        }
        float note = Note + in.Transpose;
        if (in.Sweep != 0) note += in.Sweep * (in.SweepTime > 0 ? std::min(1.0f, Time / in.SweepTime) : 1.0f);
        const float hz = Synth::NoteToHz(note);
        float wave = 0;
        if (Instrument->Shape == Wave::Noise) {
            // A new random value 16 times per cycle of the note: higher notes, brighter noise.
            const float before = Phase;
            Synth::Advance(Phase, hz * 16.0f, dt);
            if (Phase < before || Time == 0) Held = Noise();
            wave = Held;
        } else {
            const float phase = Phase;
            Synth::Advance(Phase, hz, dt);
            switch (Instrument->Shape) {
            case Wave::Sine:
                wave = Synth::Sine(phase);
                break;
            case Wave::Square:
                wave = Synth::Square(phase, in.Width);
                break;
            case Wave::Saw:
                wave = Synth::Saw(phase);
                break;
            default:
                wave = Synth::Triangle(phase);
                break;
            }
        }
        float s = wave * Level * in.Volume * Velocity;
        if (in.LowPass < 1) s = Filter(s, std::max(in.LowPass, 0.0f));
        if (in.Crush > 0 || in.Downsample > 1) s = Crush(s, std::max(1, in.Downsample), float(in.Crush));
        const float pan = std::clamp(in.Pan, -1.0f, 1.0f);
        if (!muted) {
            left += s * std::min(1.0f, 1.0f - pan);
            right += s * std::min(1.0f, 1.0f + pan);
            Peak = std::max(Peak, std::abs(s));
        }
        Time += dt;
        if (Releasing) ReleaseTime += dt;
    }
};

struct SongSource final : MusicSource {
    std::vector<CompiledInstrument> Instruments;
    std::vector<CompiledPattern> Order;
    // For each pattern of Order, its position in the song's Order (which also counts unknown names).
    std::vector<int> SongIndex;
    std::vector<Channel> Channels;
    double SamplesPerRow = 0, ToNextRow = 0;
    float Volume = 1;
    int Loop = -1;
    int Position = 0, Row = 0;
    // Passes through Order still to start from Loop (-1: forever).
    int PassesLeft = -1;
    // The row that sounds now (-1 before the first).
    int PlayedPosition = -1, PlayedRow = 0;
    // Frames rendered until the song ended (Song::Render).
    std::uint64_t Frames = 0;
    // Last: the last row has started and the song ends when it is over. Ended: it is over.
    bool Last = false, Ended = false;

    void PlayRow() {
        const CompiledPattern& pattern = Order[std::size_t(Position)];
        PlayedPosition = SongIndex[std::size_t(Position)];
        PlayedRow = Row;
        for (std::size_t t = 0; t < pattern.Tracks.size(); ++t) {
            const CompiledTrack& track = pattern.Tracks[t];
            if (std::size_t(Row) >= track.Events.size()) continue;
            const Event& event = track.Events[std::size_t(Row)];
            Channel& channel = Channels[t];
            if (event.Type == Event::Kind::Off)
                channel.Release();
            else if (event.Type == Event::Kind::On && track.Instrument >= 0)
                channel.Start(&Instruments[std::size_t(track.Instrument)], event.Note, event.Volume);
        }
        Advance();
    }

    // The next row: the next pattern at the end of one, then back to Loop, or the end of the song.
    void Advance() {
        ++Row;
        std::size_t skipped = 0;
        while (Row >= Order[std::size_t(Position)].Length) {
            Row = 0;
            if (++Position >= int(Order.size())) {
                if (Loop < 0 || PassesLeft == 0) {
                    Last = true;
                    return;
                }
                if (PassesLeft > 0) --PassesLeft;
                Position = Loop;
            }
            if (++skipped > Order.size()) { // nothing but empty patterns
                Ended = true;
                return;
            }
        }
    }

    bool Render(float* out, int frames) override {
        const float dt = 1.0f / float(MixRate);
        for (std::size_t c = 0; c < Channels.size(); ++c) {
            Channels[c].Meter = Channels[c].Peak;
            Channels[c].Peak = 0;
        }
        for (int i = 0; i < frames; ++i) {
            if (!Ended) {
                while (ToNextRow <= 0) {
                    if (Last) { // the last row is over: the notes are released
                        Ended = true;
                        for (Channel& c : Channels) c.Release();
                        break;
                    }
                    PlayRow();
                    ToNextRow += SamplesPerRow;
                    if (Ended) break;
                }
                ToNextRow -= 1;
            }
            float left = 0, right = 0;
            for (std::size_t c = 0; c < Channels.size(); ++c)
                Channels[c].Render(left, right, dt, c < 64 && ((Muted >> c) & 1u) != 0);
            out[2 * i] += left * Volume;
            out[2 * i + 1] += right * Volume;
            ++Frames;
            if (Ended && std::none_of(Channels.begin(), Channels.end(), [](auto& c) { return c.Sounding; }))
                return false;
        }
        return true;
    }

    SongPosition CurrentPosition() const override {
        if (PlayedPosition < 0) return {SongIndex.empty() ? 0 : SongIndex[std::size_t(Position)], Row};
        return {PlayedPosition, PlayedRow};
    }

    float Level(int channel) const override {
        if (channel < 0 || channel >= int(Channels.size())) return 0;
        return Channels[std::size_t(channel)].Meter * Volume;
    }
};

// Music::PlayNote: one channel playing one note until it is released and has faded.
struct NoteSource final : MusicSource {
    CompiledInstrument Instrument;
    Channel Voice;

    bool Render(float* out, int frames) override {
        const float dt = 1.0f / float(MixRate);
        for (int i = 0; i < frames; ++i) {
            if (!Voice.Sounding) return false;
            float left = 0, right = 0;
            Voice.Render(left, right, dt);
            out[2 * i] += left;
            out[2 * i + 1] += right;
        }
        return Voice.Sounding;
    }

    void Release() override { Voice.Release(); }
};

std::unique_ptr<SongSource> Compile(const Song& song, bool loop, int startOrder = 0, int startRow = 0,
                                    int passes = -1) {
    auto source = std::make_unique<SongSource>();
    std::map<std::string, int> instruments;
    for (const auto& [name, instrument] : song.Instruments) {
        instruments[name] = int(source->Instruments.size());
        source->Instruments.push_back(CompileInstrument(instrument));
    }
    std::size_t channels = 0;
    for (std::size_t index = 0; index < song.Order.size(); ++index) {
        auto found = song.Patterns.find(song.Order[index]);
        if (found == song.Patterns.end()) continue; // reported by Validate
        source->SongIndex.push_back(int(index));
        CompiledPattern pattern;
        pattern.Length = std::max(0, found->second.Length);
        for (const SongTrack& track : found->second.Tracks) {
            CompiledTrack compiled;
            auto instrument = instruments.find(track.Instrument);
            compiled.Instrument = instrument == instruments.end() ? -1 : instrument->second;
            for (const std::string& note : track.Notes) compiled.Events.push_back(ParseEvent(note).value_or(Event{}));
            pattern.Tracks.push_back(std::move(compiled));
        }
        channels = std::max(channels, pattern.Tracks.size());
        source->Order.push_back(std::move(pattern));
    }
    source->PassesLeft = passes < 0 ? -1 : std::max(0, passes - 1);
    source->Channels.resize(channels);
    for (std::size_t i = 0; i < channels; ++i) source->Channels[i].Noise.State = 0x9E3779B9u + std::uint32_t(i) * 7919u;
    source->Bpm = song.Bpm > 0 ? song.Bpm : 120;
    const int rows = std::max(1, song.Rows);
    source->SamplesPerRow = double(MixRate) * 60.0 / (source->Bpm * rows);
    source->Volume = song.Volume;
    // Loop counts in the song's Order; the compiled Order skips unknown patterns, so it is mapped.
    source->Loop = -1;
    if (loop && song.Loop >= 0 && song.Loop < int(song.Order.size())) {
        int skipped = 0;
        for (int i = 0; i < song.Loop; ++i)
            if (!song.Patterns.contains(song.Order[std::size_t(i)])) ++skipped;
        source->Loop = std::min(song.Loop - skipped, int(source->Order.size()) - 1);
    }
    if (source->Order.empty() ||
        std::all_of(source->Order.begin(), source->Order.end(), [](auto& p) { return p.Length <= 0; })) {
        source->Ended = true;
        return source;
    }
    // The start: the first pattern at or after `startOrder` that exists (none: the beginning), and a row of it.
    if (startOrder > 0) {
        auto at = std::lower_bound(source->SongIndex.begin(), source->SongIndex.end(), startOrder);
        source->Position = at == source->SongIndex.end() ? 0 : int(at - source->SongIndex.begin());
    }
    const int length = source->Order[std::size_t(source->Position)].Length;
    source->Row = startRow > 0 && startRow < length ? startRow : 0;
    return source;
}

// false (logged) if it is not a song; what it refers to that does not exist is only a warning, and playing
// skips it.
bool Validate(const Song& song, std::string_view name) {
    if (song.Format != "bitmusic") {
        Log::Error("{}: not a BitMusic song (Format is \"{}\")", name, song.Format);
        return false;
    }
    if (song.Version > 1)
        Log::Warn("{}: made by a newer BitMusic (version {}); it may not sound right", name, song.Version);
    if (song.Bpm <= 0 || song.Rows <= 0) {
        Log::Error("{}: Bpm and Rows must be more than 0", name);
        return false;
    }
    if (song.Beats <= 0) Log::Warn("{}: Beats must be more than 0 (it is {})", name, song.Beats);
    for (const auto& [id, instrument] : song.Instruments)
        if (!ParseWave(instrument.Wave)) {
            Log::Warn("{}: instrument \"{}\" has an unknown Wave \"{}\" (square is used)", name, id, instrument.Wave);
        }
    for (const auto& [id, pattern] : song.Patterns)
        for (std::size_t t = 0; t < pattern.Tracks.size(); ++t) {
            const SongTrack& track = pattern.Tracks[t];
            if (!song.Instruments.contains(track.Instrument)) {
                Log::Warn("{}: pattern \"{}\", track {}: no instrument \"{}\"", name, id, t, track.Instrument);
            }
            for (std::size_t row = 0; row < track.Notes.size(); ++row)
                if (!ParseEvent(track.Notes[row])) {
                    Log::Warn("{}: pattern \"{}\", track {}, row {}: \"{}\" is not a note", name, id, t, row,
                              track.Notes[row]);
                }
        }
    for (const std::string& id : song.Order)
        if (!song.Patterns.contains(id)) {
            Log::Warn("{}: Order names a pattern \"{}\" that does not exist", name, id);
        }
    return true;
}

} // namespace

std::optional<float> ParseNote(std::string_view text) {
    text = Trim(text);
    if (text.empty()) return std::nullopt;
    if (std::isdigit(static_cast<unsigned char>(text[0]))) {
        std::optional<float> number = ParseFloat(text);
        if (!number || *number > 127) return std::nullopt;
        return number;
    }
    static constexpr int Semitones[7] = {9, 11, 0, 2, 4, 5, 7}; // A B C D E F G
    const char letter = char(std::toupper(static_cast<unsigned char>(text[0])));
    if (letter < 'A' || letter > 'G') return std::nullopt;
    int note = Semitones[letter - 'A'];
    text.remove_prefix(1);
    if (!text.empty() && (text[0] == '#' || text[0] == 'b')) {
        note += text[0] == '#' ? 1 : -1;
        text.remove_prefix(1);
    }
    int octave = 0;
    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), octave);
    if (text.empty() || error != std::errc() || end != text.data() + text.size() || octave < -1 || octave > 9)
        return std::nullopt;
    return float((octave + 1) * 12 + note);
}

std::unique_ptr<MusicSource> MakeSongSource(const Song& song, bool loop, int order, int row, int passes) {
    return Compile(song, loop, order, row, passes);
}

std::unique_ptr<MusicSource> MakeNoteSource(const Instrument& instrument, float note, float volume) {
    auto source = std::make_unique<NoteSource>();
    source->Instrument = CompileInstrument(instrument);
    source->Voice.Noise.State = 0x9E3779B9u;
    source->Voice.Start(&source->Instrument, note, std::max(0.0f, volume));
    return source;
}

std::vector<float> RenderSong(const Song& song, double seconds) {
    auto source = Compile(song, false);
    const int frames = int(seconds * MixRate);
    std::vector<float> out(std::size_t(frames) * 2, 0.0f);
    source->Render(out.data(), frames);
    return out;
}

} // namespace Detail

std::optional<NoteEntry> NoteEntry::Parse(std::string_view text) { return Detail::ParseEvent(text); }

std::string NoteEntry::ToString() const {
    if (Type == Kind::Keep) return "";
    if (Type == Kind::Off) return "off";
    std::string text = NoteName(Note);
    if (Volume != 1) {
        char buffer[32];
        auto [end, error] = std::to_chars(buffer, buffer + sizeof buffer, Volume);
        if (error == std::errc()) text += ":" + std::string(buffer, end);
    }
    return text;
}

std::optional<float> ParseNote(std::string_view text) { return Detail::ParseNote(text); }

std::string NoteName(float note) {
    static constexpr const char* Names[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    if (note >= 0 && note <= 127 && note == std::floor(note)) {
        const int n = int(note);
        return Names[n % 12] + std::to_string(n / 12 - 1);
    }
    char buffer[32];
    auto [end, error] = std::to_chars(buffer, buffer + sizeof buffer, note);
    return error == std::errc() ? std::string(buffer, end) : std::string("0");
}

std::vector<float> Song::Render(int passes) const {
    auto source = Detail::Compile(*this, passes > 1, 0, 0, std::max(1, passes));
    constexpr int Block = 4096;
    constexpr std::uint64_t Longest = std::uint64_t(Detail::MixRate) * 3600;
    std::vector<float> out;
    bool playing = true;
    while (playing && source->Frames < Longest) {
        const std::size_t at = out.size();
        out.resize(at + 2 * Block, 0.0f);
        playing = source->Render(out.data() + at, Block);
    }
    out.resize(std::size_t(std::min(source->Frames, Longest)) * 2);
    return out;
}

std::optional<Song> Song::Load(const std::filesystem::path& path) {
    const std::filesystem::path full = path.is_absolute() ? path : Files::AssetPath(path);
    std::optional<std::string> text = Files::Read(full);
    if (!text) {
        Log::Error("Song {}: cannot read the file", path.string());
        return std::nullopt;
    }
    return Parse(*text, path.string());
}

std::optional<Song> Song::Parse(std::string_view json, std::string_view name) {
    std::optional<Song> song = Data::FromJson<Song>(json, name);
    if (!song) return std::nullopt;
    if (!Detail::Validate(*song, name)) return std::nullopt;
    return song;
}

bool Song::Save(const std::filesystem::path& path) const {
    const std::filesystem::path full = path.is_absolute() ? path : Files::SavePath(path);
    return Files::Write(full, Data::ToJson(*this));
}

double Song::Duration() const {
    if (Bpm <= 0 || Rows <= 0) return 0;
    long long rows = 0;
    for (const std::string& id : Order)
        if (auto found = Patterns.find(id); found != Patterns.end()) rows += std::max(0, found->second.Length);
    return double(rows) * 60.0 / (double(Bpm) * Rows);
}

} // namespace BitMotor
