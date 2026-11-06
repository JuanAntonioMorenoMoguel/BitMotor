#pragma once
#include "BitMotor/Math/Vector.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>

// An image the GPU can draw. Copies are cheap and share the same image; it is freed with the last copy.
//     Texture hero = Texture::Load("hero.png");              // relative paths: Files::AssetPath
//     Texture smooth = Texture::Load("title.png", Filter::Linear);
//     if (!hero) Log::Error("no hero");                       // a failed load logs why and stays empty
// Loading works before the window opens: the image goes to the GPU as soon as there is one.
namespace BitMotor {

// How the texture looks when scaled: Nearest keeps hard pixels (pixel art), Linear smooths them.
enum class Filter { Nearest, Linear };

namespace Detail {
struct TextureData;
}

class Texture {
public:
    Texture() = default;

    // PNG, JPG, BMP or TGA. An empty texture (and a log error) if it cannot be read.
    static Texture Load(const std::filesystem::path& path, Filter filter = Filter::Nearest);
    // Straight-alpha RGBA bytes, row by row from the top (width * height * 4 of them).
    static Texture FromPixels(int width, int height, std::span<const std::uint8_t> rgba,
                              Filter filter = Filter::Nearest);

    int Width() const;
    int Height() const;
    Vec2 Size() const { return {float(Width()), float(Height())}; }
    Filter GetFilter() const;

    // False for an empty texture (never loaded, or the load failed).
    explicit operator bool() const { return Data != nullptr; }
    bool operator==(const Texture& other) const { return Data == other.Data; }

    // Internal: the image behind this texture.
    Detail::TextureData* Internal() const { return Data.get(); }

private:
    std::shared_ptr<Detail::TextureData> Data;
};

} // namespace BitMotor
