#include "Printing.h"
#include "Testing.h"

#include "BitMotor/Core/Files.h"
#include "BitMotor/Render/Atlas.h"
#include "BitMotor/Text/Language.h"
#include "Core/TimeInternal.h"
#include "Render/SpritesInternal.h"
#include "Text/FontInternal.h"

using namespace BitMotor;

namespace {

Texture Blank() {
    std::vector<std::uint8_t> pixels(32 * 32 * 4, 255);
    return Texture::FromPixels(32, 32, pixels);
}

float FirstX(const std::vector<Glyph>& glyphs) { return glyphs.empty() ? -1.0f : glyphs[0].Position.X; }

std::string Join(const std::vector<std::string>& lines) {
    std::string out;
    for (const std::string& line : lines) out += (out.empty() ? "" : "|") + line;
    return out;
}

} // namespace

TEST(PixelFontHasEveryPrintableCharacter) {
    Font font = Font::Pixel();
    CHECK(font);
    for (char32_t c = 33; c < 127; ++c) CHECK(font.Has(c));
    for (char32_t c : std::u32string(U"áéíóúüñÁÉÍÓÚÜÑ¿¡çÇàèòâêô€…ºª°")) CHECK(font.Has(c));
    CHECK(font.Has(U'—')); // through its fallback, '-'
    CHECK(!font.Has(U'漢'));
    CHECK_EQ(font.CapHeight(), 7.0f);
    CHECK_EQ(font.LineHeight(), 11.0f);
}

TEST(AtlasFontFallsBackToCapitals) {
    Atlas atlas = Atlas::Parse(R"({"sprites": {"f_65": [0, 0, 5, 9, 0, 0], "f_66": [6, 0, 5, 9, 0, 0],
                                              "tree": [0, 10, 8, 8, 0, 0], "f_x": [0, 0, 1, 1, 0, 0]}})",
                               Blank());
    Font font = Font::FromAtlas(atlas);
    CHECK(font);
    CHECK(font.Has(U'A'));
    CHECK(font.Has(U'a'));
    CHECK(font.Has(U'á')); // á -> a -> A
    CHECK(font.Has(U'Á'));
    CHECK(!font.Has(U'C'));
    std::vector<Glyph> glyphs = Text::Layout("ab", {0, 0}, {.Font = font});
    CHECK_EQ(glyphs.size(), 2u);
    CHECK_EQ(glyphs[1].Image.Area, Rect(6, 0, 5, 9));
    CHECK(!Font::FromAtlas(Atlas::Parse(R"({"sprites": {"tree": [0, 0, 8, 8, 0, 0]}})", Blank())));
}

TEST(DefaultFontReplacesThePixelOne) {
    Atlas atlas = Atlas::Parse(R"({"sprites": {"f_65": [0, 0, 9, 9, 0, 0]}})", Blank());
    Font big = Font::FromAtlas(atlas, {.Advance = 10});
    CHECK(Font::Default() == Font::Pixel());
    Font::SetDefault(big);
    CHECK(Font::Default() == big);
    CHECK_EQ(Text::Measure("AA").X, 19.0f); // an empty style draws with it
    CHECK(!Font().Has(U'B'));
    Font::SetDefault({});
    CHECK(Font::Default() == Font::Pixel());
    CHECK_EQ(Text::Measure("AA").X, 11.0f);
}

TEST(TextUtf8RoundTrips) {
    std::string text = "Año ¿qué? 漢 🎮";
    CHECK_EQ(Detail::Encode(Detail::Decode(text)), text);
    CHECK_EQ(Detail::Decode("A\xFF"
                            "B")
                 .size(),
             3u); // a broken byte becomes one replacement character
    CHECK(Detail::Decode("A\xFF"
                         "B")[1] == char32_t(0xFFFD));
    CHECK_EQ(Detail::Decode("\xE2\x82").size(), 2u); // cut in the middle
}

