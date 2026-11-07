#include "BitMotor/Core/Log.h"
#include "BitMotor/Render/Atlas.h"
#include "Text/FontInternal.h"

#include <algorithm>
#include <string>

namespace BitMotor {

namespace Detail {

namespace {

constexpr int Columns = 16, CellWidth = 5, CellHeight = 11;

// The letter without its accent (lower and upper case kept); itself if it has none.
char32_t Plain(char32_t c) {
    switch (c) {
    case U'à':
    case U'á':
    case U'â':
    case U'ã':
    case U'ä':
    case U'å':
        return U'a';
    case U'è':
    case U'é':
    case U'ê':
    case U'ë':
        return U'e';
    case U'ì':
    case U'í':
    case U'î':
    case U'ï':
        return U'i';
    case U'ò':
    case U'ó':
    case U'ô':
    case U'õ':
    case U'ö':
        return U'o';
    case U'ù':
    case U'ú':
    case U'û':
    case U'ü':
        return U'u';
    case U'ñ':
        return U'n';
    case U'ç':
        return U'c';
    case U'ý':
    case U'ÿ':
        return U'y';
    case U'À':
    case U'Á':
    case U'Â':
    case U'Ã':
    case U'Ä':
    case U'Å':
        return U'A';
    case U'È':
    case U'É':
    case U'Ê':
    case U'Ë':
        return U'E';
    case U'Ì':
    case U'Í':
    case U'Î':
    case U'Ï':
        return U'I';
    case U'Ò':
    case U'Ó':
    case U'Ô':
    case U'Õ':
    case U'Ö':
        return U'O';
    case U'Ù':
    case U'Ú':
    case U'Û':
    case U'Ü':
        return U'U';
    case U'Ñ':
        return U'N';
    case U'Ç':
        return U'C';
    case U'Ý':
        return U'Y';
    default:
        return c;
    }
}

char32_t Upper(char32_t c) {
    if (c >= U'a' && c <= U'z') return c - 32;
    if ((c >= 0xE0 && c <= 0xFE && c != 0xF7)) return c - 32; // Latin-1 lower case letters
    return c;
}

// Typography a small font rarely has.
char32_t Punctuation(char32_t c) {
    switch (c) {
    case U'—':
    case U'–':
    case U'‒':
    case U'−':
        return U'-';
    case U'«':
    case U'»':
    case U'“':
    case U'”':
    case U'„':
        return U'"';
    case U'‘':
    case U'’':
    case U'‚':
        return U'\'';
    case U'…':
        return U'.';
    case U' ':
        return U' ';
    default:
        return c;
    }
}

} // namespace

char32_t Fallback(char32_t c) {
    if (char32_t p = Punctuation(c); p != c) return p;
    if (char32_t p = Plain(c); p != c) return p;
    return Upper(c);
}

const FontGlyph* FontData::Find(char32_t c) const {
    for (char32_t candidate : {c, Plain(c), Upper(c), Upper(Plain(c)), Punctuation(c)})
        if (auto it = Glyphs.find(candidate); it != Glyphs.end()) return &it->second;
    return nullptr;
}

std::u32string Decode(std::string_view text) {
    std::u32string out;
    out.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size()) {
        auto byte = static_cast<unsigned char>(text[i]);
        std::size_t extra = 0;
        char32_t c = byte;
        if (byte >= 0x80) {
            if ((byte >> 5) == 0x6) {
                extra = 1;
                c = byte & 0x1F;
            } else if ((byte >> 4) == 0xE) {
                extra = 2;
                c = byte & 0x0F;
            } else if ((byte >> 3) == 0x1E) {
                extra = 3;
                c = byte & 0x07;
            } else {
                extra = 99; // a continuation byte or garbage
            }
        }
        bool ok = extra == 0 || (extra < 4 && i + extra < text.size());
        for (std::size_t k = 1; ok && k <= extra; ++k) {
            auto next = static_cast<unsigned char>(text[i + k]);
            ok = (next & 0xC0) == 0x80;
            c = (c << 6) | (next & 0x3F);
        }
        if (!ok) {
            out.push_back(U'\uFFFD');
            ++i;
            continue;
        }
        out.push_back(c);
        i += extra + 1;
    }
    return out;
}

std::string Encode(std::u32string_view text) {
    std::string out;
    out.reserve(text.size());
    for (char32_t c : text) {
        if (c < 0x80) {
            out.push_back(char(c));
        } else if (c < 0x800) {
            out.push_back(char(0xC0 | (c >> 6)));
            out.push_back(char(0x80 | (c & 0x3F)));
        } else if (c < 0x10000) {
            out.push_back(char(0xE0 | (c >> 12)));
            out.push_back(char(0x80 | ((c >> 6) & 0x3F)));
            out.push_back(char(0x80 | (c & 0x3F)));
        } else {
            out.push_back(char(0xF0 | (c >> 18)));
            out.push_back(char(0x80 | ((c >> 12) & 0x3F)));
            out.push_back(char(0x80 | ((c >> 6) & 0x3F)));
            out.push_back(char(0x80 | (c & 0x3F)));
        }
    }
    return out;
}

namespace {

struct PixelFontData {
    std::shared_ptr<FontData> Data;
    Region White;
};

// The built-in font's texture: the glyphs in a grid (one pixel apart) and a 3x3 white block below them.
const PixelFontData& BuiltIn() {
    static const PixelFontData font = [] {
        const auto& glyphs = PixelGlyphs();
        const int rows = int((glyphs.size() + Columns - 1) / Columns);
        const int width = Columns * (CellWidth + 1), height = rows * (CellHeight + 1) + 3;
        std::vector<std::uint8_t> pixels(std::size_t(width * height * 4), 0);
        auto set = [&](int x, int y) {
            std::size_t at = std::size_t((y * width + x) * 4);
            pixels[at] = pixels[at + 1] = pixels[at + 2] = pixels[at + 3] = 255;
        };
        for (std::size_t i = 0; i < glyphs.size(); ++i) {
            int cx = int(i % Columns) * (CellWidth + 1), cy = int(i / Columns) * (CellHeight + 1);
            for (int y = 0; y < CellHeight; ++y)
                for (int x = 0; x < CellWidth; ++x)
                    if (glyphs[i].Rows[std::size_t(y)] & (0x10 >> x)) set(cx + x, cy + y);
        }
        for (int y = 0; y < 3; ++y)
            for (int x = 0; x < 3; ++x) set(x, rows * (CellHeight + 1) + y);
        Texture texture = Texture::FromPixels(width, height, pixels);

        PixelFontData result;
        result.Data = std::make_shared<FontData>();
        result.Data->CapHeight = 7;
        result.Data->LineHeight = 11;
        result.Data->Space = 6;
        for (std::size_t i = 0; i < glyphs.size(); ++i) {
            FontGlyph glyph;
            float cx = float(int(i % Columns) * (CellWidth + 1)), cy = float(int(i / Columns) * (CellHeight + 1));
            glyph.Image = Region{texture, Rect(cx, cy, CellWidth, CellHeight)};
            glyph.Offset = {0, -2};
            glyph.Advance = 6;
            result.Data->Glyphs[glyphs[i].Character] = glyph;
        }
        result.White = Region{texture, Rect(1, float(rows * (CellHeight + 1) + 1), 1, 1)};
        return result;
    }();
    return font;
}

} // namespace

const Region& WhitePixel() { return BuiltIn().White; }

} // namespace Detail

