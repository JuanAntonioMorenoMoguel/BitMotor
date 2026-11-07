#pragma once
#include "BitMotor/Render/Texture.h"
#include "BitMotor/Text/Font.h"

#include <array>
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace BitMotor::Detail {

// One character of the built-in font: 11 rows of 5 pixels (PixelFont.cpp).
struct PixelGlyph {
    char32_t Character;
    std::array<std::uint8_t, 11> Rows;
};
const std::vector<PixelGlyph>& PixelGlyphs();

struct FontGlyph {
    Region Image;
    // From the pen (top-left of the capitals, at Size 1) to the image's top-left corner.
    Vec2 Offset = {0, 0};
    float Advance = 6;
};

struct FontData {
    std::unordered_map<char32_t, FontGlyph> Glyphs;
    float CapHeight = 7, LineHeight = 11, Space = 6;
    // Looks the character up, then its fallback. nullptr if neither is there.
    const FontGlyph* Find(char32_t character) const;
};

// The character a font without it can use instead (lower case -> capital, á -> a or A, — -> -); itself if none.
char32_t Fallback(char32_t character);

// A white pixel inside the built-in font's texture: shapes use it, so text and shapes share one batch.
const Region& WhitePixel();

// UTF-8 -> code points. Invalid bytes become U+FFFD.
std::u32string Decode(std::string_view text);
std::string Encode(std::u32string_view text);

} // namespace BitMotor::Detail