TEST(TextMeasureCountsTheCapitals) {
    CHECK_EQ(Text::Measure(""), Vec2(0, 0));
    CHECK_EQ(Text::Measure("A"), Vec2(5, 7));
    CHECK_EQ(Text::Measure("AB"), Vec2(11, 7));
    CHECK_EQ(Text::Measure("AB "), Vec2(11, 7)); // trailing spaces do not count
    CHECK_EQ(Text::Measure("A\nBCD"), Vec2(17, 18));
    CHECK_EQ(Text::Measure("AB", {.Size = 3}), Vec2(33, 21));
    CHECK_EQ(Text::Measure("A\nB", {.LineHeight = 9}), Vec2(5, 16));
}

TEST(TextScaleRoundsToWholePixels) {
    Settings::Get().TextScale = 1.5f;
    CHECK_EQ(Text::Measure("A"), Vec2(10, 14));              // 1.5 -> 2
    CHECK_EQ(Text::Measure("A", {.Size = 3}), Vec2(25, 35)); // 4.5 -> 5
    Settings::Get().TextScale = 0.5f;
    CHECK_EQ(Text::Measure("A"), Vec2(5, 7)); // never below 1
    Settings::Get().TextScale = 1;
}

TEST(TextWrapBreaksAtSpaces) {
    // 11 characters of 6 pixels: 65 wide.
    CHECK_EQ(Join(Text::Wrap("hello world and more", 65)), "hello world|and more");
    CHECK_EQ(Join(Text::Wrap("hello world and more", 64)), "hello|world and|more");
    CHECK_EQ(Join(Text::Wrap("abcdefghij", 29)), "abcde|fghij"); // a word too long breaks inside
    CHECK_EQ(Join(Text::Wrap("one\n\ntwo", 100)), "one||two");
    CHECK_EQ(Join(Text::Wrap("a   b", 5)), "a|b");
    CHECK_EQ(Join(Text::Wrap("hello world", 65 * 2, {.Size = 2})), "hello world");
    CHECK_EQ(Text::Measure("hello world and more", {}, 65), Vec2(65, 18));
}

TEST(TextLayoutPlacesAndAligns) {
    std::vector<Glyph> glyphs = Text::Layout("A B\nC", {10.7f, 20});
    CHECK_EQ(glyphs.size(), 3u);                // spaces and newlines draw nothing
    CHECK_EQ(glyphs[0].Position, Vec2(10, 18)); // floored, and 2 rows of accent room above the capitals
    CHECK_EQ(glyphs[1].Position, Vec2(22, 18));
    CHECK_EQ(glyphs[2].Position, Vec2(10, 29));
    CHECK_EQ(glyphs[0].Index, 0);
    CHECK_EQ(glyphs[1].Index, 2);
    CHECK_EQ(glyphs[2].Index, 3); // the newline does not count
    CHECK_EQ(glyphs[2].Line, 1);

    CHECK_EQ(FirstX(Text::Layout("AB", {100, 0}, {.Align = Align::Center})), 95.0f);
    CHECK_EQ(FirstX(Text::Layout("AB", {100, 0}, {.Align = Align::Right})), 89.0f);
    // Inside a width, aligned within it.
    CHECK_EQ(FirstX(Text::Layout("AB", {0, 0}, {.Align = Align::Center}, 31)), 10.0f);
    CHECK_EQ(FirstX(Text::Layout("AB", {0, 0}, {.Align = Align::Right}, 31)), 20.0f);
    std::vector<Glyph> big = Text::Layout("AB", {0, 0}, {.Size = 2});
    CHECK_EQ(big[1].Position, Vec2(12, -4));
    CHECK_EQ(big[1].Scale, 2.0f);
}

