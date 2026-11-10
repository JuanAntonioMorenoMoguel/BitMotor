// TrueType and OpenType fonts, rasterised once at one size with stb_truetype into a texture of letters.
#include "BitMotor/Core/Files.h"
#include "BitMotor/Core/Log.h"
#include "Text/FontInternal.h"

#include <algorithm>
#include <cmath>

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244 4245 4456 4457 4701)
#endif
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC // private to this file: Dear ImGui carries its own copy
#include <stb_truetype.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

namespace BitMotor {

namespace Detail {

namespace {

constexpr int AtlasWidth = 512;

// What every TTF font carries: ASCII, Latin-1, Latin Extended-A and the usual typography.
std::vector<char32_t> CharacterSet(std::u32string_view extra) {
    std::vector<char32_t> set;
    for (char32_t c = 0x21; c <= 0x7E; ++c) set.push_back(c);
    for (char32_t c = 0xA1; c <= 0x17F; ++c) set.push_back(c);
    for (char32_t c : std::u32string_view(U"‐‒–—―‘’‚“”„†‡•…‰‹›€™−")) set.push_back(c);
    for (char32_t c : extra)
        if (c > 0x20) set.push_back(c);
    std::sort(set.begin(), set.end());
    set.erase(std::unique(set.begin(), set.end()), set.end());
    return set;
}

} // namespace

bool BakeTtf(std::span<const std::uint8_t> bytes, const TtfOptions& options, BakedFont& out, std::string& error) {
    out = {};
    if (bytes.size() < 12) {
        error = "not a font file";
        return false;
    }
    const int offset = stbtt_GetFontOffsetForIndex(bytes.data(), 0);
    stbtt_fontinfo font;
    if (offset < 0 || !stbtt_InitFont(&font, bytes.data(), offset)) {
        error = "not a TrueType or OpenType font stb_truetype can read";
        return false;
    }
    const float size = std::max(options.Size, 1.0f);
    const float scale = stbtt_ScaleForPixelHeight(&font, size);
    int ascent = 0, descent = 0, gap = 0;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &gap);

    // The capitals: where the top of 'H' is (from the baseline) and how tall it is.
    float capTop = -std::round(float(ascent) * scale * 0.7f), cap = -capTop;
    if (stbtt_FindGlyphIndex(&font, 'H') != 0) {
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        stbtt_GetCodepointBitmapBox(&font, 'H', scale, scale, &x0, &y0, &x1, &y1);
        if (y1 > y0) {
            capTop = float(y0);
            cap = float(y1 - y0);
        }
    }
    out.CapHeight = std::max(cap, 1.0f);
    out.LineHeight = std::max(std::round(float(ascent - descent + gap) * scale), out.CapHeight + 1);
    int spaceAdvance = 0, spaceBearing = 0;
    stbtt_GetCodepointHMetrics(&font, ' ', &spaceAdvance, &spaceBearing);
    out.Space = std::max(std::round(float(spaceAdvance) * scale), 1.0f);

    struct Pending {
        char32_t Character;
        int X0, Y0, Width, Height;
        float Advance;
        std::vector<std::uint8_t> Coverage;
    };
    std::vector<Pending> pending;
    for (char32_t c : CharacterSet(options.Extra)) {
        const int index = stbtt_FindGlyphIndex(&font, int(c));
        if (index == 0) continue; // the font does not have it: Text falls back to a close one
        int advance = 0, bearing = 0;
        stbtt_GetGlyphHMetrics(&font, index, &advance, &bearing);
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        stbtt_GetGlyphBitmapBox(&font, index, scale, scale, &x0, &y0, &x1, &y1);
        Pending glyph{c, x0, y0, std::max(x1 - x0, 0), std::max(y1 - y0, 0), std::round(float(advance) * scale), {}};
        if (glyph.Width > 0 && glyph.Height > 0) {
            glyph.Coverage.resize(std::size_t(glyph.Width) * std::size_t(glyph.Height));
            stbtt_MakeGlyphBitmap(&font, glyph.Coverage.data(), glyph.Width, glyph.Height, glyph.Width, scale, scale,
                                  index);
        }
        pending.push_back(std::move(glyph));
    }
    if (pending.empty()) {
        error = "the font has none of the characters BitMotor draws";
        return false;
    }

