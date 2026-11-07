#include "Printing.h"
#include "Testing.h"

#include "Audio/AudioInternal.h"
#include "Audio/Fixtures.h"

#include <BitMotor/BitMotor.h>

#include <cmath>
#include <cstring>
#include <filesystem>

using namespace BitMotor;
namespace fs = std::filesystem;

namespace {

// Mixes `frames` frames and returns them (stereo, interleaved).
std::vector<float> Mix(int frames) {
    std::vector<float> out(std::size_t(frames) * 2, 0.0f);
    Detail::MixAudio(out.data(), frames);
    return out;
}

float Peak(const std::vector<float>& samples, int channel = -1, std::size_t from = 0) {
    float peak = 0;
    for (std::size_t i = from * 2; i < samples.size(); ++i)
        if (channel < 0 || int(i % 2) == channel) peak = std::max(peak, std::abs(samples[i]));
    return peak;
}

// A 16-bit mono WAV file in memory.
std::vector<std::uint8_t> Wav(int rate, const std::vector<std::int16_t>& samples) {
    std::vector<std::uint8_t> out;
    auto u32 = [&](std::uint32_t v) {
        for (int i = 0; i < 4; ++i) out.push_back(std::uint8_t(v >> (8 * i)));
    };
    auto u16 = [&](std::uint16_t v) {
        out.push_back(std::uint8_t(v));
        out.push_back(std::uint8_t(v >> 8));
    };
    auto tag = [&](const char* t) { out.insert(out.end(), t, t + 4); };
    const std::uint32_t bytes = std::uint32_t(samples.size() * 2);
    tag("RIFF");
    u32(36 + bytes);
    tag("WAVE");
    tag("fmt ");
    u32(16);
    u16(1); // PCM
    u16(1); // mono
    u32(std::uint32_t(rate));
    u32(std::uint32_t(rate * 2));
    u16(2);
    u16(16);
    tag("data");
    u32(bytes);
    for (std::int16_t s : samples) u16(std::uint16_t(s));
    return out;
}

Sound Constant(float value, float duration, int maxVoices = 5) {
    return Sound::Synth(duration, [value](Voice&, float, float) { return value; }, maxVoices);
}

} // namespace

TEST(SoundsMixWithVolumeAndPan) {
    Detail::ResetAudio();
    Sound tone = Constant(0.5f, 0.01f); // 480 samples
    tone.Play();
    std::vector<float> out = Mix(400);
    CHECK_NEAR(out[0], 0.5f, 1e-4f);
    CHECK_NEAR(out[1], 0.5f, 1e-4f);
    CHECK_EQ(Detail::ActiveVoices(), 1);
    Mix(200); // past its end
    CHECK_EQ(Detail::ActiveVoices(), 0);
    CHECK_EQ(Peak(Mix(100)), 0.0f);

    tone.Play(0.5f, 1, -1); // half as loud, all to the left
    out = Mix(100);
    CHECK_NEAR(out[0], 0.25f, 1e-4f);
    CHECK_EQ(out[1], 0.0f);
    Audio::StopSounds();
    CHECK_EQ(Detail::ActiveVoices(), 0);
    Detail::ResetAudio();
}

TEST(SoundsKeepTheirVoiceLimit) {
    Detail::ResetAudio();
    Sound coin = Constant(0.1f, 1, 2);
    for (int i = 0; i < 5; ++i) coin.Play();
    CHECK_EQ(Detail::ActiveVoices(), 2); // the oldest copies made room for the new ones
    Sound other = Constant(0.1f, 1);
    other.Play();
    CHECK_EQ(Detail::ActiveVoices(), 3);
    coin.Stop();
    CHECK_EQ(Detail::ActiveVoices(), 1);
    // More sounds than voices: the oldest of all stops.
    Sound many = Constant(0.01f, 1, 100);
    for (int i = 0; i < Detail::MixVoices + 10; ++i) many.Play();
    CHECK_EQ(Detail::ActiveVoices(), Detail::MixVoices);
    Sound().Play(); // empty: nothing
    Detail::ResetAudio();
    CHECK_EQ(Detail::ActiveVoices(), 0);
}

