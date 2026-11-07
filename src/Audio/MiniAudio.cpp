// The audio device and the decoders, with miniaudio (WAV, FLAC, MP3) and stb_vorbis (OGG).
#include "Audio/AudioInternal.h"

#include "BitMotor/Core/Log.h"

#include <algorithm>
#include <cstring>

// Third-party code: its own warnings are not BitMotor's.
#if defined(_MSC_VER)
#pragma warning(push, 0)
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif
// stb_vorbis first, as a header, so that miniaudio decodes OGG with it; then its implementation.
// vcpkg installs it as stb_vorbis.c, Linux distributions as stb_vorbis.h.
#define STB_VORBIS_HEADER_ONLY
#if __has_include(<stb_vorbis.c>)
#include <stb_vorbis.c>
#define BITMOTOR_STB_VORBIS <stb_vorbis.c>
#else
#include <stb_vorbis.h>
#define BITMOTOR_STB_VORBIS <stb_vorbis.h>
#endif
#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_ENGINE
#define MA_NO_NODE_GRAPH
#define MA_NO_RESOURCE_MANAGER
#include <miniaudio.h>
#undef STB_VORBIS_HEADER_ONLY
#include BITMOTOR_STB_VORBIS
#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace BitMotor::Detail {

namespace {

ma_decoder_config DecoderConfig() { return ma_decoder_config_init(ma_format_f32, 2, MixRate); }

std::string ResultText(ma_result result) { return ma_result_description(result); }

std::string DecodeError(ma_result result) {
    return result == MA_INVALID_FILE || result == MA_NO_BACKEND || result == MA_ERROR
               ? "not a WAV, FLAC, MP3 or OGG file"
               : "cannot decode it (" + ResultText(result) + ")";
}

// A file streamed as it plays. It owns the encoded bytes: the decoder reads from them.
struct FileSource final : MusicSource {
    std::vector<std::uint8_t> Bytes;
    ma_decoder Decoder{};
    bool Open = false, Loop = true;

    ~FileSource() override {
        if (Open) ma_decoder_uninit(&Decoder);
    }

    bool Render(float* out, int frames) override {
        float buffer[2 * 512];
        int done = 0;
        bool rewound = false;
        while (done < frames) {
            const int chunk = std::min(frames - done, 512);
            ma_uint64 read = 0;
            ma_decoder_read_pcm_frames(&Decoder, buffer, ma_uint64(chunk), &read);
            for (ma_uint64 i = 0; i < read * 2; ++i) out[std::size_t(done) * 2 + i] += buffer[i];
            done += int(read);
            if (read < ma_uint64(chunk)) {
                // The end: back to the start, unless it plays once (or is empty and would loop forever).
                if (!Loop || (read == 0 && rewound)) return false;
                ma_decoder_seek_to_pcm_frame(&Decoder, 0);
                rewound = read == 0;
            } else {
                rewound = false;
            }
        }
        return true;
    }
};

ma_device Device{};
bool DeviceOpen = false;

void DeviceCallback(ma_device*, void* output, const void*, ma_uint32 frames) {
    auto* out = static_cast<float*>(output);
    std::memset(out, 0, sizeof(float) * 2 * frames);
    MixAudio(out, int(frames));
}

} // namespace

std::optional<std::vector<float>> DecodeAudio(std::span<const std::uint8_t> bytes, std::string* error) {
    ma_decoder_config config = DecoderConfig();
    ma_uint64 frames = 0;
    void* samples = nullptr;
    ma_result result = ma_decode_memory(bytes.data(), bytes.size(), &config, &frames, &samples);
    if (result != MA_SUCCESS) {
        if (error) *error = DecodeError(result);
        return std::nullopt;
    }
    const float* begin = static_cast<const float*>(samples);
    std::vector<float> decoded(begin, begin + frames * 2);
    ma_free(samples, nullptr);
    return decoded;
}

std::unique_ptr<MusicSource> MakeFileSource(std::vector<std::uint8_t> bytes, bool loop, std::string* error) {
    auto source = std::make_unique<FileSource>();
    source->Bytes = std::move(bytes);
    source->Loop = loop;
    ma_decoder_config config = DecoderConfig();
    ma_result result = ma_decoder_init_memory(source->Bytes.data(), source->Bytes.size(), &config, &source->Decoder);
    if (result != MA_SUCCESS) {
        if (error) *error = DecodeError(result);
        return nullptr;
    }
    source->Open = true;
    return source;
}

bool StartAudio() {
    if (DeviceOpen) return true;
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate = MixRate;
    config.dataCallback = DeviceCallback;
    config.periodSizeInMilliseconds = 10;
    ma_result result = ma_device_init(nullptr, &config, &Device);
    if (result == MA_SUCCESS) {
        result = ma_device_start(&Device);
        if (result != MA_SUCCESS) ma_device_uninit(&Device);
    }
    if (result != MA_SUCCESS) {
        Log::Warn("No audio device ({}): the game plays in silence", ResultText(result));
        return false;
    }
    DeviceOpen = true;
    return true;
}

void StopAudio() {
    if (!DeviceOpen) return;
    ma_device_uninit(&Device); // stops it and waits for the audio thread
    DeviceOpen = false;
}

bool AudioDeviceOpen() { return DeviceOpen; }

} // namespace BitMotor::Detail