    // Shelves, tallest letters first, one pixel apart. A letter with no pixels keeps an empty area.
    std::vector<std::size_t> order(pending.size());
    for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::stable_sort(order.begin(), order.end(),
                     [&](std::size_t a, std::size_t b) { return pending[a].Height > pending[b].Height; });
    std::vector<std::pair<int, int>> places(pending.size(), {0, 0});
    int x = 1, y = 1, shelf = 0;
    for (std::size_t i : order) {
        const Pending& glyph = pending[i];
        if (glyph.Width == 0 || glyph.Height == 0) continue;
        if (x + glyph.Width + 1 > AtlasWidth) {
            x = 1;
            y += shelf + 1;
            shelf = 0;
        }
        places[i] = {x, y};
        x += glyph.Width + 1;
        shelf = std::max(shelf, glyph.Height);
    }
    out.Width = AtlasWidth;
    out.Height = std::max(y + shelf + 1, 2);
    out.Pixels.assign(std::size_t(out.Width) * std::size_t(out.Height) * 4, 0);
    for (std::size_t i = 0; i < pending.size(); ++i) {
        const Pending& glyph = pending[i];
        BakedGlyph baked;
        baked.Character = glyph.Character;
        baked.Advance = glyph.Advance;
        // From the pen (top-left of the capitals) to the image: the box is measured from the baseline.
        baked.Offset = {float(glyph.X0), float(glyph.Y0) - capTop};
        if (glyph.Width > 0 && glyph.Height > 0) {
            const auto [px, py] = places[i];
            baked.Area = Rect(float(px), float(py), float(glyph.Width), float(glyph.Height));
            for (int gy = 0; gy < glyph.Height; ++gy)
                for (int gx = 0; gx < glyph.Width; ++gx) {
                    std::uint8_t coverage = glyph.Coverage[std::size_t(gy * glyph.Width + gx)];
                    if (!options.Smooth) coverage = coverage >= 128 ? 255 : 0;
                    std::size_t at = (std::size_t(py + gy) * std::size_t(out.Width) + std::size_t(px + gx)) * 4;
                    out.Pixels[at] = out.Pixels[at + 1] = out.Pixels[at + 2] = 255;
                    out.Pixels[at + 3] = coverage;
                }
        }
        out.Glyphs.push_back(baked);
    }
    return true;
}

} // namespace Detail

Font Font::FromTtf(std::span<const std::uint8_t> bytes, const TtfOptions& options) {
    Detail::BakedFont baked;
    std::string error;
    if (!Detail::BakeTtf(bytes, options, baked, error)) {
        Log::Error("Font::FromTtf: {}", error);
        return {};
    }
    Texture texture = Texture::FromPixels(baked.Width, baked.Height, baked.Pixels);
    if (!texture) return {};
    auto data = std::make_shared<Detail::FontData>();
    data->CapHeight = baked.CapHeight;
    data->LineHeight = baked.LineHeight;
    data->Space = baked.Space;
    for (const Detail::BakedGlyph& glyph : baked.Glyphs) {
        Detail::FontGlyph entry;
        entry.Image = Region{texture, glyph.Area};
        entry.Offset = glyph.Offset;
        entry.Advance = glyph.Advance;
        data->Glyphs[glyph.Character] = entry;
    }
    Font font;
    font.Data = std::move(data);
    return font;
}

Font Font::Load(const std::filesystem::path& path, const TtfOptions& options) {
    auto bytes = Files::ReadBytes(path);
    if (!bytes) {
        Log::Error("Font::Load: cannot read {}", path.string());
        return {};
    }
    Font font = FromTtf(*bytes, options);
    if (!font) Log::Error("Font::Load: {} is not a font BitMotor can read", path.string());
    return font;
}

} // namespace BitMotor