TEST(SynthesizedSoundsUseTheirVoice) {
    Detail::ResetAudio();
    // A saw that rises once per cycle: the pitch makes it rise faster.
    Sound saw = Sound::Synth(1, [](Voice& v, float, float dt) { return v.Advance(0, 100, dt); });
    saw.Play(1, 2);
    std::vector<float> out = Mix(240); // 240 samples at 200 Hz: one cycle
    CHECK_NEAR(out[2 * 120], 0.5f, 0.01f);
    Detail::ResetAudio();

    Sound baked = Constant(0.25f, 0.5f).Bake();
    CHECK_NEAR(baked.Duration(), 0.5f, 1e-4f);
    CHECK_EQ(baked.Internal()->Frames(), std::size_t(24000));
    CHECK_EQ(baked.Internal()->Samples[100], 0.25f);
    baked.Play(1, 2); // a baked sound plays faster with the pitch
    Mix(12001);
    CHECK_EQ(Detail::ActiveVoices(), 0);
    Detail::ResetAudio();
}

TEST(SoundsDecodeWavFiles) {
    Detail::ResetAudio();
    std::vector<std::uint8_t> wav = Wav(48000, std::vector<std::int16_t>(100, 16384));
    Sound sound = Sound::FromMemory(wav.data(), wav.size());
    CHECK(bool(sound));
    CHECK_EQ(sound.Internal()->Frames(), std::size_t(100));
    CHECK_NEAR(sound.Internal()->Samples[0], 0.5f, 1e-3f);
    CHECK_NEAR(sound.Internal()->Samples[1], 0.5f, 1e-3f); // mono becomes both channels
    // Another sample rate is converted to the mixer's.
    wav = Wav(24000, std::vector<std::int16_t>(100, 8192));
    sound = Sound::FromMemory(wav.data(), wav.size());
    CHECK_EQ(sound.Internal()->Frames(), std::size_t(200));
    CHECK_NEAR(sound.Duration(), 200.0f / 48000, 1e-6f);

    const std::uint8_t junk[16] = {1, 2, 3};
    CHECK(!Sound::FromMemory(junk, sizeof junk));
    fs::path folder = fs::temp_directory_path() / "BitMotorTests" / "Audio";
    fs::create_directories(folder);
    CHECK(Files::WriteBytes(folder / "beep.wav", Wav(48000, std::vector<std::int16_t>(4800, 3000))));
    sound = Sound::Load(folder / "beep.wav");
    CHECK_NEAR(sound.Duration(), 0.1f, 1e-4f);
    CHECK(!Sound::Load(folder / "missing.wav"));
    Detail::ResetAudio();
}

TEST(SoundsDecodeOggFlacAndMp3) {
    struct Case {
        const char* Name;
        const std::uint8_t* Bytes;
        std::size_t Size;
    };
    const Case cases[] = {{"ogg", AudioFixtures::Ogg, sizeof AudioFixtures::Ogg},
                          {"flac", AudioFixtures::Flac, sizeof AudioFixtures::Flac},
                          {"mp3", AudioFixtures::Mp3, sizeof AudioFixtures::Mp3}};
    for (const Case& c : cases) {
        std::string error;
        auto samples = Detail::DecodeAudio({c.Bytes, c.Size}, &error);
        CHECK(samples.has_value());
        if (!samples) {
            Log::Error("{}: {}", c.Name, error);
            continue;
        }
        // 0.1 s at 22050 Hz becomes about 4800 frames at 48000 (MP3 adds a little silence at the start).
        const std::size_t frames = samples->size() / 2;
        CHECK(frames >= 4700 && frames <= 6500);
        float peak = 0;
        for (float v : *samples) peak = std::max(peak, std::abs(v));
        CHECK(peak > 0.08f && peak < 0.2f); // ffmpeg's sine is at 1/8
    }
}

TEST(BusesFollowTheSettings) {
    Detail::ResetAudio();
    Sound tone = Constant(0.5f, 10);
    tone.Play();
    Audio::SetVolume(Bus::Sound, 0.5f);
    CHECK_EQ(Audio::Volume(Bus::Sound), 0.5f);
    Detail::UpdateAudio(0);
    Mix(48000); // the volume glides there in a few milliseconds
    std::vector<float> out = Mix(10);
    CHECK_NEAR(out[0], 0.25f, 1e-3f);
    Audio::SetVolume(Bus::Master, 0);
    Detail::UpdateAudio(0);
    Mix(48000);
    CHECK_NEAR(Mix(10)[0], 0.0f, 1e-4f);
    Detail::ResetAudio();
    CHECK_EQ(Audio::Volume(Bus::Master), 1.0f);
}

TEST(LoudMixesAreSoftened) {
    Detail::ResetAudio();
    Sound loud = Constant(1, 1, 10);
    for (int i = 0; i < 4; ++i) loud.Play();
    std::vector<float> out = Mix(10);
    CHECK(out[0] <= 1.0f);
    CHECK(out[0] > 0.95f);
    Detail::ResetAudio();
    Constant(0.7f, 1).Play();
    CHECK_NEAR(Mix(10)[0], 0.7f, 1e-5f); // quiet ones are untouched
    Detail::ResetAudio();
}

