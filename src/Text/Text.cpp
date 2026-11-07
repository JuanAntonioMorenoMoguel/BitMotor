#include "BitMotor/Text/Text.h"

#include "BitMotor/Colors/Colors.h"
#include "BitMotor/Core/Settings.h"
#include "BitMotor/Core/Time.h"
#include "BitMotor/Math/Functions.h"
#include "BitMotor/Math/Noise.h"
#include "Text/FontInternal.h"

#include <cmath>
#include <utility>

namespace BitMotor {

namespace {

enum Effect : std::uint8_t { None = 0, Wave = 1, Shake = 2 };

// One character with what the markup said about it.
struct Letter {
    char32_t Character = 0;
    BitMotor::Color Color = Colors::White;
    std::uint8_t Effects = None;
};

const Detail::FontData& FontOf(const TextStyle& style) {
    return style.Font ? *style.Font.Internal() : *Font::Pixel().Internal();
}

// The style's Size after the player's TextScale, in whole steps.
float ScaleOf(const TextStyle& style) {
    float scale = float(Max(style.Size, 1)) * Settings::Get().TextScale;
    return Max(1.0f, std::round(scale));
}

float LineStep(const TextStyle& style, const Detail::FontData& font) {
    return style.LineHeight > 0 ? style.LineHeight : font.LineHeight;
}

std::vector<Letter> PlainLetters(std::string_view text, Color color) {
    std::vector<Letter> letters;
    for (char32_t c : Detail::Decode(text)) letters.push_back({c, color, None});
    return letters;
}

struct NamedColor {
    const char* Name;
    Color Value;
};

// Every Colors:: name, lower case.
constexpr NamedColor Named[] = {
    {"white", Colors::White},
    {"black", Colors::Black},
    {"gray", Colors::Gray},
    {"grey", Colors::Gray},
    {"lightgray", Colors::LightGray},
    {"darkgray", Colors::DarkGray},
    {"slate", Colors::Slate},
    {"red", Colors::Red},
    {"darkred", Colors::DarkRed},
    {"crimson", Colors::Crimson},
    {"maroon", Colors::Maroon},
    {"coral", Colors::Coral},
    {"salmon", Colors::Salmon},
    {"orange", Colors::Orange},
    {"amber", Colors::Amber},
    {"gold", Colors::Gold},
    {"yellow", Colors::Yellow},
    {"lime", Colors::Lime},
    {"green", Colors::Green},
    {"darkgreen", Colors::DarkGreen},
    {"olive", Colors::Olive},
    {"mint", Colors::Mint},
    {"phosphor", Colors::Phosphor},
    {"teal", Colors::Teal},
    {"turquoise", Colors::Turquoise},
    {"cyan", Colors::Cyan},
    {"ice", Colors::Ice},
    {"skyblue", Colors::SkyBlue},
    {"blue", Colors::Blue},
    {"navy", Colors::Navy},
    {"indigo", Colors::Indigo},
    {"purple", Colors::Purple},
    {"violet", Colors::Violet},
    {"lavender", Colors::Lavender},
    {"magenta", Colors::Magenta},
    {"pink", Colors::Pink},
    {"rose", Colors::Rose},
    {"brown", Colors::Brown},
    {"tan", Colors::Tan},
    {"beige", Colors::Beige},
    {"transparent", Colors::Transparent},
};

bool IsHex(char32_t c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

// A colour tag: a name (spaces, '-' and '_' ignored, any case) or #RRGGBB / #RRGGBBAA.
bool ParseColor(std::u32string_view tag, Color& out) {
    if (!tag.empty() && tag.front() == U'#') {
        std::u32string_view digits = tag.substr(1);
        if (digits.size() != 6 && digits.size() != 8) return false;
        for (char32_t c : digits)
            if (!IsHex(c)) return false;
        out = Color::Hex(Detail::Encode(tag));
        return true;
    }
    std::string name;
    for (char32_t c : tag) {
        if (c == U' ' || c == U'-' || c == U'_') continue;
        if (c >= U'A' && c <= U'Z') c += 32;
        if (c > 0x7F) return false;
        name.push_back(char(c));
    }
    for (const NamedColor& named : Named)
        if (name == named.Name) {
            out = named.Value;
            return true;
        }
    return false;
}

// Reads Rich markup. Each tag opens a colour or an effect; [/] closes the last one.
std::vector<Letter> RichLetters(std::string_view markup, Color base) {
    struct Open {
        bool IsColor;
        Color Value;
        std::uint8_t Effect;
    };
    std::vector<Open> stack;
    auto current = [&](Letter& letter) {
        letter.Color = base;
        letter.Effects = None;
        for (const Open& open : stack) {
            if (open.IsColor) letter.Color = open.Value.WithAlpha(open.Value.A * base.A);
            letter.Effects |= open.Effect;
        }
    };
    std::u32string text = Detail::Decode(markup);
    std::vector<Letter> letters;
    for (std::size_t i = 0; i < text.size(); ++i) {
        char32_t c = text[i];
        if (c == U'[') {
            if (i + 1 < text.size() && text[i + 1] == U'[') {
                Letter letter{U'['};
                current(letter);
                letters.push_back(letter);
                ++i;
                continue;
            }
            std::size_t close = text.find(U']', i + 1);
            if (close != std::u32string::npos) {
                std::u32string_view tag = std::u32string_view(text).substr(i + 1, close - i - 1);
                Color color;
                bool known = true;
                if (tag == U"/") {
                    if (!stack.empty()) stack.pop_back();
                } else if (tag == U"wave") {
                    stack.push_back({false, {}, Wave});
                } else if (tag == U"shake") {
                    stack.push_back({false, {}, Shake});
                } else if (ParseColor(tag, color)) {
                    stack.push_back({true, color, None});
                } else {
                    known = false;
                }
                if (known) {
                    i = close;
                    continue;
                }
            }
        }
        Letter letter{c};
        current(letter);
        letters.push_back(letter);
    }
    return letters;
}

float AdvanceOf(const Detail::FontData& font, char32_t c) {
    if (c == U' ') return font.Space;
    const Detail::FontGlyph* glyph = font.Find(c);
    return glyph ? glyph->Advance : 0.0f;
}

// Width of a line at Size 1: up to the right edge of its last visible letter.
float LineWidth(const Detail::FontData& font, const std::vector<Letter>& letters, std::size_t from, std::size_t to) {
    float pen = 0, right = 0;
    for (std::size_t i = from; i < to; ++i) {
        char32_t c = letters[i].Character;
        if (c != U' ') {
            if (const Detail::FontGlyph* glyph = font.Find(c))
                right = Max(right, pen + glyph->Offset.X + glyph->Image.Size().X);
        }
        pen += AdvanceOf(font, c);
    }
    return right;
}

struct Line {
    std::size_t From, To; // letters [From, To), without the space or newline that ended it
};

// Breaks at newlines and, with `width` > 0 (at Size 1), at the last space that keeps the line inside it.
std::vector<Line> BreakLines(const Detail::FontData& font, const std::vector<Letter>& letters, float width) {
    std::vector<Line> lines;
    std::size_t start = 0;
    auto finish = [&](std::size_t end) {
        // Trailing spaces are not part of the line.
        std::size_t to = end;
        while (to > start && letters[to - 1].Character == U' ') --to;
        lines.push_back({start, to});
    };
    std::size_t i = 0;
    while (i <= letters.size()) {
        if (i == letters.size() || letters[i].Character == U'\n') {
            finish(i);
            start = i + 1;
            ++i;
            continue;
        }
        if (width > 0 && letters[i].Character != U' ' && LineWidth(font, letters, start, i + 1) > width) {
            // Back to the last space of this line; a single word too long breaks where it is.
            std::size_t space = i;
            while (space > start && letters[space].Character != U' ') --space;
            if (space > start) {
                finish(space);
                start = space + 1;
                while (start < letters.size() && letters[start].Character == U' ') ++start;
                i = start;
                continue;
            }
            if (i > start) {
                finish(i);
                start = i;
                continue;
            }
        }
        ++i;
    }
    return lines;
}

float BlockHeight(std::size_t lines, float step, float capHeight) {
    return lines == 0 ? 0.0f : float(lines - 1) * step + capHeight;
}

std::vector<Glyph> Place(const std::vector<Letter>& letters, Vec2 position, const TextStyle& style, float width) {
    const Detail::FontData& font = FontOf(style);
    const float scale = ScaleOf(style), step = LineStep(style, font) * scale;
    const std::vector<Line> lines = BreakLines(font, letters, width > 0 ? width / scale : 0);
    const float time = float(Time::RealTotal());
    position = {std::floor(position.X), std::floor(position.Y)};

    // Each letter's index in the text, counting every character except newlines.
    std::vector<int> index(letters.size() + 1, 0);
    for (std::size_t i = 0, n = 0; i < letters.size(); ++i) {
        index[i] = int(n);
        if (letters[i].Character != U'\n') ++n;
    }

    std::vector<Glyph> glyphs;
    for (std::size_t l = 0; l < lines.size(); ++l) {
        const Line& line = lines[l];
        float lineWidth = LineWidth(font, letters, line.From, line.To) * scale;
        float x = position.X;
        if (width > 0) {
            if (style.Align == Align::Center)
                x += std::floor((width - lineWidth) * 0.5f);
            else if (style.Align == Align::Right)
                x += width - lineWidth;
        } else {
            if (style.Align == Align::Center)
                x -= std::floor(lineWidth * 0.5f);
            else if (style.Align == Align::Right)
                x -= lineWidth;
        }
        float y = position.Y + float(l) * step;
        for (std::size_t i = line.From; i < line.To; ++i) {
            const Letter& letter = letters[i];
            if (letter.Character != U' ') {
                if (const Detail::FontGlyph* found = font.Find(letter.Character)) {
                    Glyph glyph;
                    glyph.Character = letter.Character;
                    glyph.Image = found->Image;
                    glyph.Position = Vec2(x, y) + found->Offset * scale;
                    glyph.Scale = scale;
                    glyph.Color = letter.Color;
                    glyph.Index = index[i];
                    glyph.Line = int(l);
                    if (letter.Effects & Wave)
                        glyph.Position.Y +=
                            std::round(std::sin(time * 7.0f - float(glyph.Index) * 0.7f) * 1.5f) * scale;
                    if (letter.Effects & Shake)
                        glyph.Position += Vec2(std::round(Noise1D(time * 30.0f, std::uint32_t(glyph.Index * 2))),
                                               std::round(Noise1D(time * 30.0f, std::uint32_t(glyph.Index * 2 + 1)))) *
                                          scale;
                    glyphs.push_back(glyph);
                }
            }
            x += AdvanceOf(font, letter.Character) * scale;
        }
    }
    return glyphs;
}

void Queue(const Glyph& glyph, Vec2 offset, Color color, const TextStyle& style) {
    Screen::Draw(Sprite2D{.Region = glyph.Image,
                          .Position = glyph.Position + offset,
                          .Scale = {glyph.Scale, glyph.Scale},
                          .Origin = Vec2(0, 0),
                          .Color = color,
                          .Layer = style.Layer,
                          .Depth = style.Depth});
}

float HeightOf(const std::vector<Letter>& letters, const TextStyle& style, float width) {
    const Detail::FontData& font = FontOf(style);
    const float scale = ScaleOf(style);
    std::size_t lines = letters.empty() ? 0 : BreakLines(font, letters, width > 0 ? width / scale : 0).size();
    return BlockHeight(lines, LineStep(style, font) * scale, font.CapHeight * scale);
}

} // namespace

// ---------------------------------------------------------------- Typewriter

void Typewriter::Start(std::string_view text) {
    Source = text;
    Plain = Detail::Decode(Text::Strip(text));
    std::erase(Plain, U'\n'); // newlines are not counted, like Glyph::Index
    Count = int(Plain.size());
    Shown = 0;
    Wait = 0;
}

int Typewriter::Update() { return Update(Time::Delta()); }

int Typewriter::Update(float seconds) {
    if (Done()) return 0;
    if (Speed <= 0) {
        int added = Count - Shown;
        Skip();
        return added;
    }
    int before = Shown;
    Wait -= seconds;
    while (Wait <= 0 && Shown < Count) {
        char32_t c = Plain[std::size_t(Shown)];
        ++Shown;
        Wait += 1.0f / Speed;
        // A pause after punctuation that ends a phrase (not inside "..." or "3.14").
        bool last = Shown >= Count || Plain[std::size_t(Shown)] == U' ';
        if (last) {
            if (c == U'.' || c == U'!' || c == U'?' || c == U'…' || c == U':' || c == U';')
                Wait += Pause;
            else if (c == U',')
                Wait += Pause * 0.5f;
        }
    }
    return Shown - before;
}

void Typewriter::Skip() {
    Shown = Count;
    Wait = 0;
}

// ---------------------------------------------------------------- Text

namespace Text {

Vec2 Measure(std::string_view text, const TextStyle& style, float width) {
    std::vector<Letter> letters = PlainLetters(text, style.Color);
    const Detail::FontData& font = FontOf(style);
    const float scale = ScaleOf(style);
    std::vector<Line> lines =
        letters.empty() ? std::vector<Line>{} : BreakLines(font, letters, width > 0 ? width / scale : 0);
    float widest = 0;
    for (const Line& line : lines) widest = Max(widest, LineWidth(font, letters, line.From, line.To) * scale);
    return {widest, BlockHeight(lines.size(), LineStep(style, font) * scale, font.CapHeight * scale)};
}

std::vector<std::string> Wrap(std::string_view text, float width, const TextStyle& style) {
    std::vector<Letter> letters = PlainLetters(text, style.Color);
    std::vector<std::string> out;
    if (letters.empty()) return out;
    for (const Line& line : BreakLines(FontOf(style), letters, width / ScaleOf(style))) {
        std::u32string part;
        for (std::size_t i = line.From; i < line.To; ++i) part.push_back(letters[i].Character);
        out.push_back(Detail::Encode(part));
    }
    return out;
}

std::vector<Glyph> Layout(std::string_view text, Vec2 position, const TextStyle& style, float width) {
    return Place(PlainLetters(text, style.Color), position, style, width);
}

std::vector<Glyph> RichLayout(std::string_view markup, Vec2 position, const TextStyle& style, float width) {
    return Place(RichLetters(markup, style.Color), position, style, width);
}

void Draw(const std::vector<Glyph>& glyphs, const TextStyle& style) {
    if (style.Shadow.A > 0)
        for (const Glyph& glyph : glyphs)
            Queue(glyph, Vec2(glyph.Scale, glyph.Scale), style.Shadow.WithAlpha(style.Shadow.A * glyph.Color.A), style);
    if (style.Outline.A > 0) {
        for (const Glyph& glyph : glyphs) {
            Color outline = style.Outline.WithAlpha(style.Outline.A * glyph.Color.A);
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                    if (dx || dy) Queue(glyph, Vec2(float(dx), float(dy)) * glyph.Scale, outline, style);
        }
    }
    for (const Glyph& glyph : glyphs) Queue(glyph, {0, 0}, glyph.Color, style);
}

void Draw(std::string_view text, Vec2 position, const TextStyle& style) { Draw(Layout(text, position, style), style); }

float Draw(const Typewriter& typewriter, Vec2 position, const TextStyle& style, float width) {
    std::vector<Glyph> glyphs = RichLayout(typewriter.Text(), position, style, width);
    std::erase_if(glyphs, [&](const Glyph& g) { return g.Index >= typewriter.Visible(); });
    Draw(glyphs, style);
    return HeightOf(RichLetters(typewriter.Text(), style.Color), style, width);
}

float Paragraph(std::string_view text, Vec2 position, float width, const TextStyle& style) {
    Draw(Layout(text, position, style, width), style);
    return HeightOf(PlainLetters(text, style.Color), style, width);
}

float Rich(std::string_view markup, Vec2 position, const TextStyle& style, float width) {
    std::vector<Letter> letters = RichLetters(markup, style.Color);
    Draw(Place(letters, position, style, width), style);
    return HeightOf(letters, style, width);
}

std::string Strip(std::string_view markup) {
    std::u32string plain;
    for (const Letter& letter : RichLetters(markup, Colors::White)) plain.push_back(letter.Character);
    return Detail::Encode(plain);
}

} // namespace Text

} // namespace BitMotor
