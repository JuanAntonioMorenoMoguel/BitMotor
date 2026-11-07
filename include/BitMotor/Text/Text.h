#pragma once
#include "BitMotor/Colors/Color.h"
#include "BitMotor/Colors/Colors.h"
#include "BitMotor/Math/Vector.h"
#include "BitMotor/Render/Sprite.h"
#include "BitMotor/Text/Font.h"

#include <string>
#include <string_view>
#include <vector>

// Drawing text. Positions are the top-left corner of the capitals (Align moves it to the centre or the
// right end of each line); text is UTF-8 and "\n" starts a new line.
//     Text::Draw("SCORE", {4, 4});
//     Text::Draw("PAUSED", Screen::Center(), {.Size = 2, .Align = Align::Center, .Shadow = Colors::Black});
//     float height = Text::Paragraph(description, {10, 40}, 200);       // wrapped to 200 pixels
//     Text::Rich("[gold]100[/] coins, [red][shake]hurry[/][/]!", {10, 80});
//     Vec2 size = Text::Measure("Continue", style);                      // for layouts
//     for (const Glyph& g : Text::Layout("Hi", {0, 0})) ...              // your own renderer
// Text is drawn in the Hud layer unless the style says otherwise (damage numbers can go in the World).
// Settings::TextScale multiplies every Size, rounded to whole pixels so letters stay crisp.
namespace BitMotor {

enum class Align { Left, Center, Right };

struct TextStyle {
    // Empty: Font::Pixel().
    BitMotor::Font Font = {};
    // Whole steps: 2 doubles every pixel of the font.
    int Size = 1;
    BitMotor::Color Color = Colors::White;
    BitMotor::Align Align = Align::Left;
    // A copy one pixel (times Size) down and right in this colour, behind the text. Transparent: none.
    BitMotor::Color Shadow = Colors::Transparent;
    // A one-pixel border around every letter in this colour. Transparent: none.
    BitMotor::Color Outline = Colors::Transparent;
    // Distance between lines, in pixels at Size 1. 0: the font's.
    float LineHeight = 0;
    BitMotor::Layer Layer = Layer::Hud;
    float Depth = 0;
};

// One letter placed by Text::Layout: everything a renderer needs to draw it.
struct Glyph {
    char32_t Character = 0;
    // The font's image for it.
    Region Image = {};
    // Where the top-left corner of Image goes (screen pixels, or world pixels outside the Hud).
    Vec2 Position = {0, 0};
    // How much Image is enlarged (the style's Size after Settings::TextScale).
    float Scale = 1;
    BitMotor::Color Color = Colors::White;
    // Its place in the text (counting characters, not bytes; markup does not count) and its line.
    int Index = 0;
    int Line = 0;
};

// Shows a text little by little, like a dialogue box. The text may carry Rich markup.
//     Typewriter line;
//     line.Start(Tr("intro.1"));
//     // Update:
//     if (line.Update() > 0) blip.Play();
//     if (Input::Pressed(Action::Accept)) line.Done() ? NextLine() : line.Skip();
//     // Draw:
//     Text::Draw(line, {16, 200}, style, 288);
class Typewriter {
public:
    // Characters per second.
    float Speed = 40;
    // Extra seconds after . ! ? … : and ; (half of it after a comma).
    float Pause = 0.25f;

    void Start(std::string_view text);
    // Advances by `seconds` (Time::Delta() by default). Returns how many characters appeared.
    int Update();
    int Update(float seconds);
    // Shows everything at once.
    void Skip();
    bool Done() const { return Shown >= Count; }
    // Characters shown so far, and in total (markup does not count).
    int Visible() const { return Shown; }
    int Total() const { return Count; }
    const std::string& Text() const { return Source; }

private:
    std::string Source;
    std::u32string Plain;
    int Shown = 0, Count = 0;
    float Wait = 0;
};

namespace Text {

// Width and height of the text (height: from the top of the capitals of the first line to the bottom of
// the capitals of the last). With `width` > 0, wrapped to it first.
Vec2 Measure(std::string_view text, const TextStyle& style = {}, float width = 0);
// Splits the text into lines no wider than `width`, breaking at spaces (and inside a word that does not fit).
std::vector<std::string> Wrap(std::string_view text, float width, const TextStyle& style = {});

// The letters of the text, placed. With `width` > 0, wrapped to it and aligned inside [x, x + width].
std::vector<Glyph> Layout(std::string_view text, Vec2 position, const TextStyle& style = {}, float width = 0);
// The same with Rich markup (see Rich). Waving and shaking letters move with Time::RealTotal.
std::vector<Glyph> RichLayout(std::string_view markup, Vec2 position, const TextStyle& style = {}, float width = 0);

void Draw(std::string_view text, Vec2 position, const TextStyle& style = {});
// Draws laid-out letters (moved or recoloured by the game), with the style's shadow, outline and layer.
void Draw(const std::vector<Glyph>& glyphs, const TextStyle& style = {});
// The visible part of a Typewriter. Returns the height used.
float Draw(const Typewriter& typewriter, Vec2 position, const TextStyle& style = {}, float width = 0);
// Wrapped to `width`. Returns the height used.
float Paragraph(std::string_view text, Vec2 position, float width, const TextStyle& style = {});
// Text with markup in square brackets, closed by [/] (the last one opened):
//     colours by name ([red], [gold], [sky blue] or [skyblue]: any of Colors) or [#ff8800];
//     [wave] letters that float up and down, [shake] letters that tremble;
//     [[ writes a '['. Unknown tags are written as they are.
// With `width` > 0 it wraps. Returns the height used.
float Rich(std::string_view markup, Vec2 position, const TextStyle& style = {}, float width = 0);
// The text without its markup.
std::string Strip(std::string_view markup);

} // namespace Text

} // namespace BitMotor