TEST(TextRichMarkup) {
    CHECK_EQ(Text::Strip("[red]a[/]b[[c[unknown]d[wave]e[/]"), "ab[c[unknown]de");
    CHECK_EQ(Text::Strip("[#FF8800]x[/][sky blue]y[/][Dark_Red]z[/]"), "xyz");
    CHECK_EQ(Text::Strip("[#GG0000]x"), "[#GG0000]x");
    CHECK_EQ(Text::Strip("[/]a"), "a"); // closing with nothing open is ignored

    std::vector<Glyph> glyphs =
        Text::RichLayout("[red]A[gold]B[/]C[/]D", {0, 0}, {.Color = Colors::White.WithAlpha(0.5f)});
    CHECK_EQ(glyphs.size(), 4u);
    CHECK_EQ(glyphs[0].Color, Colors::Red.WithAlpha(0.5f));
    CHECK_EQ(glyphs[1].Color, Colors::Gold.WithAlpha(0.5f));
    CHECK_EQ(glyphs[2].Color, Colors::Red.WithAlpha(0.5f));
    CHECK_EQ(glyphs[3].Color, Colors::White.WithAlpha(0.5f));
    CHECK_EQ(glyphs[3].Index, 3);
    CHECK_EQ(Text::RichLayout("[#00FF00]A", {0, 0}).front().Color.ToHex(), Color::Hex(0x00FF00).ToHex());
    // Effects only move letters, by whole pixels.
    std::vector<Glyph> moving = Text::RichLayout("[wave][shake]ABCD", {0, 0});
    for (const Glyph& g : moving) {
        CHECK_EQ(g.Position.X, std::round(g.Position.X));
        CHECK_EQ(g.Position.Y, std::round(g.Position.Y));
    }
}

TEST(TextDrawQueuesShadowAndOutline) {
    Detail::ResetSprites();
    Text::Draw("AB", {0, 0});
    CHECK_EQ(Detail::SpriteQueue().size(), 2u);
    CHECK(Detail::SpriteQueue()[0].Where == Layer::Hud);
    Detail::ResetSprites();
    Text::Draw("AB", {0, 0}, {.Shadow = Colors::Black});
    CHECK_EQ(Detail::SpriteQueue().size(), 4u);
    CHECK_EQ(Detail::SpriteQueue()[0].Instance.X, 1.0f); // the shadow first, one pixel off
    Detail::ResetSprites();
    Text::Draw("AB", {0, 0}, {.Outline = Colors::Black, .Layer = Layer::World, .Depth = 5});
    CHECK_EQ(Detail::SpriteQueue().size(), 18u);
    CHECK(Detail::SpriteQueue().back().Where == Layer::World);
    CHECK_EQ(Detail::SpriteQueue().back().Depth, 5.0f);
    Detail::ResetSprites();
    CHECK_EQ(Text::Paragraph("hello world and more", {0, 0}, 65), 18.0f);
    CHECK_EQ(Text::Rich("[red]hi[/]", {0, 0}), 7.0f);
    Detail::ResetSprites();
}

TEST(TypewriterShowsLettersOverTime) {
    Typewriter line;
    line.Speed = 10;
    line.Pause = 0.5f;
    line.Start("[red]Hi.[/] Yo");
    CHECK_EQ(line.Total(), 6);
    CHECK_EQ(line.Visible(), 0);
    CHECK_EQ(line.Update(0.25f), 3); // H, i, . at 0, 0.1 and 0.2
    CHECK_EQ(line.Visible(), 3);
    CHECK_EQ(line.Update(0.5f), 0); // the pause after the full stop
    CHECK_EQ(line.Update(0.2f), 2);
    CHECK(!line.Done());
    line.Skip();
    CHECK(line.Done());
    CHECK_EQ(line.Visible(), 6);
    CHECK_EQ(line.Update(1), 0);

    // Drawing shows only the visible part, laid out as the whole text so lines do not jump.
    line.Start("AB CD");
    line.Update(0.15f);
    Detail::ResetSprites();
    CHECK_EQ(Text::Draw(line, {0, 0}), 7.0f);
    CHECK_EQ(Detail::SpriteQueue().size(), 2u);
    Detail::ResetSprites();
    line.Speed = 0; // everything at once
    line.Start("ABC");
    CHECK_EQ(line.Update(0.01f), 3);
}