TEST(ProceduralMusicReadsItsParameters) {
    Detail::ResetAudio();
    Music::SetParam(0, 0.3f);
    Music::Procedural([](MusicState& m, float) { return m.Params[0]; }, 0, 120);
    CHECK(Music::IsPlaying());
    CHECK_NEAR(Mix(10)[0], 0.3f, 1e-5f);
    Music::SetParam(0, -0.2f);
    CHECK_EQ(Music::Param(0), -0.2f);
    CHECK_NEAR(Mix(10)[1], -0.2f, 1e-5f);
    Music::SetParam(9, 1); // out of range: ignored
    // 120 beats per minute: two each second.
    Detail::ResetAudio();
    Music::Procedural([](MusicState&, float) { return 0.0f; }, 0, 120);
    Mix(48000);
    CHECK_NEAR(Music::Beat(), 2.0, 1e-9);
    CHECK_NEAR(Music::Pulse(), 1.0f, 1e-5f);
    Mix(12000);
    CHECK(Music::Pulse() < 0.1f);
    Music::SetTempo(60);
    CHECK_NEAR(Music::Beat(), 1.25, 1e-9);
    Music::Stop(0);
    CHECK(!Music::IsPlaying());
    CHECK_EQ(Music::Beat(), 0.0);
    CHECK_EQ(Peak(Mix(10)), 0.0f);
    Detail::ResetAudio();
}

TEST(MusicCrossfades) {
    Detail::ResetAudio();
    Music::Procedural([](MusicState&, float) { return 0.4f; }, 0);
    Mix(10);
    Music::Procedural([](MusicState&, float) { return -0.4f; }, 1); // one second
    std::vector<float> out = Mix(24000);
    CHECK_NEAR(out.back(), 0.0f, 1e-3f); // half way: one as loud as the other
    Mix(24000);
    CHECK_NEAR(Mix(10)[0], -0.4f, 1e-4f);
    Music::Stop(0.5f);
    CHECK(!Music::IsPlaying());
    Mix(24001);
    CHECK_EQ(Peak(Mix(10)), 0.0f);
    Detail::UpdateAudio(0); // what faded out is freed here
    Detail::ResetAudio();
}

TEST(PauseMuffles) {
    Detail::ResetAudio();
    // The highest note there is: +0.5, -0.5, +0.5...
    Music::Procedural([](MusicState& m, float) { return m.Sample % 2 ? -0.5f : 0.5f; }, 0);
    CHECK(Peak(Mix(1000)) > 0.45f);
    Audio::SetPaused(true);
    CHECK(Audio::IsPaused());
    Mix(48000);
    CHECK(Peak(Mix(1000)) < 0.05f);
    // Sound effects are not muffled.
    Constant(0.3f, 1).Play();
    CHECK(Peak(Mix(100)) > 0.25f);
    Audio::SetPaused(false);
    Detail::ResetAudio();
}

TEST(MusicPlaysFiles) {
    Detail::ResetAudio();
    fs::path folder = fs::temp_directory_path() / "BitMotorTests" / "Audio";
    fs::create_directories(folder);
    CHECK(Files::WriteBytes(folder / "loop.wav", Wav(48000, std::vector<std::int16_t>(1000, 16384))));
    CHECK(Music::Play(folder / "loop.wav", 0));
    std::vector<float> out = Mix(3000); // three times through
    CHECK_NEAR(out[2 * 2500], 0.5f, 1e-3f);
    CHECK(Music::IsPlaying());
    CHECK(Music::Play(folder / "loop.wav", 0, false)); // once
    Mix(1500);
    CHECK(!Music::IsPlaying());
    CHECK(!Music::Play(folder / "missing.ogg"));
    CHECK(Files::Write(folder / "bad.mp3", "not music"));
    CHECK(!Music::Play(folder / "bad.mp3"));
    Detail::ResetAudio();
}

TEST(WithoutADeviceTimeStillPasses) {
    Detail::ResetAudio();
    Constant(0.1f, 0.5f).Play();
    Music::Procedural([](MusicState&, float) { return 0.0f; }, 0, 120);
    for (int i = 0; i < 60; ++i) Detail::UpdateAudio(1.0 / 60);
    CHECK_NEAR(Music::Beat(), 2.0, 0.01);
    CHECK_EQ(Detail::ActiveVoices(), 0);
    CHECK(!Audio::Available());
    CHECK_EQ(Audio::SampleRate(), 48000);
    Detail::ResetAudio();
}
