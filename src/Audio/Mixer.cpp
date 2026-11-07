// The mixer: sound effects, the music (two slots for the crossfade), the buses and the pause filter.
#include "Audio/AudioInternal.h"

#include "BitMotor/Core/Files.h"
#include "BitMotor/Core/Log.h"
#include "BitMotor/Core/Settings.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <mutex>

namespace BitMotor {

namespace Detail {

namespace {

constexpr int Block = 256;
constexpr float SampleTime = 1.0f / float(MixRate);
// Gains move this much of the way to their target each sample (about 10 ms): no clicks when they change.
constexpr float GainSmoothing = 0.002f;
// The music's low-pass while paused.
constexpr float PausedMuffle = 0.05f;

struct Playing {
    std::shared_ptr<SoundData> Sound;
    BitMotor::Voice Voice;
    double Position = 0; // frames into Sound->Samples
    float Volume = 1, Pitch = 1, Left = 1, Right = 1;
    std::uint64_t Order = 0;
};

struct Slot {
    std::unique_ptr<MusicSource> Source;
    float Gain = 0, Target = 0, Step = 0;
};

struct ProceduralSource;

struct Mixer {
    std::mutex Lock;
    std::array<Playing, MixVoices> Voices;
    std::uint64_t Counter = 0;
    Slot Current, Old;
    // Freed on the main thread (UpdateAudio): the audio thread never frees memory.
    std::vector<std::shared_ptr<SoundData>> RetiredSounds;
    std::vector<std::unique_ptr<MusicSource>> RetiredMusic;
    std::array<float, 8> Params{};
    // The volumes (Settings times Audio::SetVolume) and where they are on their way to them.
    float MasterTarget = 1, MusicTarget = 1, SoundTarget = 1;
    float Master = 1, MusicGain = 1, SoundGain = 1;
    bool Paused = false;
    float Muffle = 1;
    Synth::LowPass MuffleLeft, MuffleRight;
    std::uint64_t MusicSamples = 0;
    // Mixing without a device: seconds not mixed yet.
    double Pending = 0;