TEST(TranslatorChangesThePlainText) {
    // gettext style: the original text is the key.
    Text::SetTranslator(
        [](std::string_view text) { return text == "Hola" ? std::string("Hello there") : std::string(text); });
    CHECK_EQ(Text::Measure("Hola").X, Text::Measure("Hello there").X);
    std::vector<std::string> lines = Text::Wrap("Hola", 1000);
    CHECK_EQ(lines.size(), size_t(1));
    CHECK_EQ(lines[0], std::string("Hello there"));
    CHECK_EQ(Text::Layout("Hola", {0, 0}).size(), Text::Layout("Hello there", {0, 0}).size());
    Text::SetTranslator(nullptr);
    CHECK(Text::Measure("Hola").X < Text::Measure("Hello there").X);
}

TEST(TrueTypeFontsAreRasterised) {
    auto bytes = Files::ReadBytes(BITMOTOR_TEST_FONT);
    CHECK(bytes.has_value());
    if (!bytes) return;
    // Press Start 2P is drawn on an 8-pixel grid: at Size 8 and without smoothing, its letters are exact.
    Detail::BakedFont baked;
    std::string error;
    CHECK(Detail::BakeTtf(*bytes, {.Size = 8, .Smooth = false}, baked, error));
    CHECK_EQ(baked.Width, 512);
    CHECK(baked.Height > 0);
    CHECK_EQ(baked.CapHeight, 7.0f);
    CHECK_EQ(baked.Space, 8.0f);
    auto find = [&](char32_t c) -> const Detail::BakedGlyph* {
        for (const Detail::BakedGlyph& glyph : baked.Glyphs)
            if (glyph.Character == c) return &glyph;
        return nullptr;
    };
    const Detail::BakedGlyph* h = find(U'H');
    CHECK(h != nullptr);
    if (!h) return;
    CHECK_EQ(h->Advance, 8.0f);
    CHECK_EQ(h->Offset.Y, 0.0f); // the top of the capital is the pen's line
    CHECK_EQ(h->Area.Height, 7.0f);
    // Every pixel is on or off, white.
    bool hard = true;
    for (std::size_t i = 0; i < baked.Pixels.size(); i += 4)
        hard = hard && (baked.Pixels[i + 3] == 0 || baked.Pixels[i + 3] == 255);
    CHECK(hard);
    CHECK(find(U'ñ') != nullptr);
    CHECK(find(U'漢') == nullptr);
    // Smooth: soft edges at an off-grid size.
    CHECK(Detail::BakeTtf(*bytes, {.Size = 13}, baked, error));
    bool soft = false;
    for (std::size_t i = 0; i < baked.Pixels.size(); i += 4)
        soft = soft || (baked.Pixels[i + 3] > 0 && baked.Pixels[i + 3] < 255);
    CHECK(soft);
    // Not a font.
    const std::uint8_t junk[64] = {1, 2, 3};
    CHECK(!Detail::BakeTtf(junk, {}, baked, error));
    CHECK(!error.empty());
}

TEST(TrueTypeFontsDrawText) {
    Font font = Font::Load(BITMOTOR_TEST_FONT, {.Size = 8, .Smooth = false});
    CHECK(font);
    CHECK_EQ(font.CapHeight(), 7.0f);
    CHECK(font.Has(U'A'));
    CHECK(font.Has(U'é'));
    // Fixed width: 8 pixels per letter; Measure ends at the last pixel of the last letter.
    const Vec2 size = Text::Measure("HH", {.Font = font});
    CHECK_EQ(size.Y, 7.0f);
    CHECK(size.X > 8 && size.X <= 16);
    std::vector<Glyph> glyphs = Text::Layout("AB", {10, 20}, {.Font = font});
    CHECK_EQ(glyphs.size(), 2u);
    if (glyphs.size() == 2) CHECK_EQ(glyphs[1].Position.X - glyphs[0].Position.X, 8.0f);
    CHECK(!Font::Load("missing.ttf"));
}
