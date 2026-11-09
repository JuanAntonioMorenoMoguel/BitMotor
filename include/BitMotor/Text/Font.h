#pragma once
#include "BitMotor/Render/Texture.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>

// The letters text is drawn with. BitMotor carries one pixel font (Font::Pixel: 5x7 capitals, lower case,
// Spanish and other western accents, ¿ ¡ ñ ç € …); a game can use the font of its own atlas instead, made of
// one sprite per character called f_<codepoint> (f_65 is 'A'), as nyiengisprite exports it, or a TrueType
// or OpenType file (.ttf, .otf) drawn at one size.
//     Font pixel = Font::Pixel();
//     Font mine = Font::FromAtlas(atlas);                         // f_65, f_66...
//     Font body = Font::Load("fonts/Inter.ttf", {.Size = 12});    // smooth letters 12 pixels tall
//     Font retro = Font::Load("fonts/PressStart2P.ttf", {.Size = 8, .Smooth = false});
//     Text::Draw("Hola", {8, 8}, {.Font = mine});
// A character the font lacks falls back to a close one (lower case to capitals, accented letters to the plain
// letter, typographic dashes and quotes to ASCII); if there is none, it is skipped.
namespace BitMotor {

class Atlas;

namespace Detail {
struct FontData;
}

// How an atlas font is laid out, in pixels at Size 1. The defaults match nyiengisprite's 5x7 fonts.
struct AtlasFontLayout {
    // Sprite names: Prefix + the character's code in decimal.
    std::string Prefix = "f_";
    // Distance from one character to the next. 0: each sprite's own width plus 1 (proportional fonts).
    float Advance = 6;
    // Rows above the capitals in each sprite (room for accents): the sprite is drawn that much higher.
    float Top = 2;
    // Height of the capitals, and distance from one line to the next.
    float CapHeight = 7;
    float LineHeight = 11;
    // Width of a space (0: the same as Advance, or 4 in a proportional font).
    float Space = 0;
};

// How a TrueType font is drawn: it is rasterised once, at Size pixels, into a texture of its letters.
struct TtfOptions {
    // The font's size in pixels (what a word processor calls the size in points at 72 dpi). TextStyle::Size
    // then enlarges it in whole steps, blocky: load it at the size you will draw it.
    float Size = 16;
    // Soft edges. false: every pixel is on or off, for pixel fonts (load them at their design size).
    bool Smooth = true;
    // Characters to rasterise besides ASCII, Latin-1, Latin Extended-A and typographic punctuation
    // (for example Greek or Cyrillic letters). The ones the font lacks are skipped.
    std::u32string_view Extra = {};
};

class Font {
public:
    // An empty font: Text uses Font::Default() for it.
    Font() = default;

    // What an empty font draws with: Font::Pixel() unless the game sets its own, so every TextStyle (and
    // the UI) uses it without naming it.
    //     Font::SetDefault(Font::FromAtlas(atlas));
    static void SetDefault(const Font& font);
    static Font Default();

    // The built-in pixel font. Always available, also before the window opens.
    static Font Pixel();
    // A TrueType or OpenType font (relative paths: Files::AssetPath). Empty (and a log error) if it cannot be read.
    static Font Load(const std::filesystem::path& path, const TtfOptions& options = {});
    // The same from the file's bytes (a font embedded in the executable).
    static Font FromTtf(std::span<const std::uint8_t> bytes, const TtfOptions& options = {});
    // A font from the sprites of an atlas. Empty (and a log warning) if the atlas has none of them.
    static Font FromAtlas(const Atlas& atlas, const AtlasFontLayout& layout = {});

    // True if the font can draw the character (directly or through its fallback).
    bool Has(char32_t character) const;
    // Pixels at Size 1.
    float CapHeight() const;
    float LineHeight() const;

    explicit operator bool() const { return Data != nullptr; }
    bool operator==(const Font& other) const { return Data == other.Data; }

    // Internal: the glyphs behind this font.
    const Detail::FontData* Internal() const { return Data.get(); }

private:
    std::shared_ptr<const Detail::FontData> Data;
};

} // namespace BitMotor