    Mixer() {
        RetiredSounds.reserve(MixVoices * 2);
        RetiredMusic.reserve(8);
    }
};

Mixer& Mix() {
    static Mixer mixer;
    return mixer;
}

// The game's own bus volumes (main thread).
std::array<float, 3> BusVolumes{1, 1, 1};

void Retire(Mixer& m, Playing& voice) {
    if (!voice.Sound) return;
    m.RetiredSounds.push_back(std::move(voice.Sound));
    voice.Sound = nullptr;
}

void Retire(Mixer& m, Slot& slot) {
    if (slot.Source) m.RetiredMusic.push_back(std::move(slot.Source));
    slot = Slot{};
}

// Loud mixes are bent smoothly instead of cut: below 0.8 nothing changes.
float SoftClip(float x) {
    const float a = std::abs(x);
    if (a <= 0.8f) return x;
    return std::copysign(0.8f + 0.2f * std::tanh((a - 0.8f) / 0.2f), x);
}

// One voice, `frames` frames, added to `out`. false once it has ended.
bool RenderVoice(Playing& v, float* out, int frames) {
    const SoundData& data = *v.Sound;
    const float left = v.Volume * v.Left, right = v.Volume * v.Right;
    if (data.Function) {
        for (int i = 0; i < frames; ++i) {
            if (v.Voice.Time >= v.Voice.Duration) return false;
            const float u = v.Voice.Duration > 0 ? v.Voice.Time / v.Voice.Duration : 1.0f;
            const float s = data.Function(v.Voice, u, SampleTime);
            v.Voice.Time += SampleTime;
            out[2 * i] += s * left;
            out[2 * i + 1] += s * right;
        }
        return true;
    }
    const std::size_t count = data.Frames();
    const float* samples = data.Samples.data();
    for (int i = 0; i < frames; ++i) {
        if (v.Position >= double(count)) return false;
        const std::size_t at = std::size_t(v.Position);
        const float t = float(v.Position - double(at));
        const std::size_t next = std::min(at + 1, count - 1);
        const float l = samples[2 * at] + (samples[2 * next] - samples[2 * at]) * t;
        const float r = samples[2 * at + 1] + (samples[2 * next + 1] - samples[2 * at + 1]) * t;
        out[2 * i] += l * left;
        out[2 * i + 1] += r * right;
        v.Position += double(v.Pitch);
    }
    return true;
}

// One music slot into `out`, with its fade. false once it has finished or faded out.
bool RenderSlot(Slot& slot, float* scratch, float* out, int frames) {
    std::fill(scratch, scratch + 2 * frames, 0.0f);
    const bool alive = slot.Source->Render(scratch, frames);
    for (int i = 0; i < frames; ++i) {
        if (slot.Gain < slot.Target)
            slot.Gain = std::min(slot.Target, slot.Gain + slot.Step);
        else if (slot.Gain > slot.Target)
            slot.Gain = std::max(slot.Target, slot.Gain - slot.Step);
        out[2 * i] += scratch[2 * i] * slot.Gain;
        out[2 * i + 1] += scratch[2 * i + 1] * slot.Gain;
    }
    return alive && !(slot.Target <= 0 && slot.Gain <= 0);
}

void MixBlock(Mixer& m, float* out, int frames) {
    float sounds[2 * Block] = {}, music[2 * Block] = {}, scratch[2 * Block];
    for (Playing& v : m.Voices)
        if (v.Sound && !RenderVoice(v, sounds, frames)) Retire(m, v);
    if (m.Current.Source && !RenderSlot(m.Current, scratch, music, frames)) Retire(m, m.Current);
    if (m.Old.Source && !RenderSlot(m.Old, scratch, music, frames)) Retire(m, m.Old);
    m.MusicSamples += std::uint64_t(frames);

    const float muffleTarget = m.Paused ? PausedMuffle : 1.0f;
    for (int i = 0; i < frames; ++i) {
        m.Master += (m.MasterTarget - m.Master) * GainSmoothing;
        m.MusicGain += (m.MusicTarget - m.MusicGain) * GainSmoothing;
        m.SoundGain += (m.SoundTarget - m.SoundGain) * GainSmoothing;
        m.Muffle += (muffleTarget - m.Muffle) * GainSmoothing;
        float l = music[2 * i], r = music[2 * i + 1];
        // The filter always runs, so that it starts from the right value when the game pauses.
        const float fl = m.MuffleLeft(l, m.Muffle), fr = m.MuffleRight(r, m.Muffle);
        if (m.Muffle < 0.999f) {
            l = fl;
            r = fr;
        }
        out[2 * i] += SoftClip(m.Master * (l * m.MusicGain + sounds[2 * i] * m.SoundGain));
        out[2 * i + 1] += SoftClip(m.Master * (r * m.MusicGain + sounds[2 * i + 1] * m.SoundGain));
    }
}

void MixLocked(Mixer& m, float* out, int frames) {
    for (int done = 0; done < frames; done += Block) MixBlock(m, out + 2 * done, std::min(Block, frames - done));
}

struct ProceduralSource final : MusicSource {
    MusicFunction Function;
    MusicState State;