Font Font::Pixel() {
    Font font;
    font.Data = Detail::BuiltIn().Data;
    return font;
}

Font Font::FromAtlas(const Atlas& atlas, const AtlasFontLayout& layout) {
    auto data = std::make_shared<Detail::FontData>();
    data->CapHeight = layout.CapHeight;
    data->LineHeight = layout.LineHeight;
    for (const std::string& name : atlas.Names()) {
        if (!name.starts_with(layout.Prefix)) continue;
        std::string_view code = std::string_view(name).substr(layout.Prefix.size());
        if (code.empty() || !std::all_of(code.begin(), code.end(), [](char c) { return c >= '0' && c <= '9'; }))
            continue;
        Detail::FontGlyph glyph;
        glyph.Image = atlas.Sprite(name);
        glyph.Offset = {0, -layout.Top};
        glyph.Advance = layout.Advance > 0 ? layout.Advance : glyph.Image.Size().X + 1;
        data->Glyphs[char32_t(std::stoul(std::string(code)))] = glyph;
    }
    if (data->Glyphs.empty()) {
        Log::Warn("Font::FromAtlas: the atlas has no sprites called {}<code>", layout.Prefix);
        return {};
    }
    data->Space = layout.Space > 0 ? layout.Space : layout.Advance > 0 ? layout.Advance : 4;
    Font font;
    font.Data = std::move(data);
    return font;
}

bool Font::Has(char32_t character) const {
    const Detail::FontData* data = Data ? Data.get() : Pixel().Data.get();
    return character == U' ' || data->Find(character) != nullptr;
}

float Font::CapHeight() const { return Data ? Data->CapHeight : Pixel().CapHeight(); }
float Font::LineHeight() const { return Data ? Data->LineHeight : Pixel().LineHeight(); }

} // namespace BitMotor
