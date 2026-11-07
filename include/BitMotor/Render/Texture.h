#pragma once
#include "BitMotor/Math/Shapes.h"
#include "BitMotor/Math/Vector.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>

// An image the GPU can draw. Copies are cheap and share the same image; it is freed with the last copy.
//     Texture hero = Texture::Load("hero.png");              // relative paths: Files::AssetPath
//     Texture smooth = Texture::Load("title.png", Filter::Linear);
//     if (!hero) Log::Error("no hero");                       // a failed load logs why and stays empty
//     Screen::Draw(Sprite2D{.Region = hero.Full(), .Position = {10, 20}});
// Loading works before the window opens: the image goes to the GPU as soon as there is one.
namespace BitMotor {

// How the texture looks when scaled: Nearest keeps hard pixels (pixel art), Linear smooths them.
enum class Filter { Nearest, Linear };

namespace Detail {
struct TextureData;
}

struct Region;

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
    // The whole image as a Region, with its pivot at `pivot` (in pixels; the top-left corner by default).
    Region Full(Vec2 pivot = {0, 0}) const;

    // False for an empty texture (never loaded, or the load failed).
    explicit operator bool() const { return Data != nullptr; }
    bool operator==(const Texture& other) const { return Data == other.Data; }

    // Internal: the image behind this texture.
    Detail::TextureData* Internal() const { return Data.get(); }

private:
    std::shared_ptr<Detail::TextureData> Data;
};

// A piece of a texture to draw: the rectangle `Area` in the texture's pixels, and the `Pivot` (inside
// that rectangle, in its pixels) that sits on Sprite2D::Position and turns with Rotation. Atlas::Sprite
// and Animation::Frame give them; Texture::Full covers a whole image.
struct Region {
    Texture Source = {};
    Rect Area = {};
    Vec2 Pivot = {0, 0};
    // Texture pixels per screen pixel: 2 draws a sprite made at double detail at its normal size.
    float Density = 1;
    // Which sprite of its Atlas this is (for Atlas::Socket); -1 if it is not from an atlas.
    int Id = -1;

    // Size on screen at Scale 1.
    Vec2 Size() const { return Area.Size() * (1.0f / (Density > 0 ? Density : 1.0f)); }
    // False when there is nothing to draw (no texture or an empty rectangle).
    explicit operator bool() const { return bool(Source) && Area.Width > 0 && Area.Height > 0; }
};

inline Region Texture::Full(Vec2 pivot) const {
    return Region{*this, Rect(0, 0, float(Width()), float(Height())), pivot};
}

} // namespace BitMotor