    bool Render(float* out, int frames) override {
        State.Params = Mix().Params; // the lock is held while mixing
        for (int i = 0; i < frames; ++i) {
            const float s = Function(State, SampleTime);
            ++State.Sample;
            out[2 * i] += s;
            out[2 * i + 1] += s;
        }
        return true;
    }
};

} // namespace

void MixAudio(float* out, int frames) {
    Mixer& m = Mix();
    std::lock_guard lock(m.Lock);
    MixLocked(m, out, frames);
}

void UpdateAudio(double realDt) {
    Mixer& m = Mix();
    const Settings& settings = Settings::Get();
    std::vector<std::shared_ptr<SoundData>> sounds;
    std::vector<std::unique_ptr<MusicSource>> music;
    {
        std::lock_guard lock(m.Lock);
        m.MasterTarget = std::max(0.0f, settings.MasterVolume * BusVolumes[0]);
        m.MusicTarget = std::max(0.0f, settings.MusicVolume * BusVolumes[1]);
        m.SoundTarget = std::max(0.0f, settings.SoundVolume * BusVolumes[2]);
        if (!AudioDeviceOpen()) {
            // In silence, at the speed of real time (at most a quarter of a second each frame).
            m.Pending = std::min(m.Pending + std::max(0.0, realDt), 0.25);
            const int frames = int(m.Pending * MixRate);
            m.Pending -= double(frames) / MixRate;
            std::vector<float> discard(std::size_t(frames) * 2);
            if (frames > 0) MixLocked(m, discard.data(), frames);
        }
        sounds.swap(m.RetiredSounds);
        music.swap(m.RetiredMusic);
        m.RetiredSounds.reserve(MixVoices * 2);
        m.RetiredMusic.reserve(8);
    }
    // Freed here, outside the lock.
}

void ResetAudio() {
    Mixer& m = Mix();
    std::vector<std::shared_ptr<SoundData>> sounds;
    std::vector<std::unique_ptr<MusicSource>> music;
    {
        std::lock_guard lock(m.Lock);
        for (Playing& v : m.Voices) Retire(m, v);
        Retire(m, m.Current);
        Retire(m, m.Old);
        sounds.swap(m.RetiredSounds);
        music.swap(m.RetiredMusic);
        m.RetiredSounds.reserve(MixVoices * 2);
        m.RetiredMusic.reserve(8);
        m.Params = {};
        m.MasterTarget = m.MusicTarget = m.SoundTarget = 1;
        m.Master = m.MusicGain = m.SoundGain = 1;
        m.Paused = false;
        m.Muffle = 1;
        m.MuffleLeft = {};
        m.MuffleRight = {};
        m.MusicSamples = 0;
        m.Pending = 0;
        m.Counter = 0;
    }
    BusVolumes = {1, 1, 1};
}

void PlaySound(const std::shared_ptr<SoundData>& sound, float volume, float pitch, float pan) {
    if (!sound || volume <= 0 || pitch <= 0) return;
    if (!sound->Function && sound->Samples.empty()) return;
    Mixer& m = Mix();
    std::lock_guard lock(m.Lock);
    // A copy of the same sound to take over when it already has MaxVoices; otherwise a free one; otherwise
    // the oldest of all.
    Playing* chosen = nullptr;
    Playing* oldestSame = nullptr;
    Playing* oldest = nullptr;
    int same = 0;
    for (Playing& v : m.Voices) {
        if (!v.Sound) {
            if (!chosen) chosen = &v;
            continue;
        }
        if (v.Sound == sound) {
            ++same;
            if (!oldestSame || v.Order < oldestSame->Order) oldestSame = &v;
        }
        if (!oldest || v.Order < oldest->Order) oldest = &v;
    }
    if (same >= std::max(1, sound->MaxVoices)) chosen = oldestSame;
    if (!chosen) chosen = oldest;
    Retire(m, *chosen);

    Playing& v = *chosen;
    v.Sound = sound;
    v.Voice = BitMotor::Voice{};
    v.Voice.Duration = sound->Duration;
    v.Voice.Volume = volume;
    v.Voice.Pitch = pitch;
    v.Voice.Noise.State = std::uint32_t(0x9E3779B9u ^ (m.Counter * 2654435761u)) | 1u;
    v.Position = 0;
    v.Volume = volume;
    v.Pitch = pitch;
    pan = std::clamp(pan, -1.0f, 1.0f);
    v.Left = std::min(1.0f, 1.0f - pan);
    v.Right = std::min(1.0f, 1.0f + pan);
    v.Order = ++m.Counter;
}

void StopSound(const SoundData* sound) {
    Mixer& m = Mix();
    std::lock_guard lock(m.Lock);
    for (Playing& v : m.Voices)
        if (v.Sound.get() == sound) Retire(m, v);
}

void PlayMusic(std::unique_ptr<MusicSource> source, float fade) {
    Mixer& m = Mix();
    std::lock_guard lock(m.Lock);
    const float step = fade > 0 ? 1.0f / (fade * float(MixRate)) : 1.0f;
    Retire(m, m.Old);
    if (m.Current.Source) {
        if (fade > 0) {
            m.Old = std::move(m.Current);
            m.Old.Target = 0;
            m.Old.Step = step;
        }
        Retire(m, m.Current);
    }
    if (!source) return;
    m.Current.Source = std::move(source);
    m.Current.Gain = fade > 0 ? 0.0f : 1.0f;
    m.Current.Target = 1;
    m.Current.Step = step;
    m.MusicSamples = 0;
}

std::unique_ptr<MusicSource> MakeProceduralSource(MusicFunction function, double bpm) {
    auto source = std::make_unique<ProceduralSource>();
    source->Function = std::move(function);
    source->State.SampleRate = float(MixRate);
    source->Bpm = bpm;
    return source;
}

int ActiveVoices() {
    Mixer& m = Mix();
    std::lock_guard lock(m.Lock);
    return int(std::count_if(m.Voices.begin(), m.Voices.end(), [](const Playing& v) { return v.Sound != nullptr; }));
}

} // namespace Detail

// ---------------------------------------------------------------- Sound

namespace {

std::filesystem::path AssetFile(const std::filesystem::path& path) {
    return path.is_absolute() ? path : Files::AssetPath(path);
}

} // namespace

Sound Sound::Load(const std::filesystem::path& path, int maxVoices) {
    auto bytes = Files::ReadBytes(AssetFile(path));
    if (!bytes) {
        Log::Error("Sound {}: cannot read the file", path.string());
        return {};
    }
    std::string error;
    auto samples = Detail::DecodeAudio(*bytes, &error);
    if (!samples) {
        Log::Error("Sound {}: {}", path.string(), error);
        return {};
    }
    Sound sound;
    sound.Data = std::make_shared<Detail::SoundData>();
    sound.Data->Samples = std::move(*samples);
    sound.Data->Duration = float(sound.Data->Frames()) / float(Detail::MixRate);
    sound.Data->MaxVoices = std::max(1, maxVoices);
    return sound;
}

Sound Sound::FromMemory(const void* data, std::size_t bytes, int maxVoices) {
    std::string error;
    auto samples = Detail::DecodeAudio({static_cast<const std::uint8_t*>(data), bytes}, &error);
    if (!samples) {
        Log::Error("Sound from memory: {}", error);
        return {};
    }
    Sound sound;
    sound.Data = std::make_shared<Detail::SoundData>();
    sound.Data->Samples = std::move(*samples);
    sound.Data->Duration = float(sound.Data->Frames()) / float(Detail::MixRate);
    sound.Data->MaxVoices = std::max(1, maxVoices);
    return sound;
}

Sound Sound::Synth(float duration, SoundFunction function, int maxVoices) {
    if (!function || duration <= 0) return {};
    Sound sound;
    sound.Data = std::make_shared<Detail::SoundData>();
    sound.Data->Function = std::move(function);
    sound.Data->Duration = duration;
    sound.Data->MaxVoices = std::max(1, maxVoices);
    return sound;
}

Sound Sound::Bake() const {
    if (!Data || !Data->Function) return *this;
    const std::size_t frames = std::size_t(std::ceil(double(Data->Duration) * Detail::MixRate));
    Voice voice;
    voice.Duration = Data->Duration;
    std::vector<float> samples(frames * 2);
    const float dt = 1.0f / float(Detail::MixRate);
    for (std::size_t i = 0; i < frames; ++i) {
        const float s = Data->Function(voice, voice.Time / voice.Duration, dt);
        voice.Time += dt;
        samples[2 * i] = samples[2 * i + 1] = s;
    }
    Sound baked;
    baked.Data = std::make_shared<Detail::SoundData>();
    baked.Data->Samples = std::move(samples);
    baked.Data->Duration = Data->Duration;
    baked.Data->MaxVoices = Data->MaxVoices;
    return baked;
}

void Sound::Play(float volume, float pitch, float pan) const { Detail::PlaySound(Data, volume, pitch, pan); }

void Sound::Stop() const {
    if (Data) Detail::StopSound(Data.get());
}

void Sound::SetMaxVoices(int voices) const {
    if (Data) Data->MaxVoices = std::max(1, voices);
}

int Sound::MaxVoices() const { return Data ? Data->MaxVoices : 0; }

float Sound::Duration() const { return Data ? Data->Duration : 0.0f; }

// ---------------------------------------------------------------- Audio

namespace Audio {

void SetVolume(Bus bus, float volume) { Detail::BusVolumes[std::size_t(bus)] = std::max(0.0f, volume); }

float Volume(Bus bus) { return Detail::BusVolumes[std::size_t(bus)]; }

void SetPaused(bool paused) {
    Detail::Mixer& m = Detail::Mix();
    std::lock_guard lock(m.Lock);
    m.Paused = paused;
}

bool IsPaused() {
    Detail::Mixer& m = Detail::Mix();
    std::lock_guard lock(m.Lock);
    return m.Paused;
}

void StopSounds() {
    Detail::Mixer& m = Detail::Mix();
    std::lock_guard lock(m.Lock);
    for (Detail::Playing& v : m.Voices) Detail::Retire(m, v);
}

bool Available() { return Detail::AudioDeviceOpen(); }

int SampleRate() { return Detail::MixRate; }

} // namespace Audio

// ---------------------------------------------------------------- Music

namespace Music {

bool Play(const std::filesystem::path& path, float fade, bool loop) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    if (extension == ".bitmusic") {
        std::optional<Song> song = Song::Load(path);
        if (!song) return false;
        Play(*song, fade, loop);
        return true;
    }
    auto bytes = Files::ReadBytes(AssetFile(path));
    if (!bytes) {
        Log::Error("Music {}: cannot read the file", path.string());
        return false;
    }
    std::string error;
    auto source = Detail::MakeFileSource(std::move(*bytes), loop, &error);
    if (!source) {
        Log::Error("Music {}: {}", path.string(), error);
        return false;
    }
    Detail::PlayMusic(std::move(source), fade);
    return true;
}

void Play(const Song& song, float fade, bool loop) { Detail::PlayMusic(Detail::MakeSongSource(song, loop), fade); }

void Procedural(MusicFunction function, float fade, double bpm) {
    if (!function) return;
    Detail::PlayMusic(Detail::MakeProceduralSource(std::move(function), bpm), fade);
}

void Stop(float fade) { Detail::PlayMusic(nullptr, fade); }

bool IsPlaying() {
    Detail::Mixer& m = Detail::Mix();
    std::lock_guard lock(m.Lock);
    return m.Current.Source != nullptr;
}

void SetParam(int index, float value) {
    if (index < 0 || index >= 8) {
        Log::Warn("Music::SetParam: index {} out of 0..7", index);
        return;
    }
    Detail::Mixer& m = Detail::Mix();
    std::lock_guard lock(m.Lock);
    m.Params[std::size_t(index)] = value;
}

float Param(int index) {
    if (index < 0 || index >= 8) return 0;
    Detail::Mixer& m = Detail::Mix();
    std::lock_guard lock(m.Lock);
    return m.Params[std::size_t(index)];
}

double Beat() {
    Detail::Mixer& m = Detail::Mix();
    std::lock_guard lock(m.Lock);
    if (!m.Current.Source) return 0;
    return double(m.MusicSamples) / Detail::MixRate * m.Current.Source->Bpm / 60.0;
}

float Pulse() {
    const double beat = Beat();
    if (!IsPlaying()) return 0;
    return float(std::exp(-(beat - std::floor(beat)) * 6.0));
}

void SetTempo(double bpm) {
    if (bpm <= 0) return;
    Detail::Mixer& m = Detail::Mix();
    std::lock_guard lock(m.Lock);
    if (m.Current.Source) m.Current.Source->Bpm = bpm;
}

} // namespace Music

} // namespace BitMotor
