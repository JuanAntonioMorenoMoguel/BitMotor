// 13_Extras: TrueType fonts, a RenderTexture, a dropdown, a text field and the Wipe and Slide transitions.
// - Type your name in the box (click it, or move the focus onto it and press Enter), choose a transition
//   and a side in the dropdowns and press "Go": the greeting comes in with that transition, and "Back" (or
//   Esc) returns the same way.
// - The spinning badge on the right is drawn into a 64x64 RenderTexture each frame, then shown twice: at
//   its size and enlarged and turned, like any other texture.
// - Titles use Press Start 2P (assets/fonts, SIL Open Font License), loaded with Font::Load at 8 pixels.
// F12: screenshot. Esc on the first screen quits (unless you are typing).
#include <BitMotor/BitMotor.h>

#include <cmath>
#include <string>

using namespace BitMotor;

namespace {

// What the two screens share.
struct Profile {
    std::string Name = "Ñenjis";
    int Look = 2;    // Fade, Wipe, Slide
    int Towards = 0; // Left, Right, Up, Down
    Font Title;
    RenderTexture Badge;

    Transition Chosen() const {
        const auto side = Transition::Side(Towards);
        if (Look == 0) return Transition::Fade(0.6f);
        if (Look == 1) return Transition::Wipe(0.7f, side, Color::Hex(0x203050));
        return Transition::Slide(0.45f, side);
    }
} Shared;

TextStyle TitleStyle(Color color = Colors::Gold) {
    return {.Font = Shared.Title, .Color = color, .Shadow = Colors::Black};
}

// The badge: shapes and text drawn into the RenderTexture, in its own 64x64 pixels.
void DrawBadge() {
    const float t = float(Time::RealTotal());
    Shared.Badge.Begin(Color::Hex(0x10141C));
    UI::Circle({32, 32}, 28, Color::Hex(0x34405A));
    const Color dots[6] = {Colors::Red, Colors::Orange, Colors::Gold, Colors::Lime, Colors::SkyBlue, Colors::Violet};
    for (int i = 0; i < 6; ++i) {
        const float angle = t * 1.5f + float(i) * 1.047f;
        UI::Circle(Vec2(32, 32) + Vec2(std::cos(angle), std::sin(angle)) * 18, 5, dots[i]);
    }
    // The first letter (one or two bytes in UTF-8 for the letters the font has).
    const std::string initial =
        Shared.Name.empty() ? "?" : Shared.Name.substr(0, (Shared.Name[0] & 0x80) && Shared.Name.size() > 1 ? 2 : 1);
    Text::Draw(initial, {32, 29}, {.Font = Shared.Title, .Align = Align::Center, .Shadow = Colors::Black});
    Shared.Badge.End();
}

struct Greeting : Scene {
    void Draw() override {
        Screen::Clear(Color::Hex(0x183048));
        DrawBadge();
        const std::string name = Shared.Name.empty() ? "?" : Shared.Name;
        TextStyle small = TitleStyle(Colors::White);
        small.Align = Align::Center;
        Text::Draw("HOLA,", {160, 50}, small);
        TextStyle big = TitleStyle();
        big.Size = 2;
        big.Align = Align::Center;
        Text::Draw(name, {160, 66}, big);
        Screen::Draw(Sprite2D{
            .Region = Shared.Badge.Full(), .Position = {160, 120}, .Origin = Vec2(0.5f, 0.5f), .Layer = Layer::Hud});
        if (UI::Button({120, 156, 80, 14}, "Back") || UI::Pressed(Nav::Back))
            if (!Scenes::InTransition()) Scenes::Pop(Shared.Chosen());
    }
};

struct Form : Scene {
    void Draw() override {
        Screen::Clear(Color::Hex(0x0B0E14));
        DrawBadge();
        Text::Draw("BITMOTOR EXTRAS", {8, 8}, TitleStyle());
        Text::Draw("Font::Load, RenderTexture, Dropdown, TextField", {8, 20}, {.Color = Colors::Gray});

        const float x = 8, w = 200, h = 14;
        UI::Panel({4, 36, 208, 100}, "Profile");
        UI::TextField({x, 46, w, h}, "Name", Shared.Name, {.Placeholder = "your name", .MaxLength = 12});
        UI::Tooltip("Click or press Enter to type; Enter or Esc to finish");
        UI::Dropdown({x, 64, w, h}, "Transition", Shared.Look, {"Fade", "Wipe", "Slide"});
        UI::Dropdown({x, 82, w, h}, "Towards", Shared.Towards, {"Left", "Right", "Up", "Down"}, Shared.Look != 0);
        if (UI::Button({x, 104, 96, h}, "Go") && !Scenes::InTransition())
            Scenes::Push(MakeScene<Greeting>(), Shared.Chosen());
        if (UI::Button({x + 104, 104, 96, h}, "Quit")) Quit();

        // The badge at its size, and enlarged and turning: a RenderTexture is a texture like any other.
        Screen::Draw(Sprite2D{.Region = Shared.Badge.Full(), .Position = {226, 40}, .Layer = Layer::Hud});
        Screen::Draw(Sprite2D{.Region = Shared.Badge.Full(),
                              .Position = {270, 132},
                              .Rotation = float(Time::RealTotal()) * 0.3f,
                              .Scale = {1.25f, 1.25f},
                              .Origin = Vec2(0.5f, 0.5f),
                              .Layer = Layer::Hud});
        Text::Draw(UI::Typing() ? "typing..." : "", {8, 140}, {.Color = Colors::Lime});
        if (!UI::Typing() && UI::Pressed(Nav::Back) && Scenes::Count() == 1) Quit();
    }
};

struct Extras : Game {
    void Start() override {
        Shared.Title = Font::Load("13_Extras/Assets/fonts/PressStart2P.ttf", {.Size = 8, .Smooth = false});
        Shared.Badge = RenderTexture::Create(64, 64);
        Scenes::Push(MakeScene<Form>());
    }
    void Update() override {
        if (Input::KeyPressed(Key::F12)) Screen::Screenshot();
    }
};

} // namespace

int main() {
    return Run<Extras>({
        .Title = "BitMotor 13 - Extras",
        .Width = 320,
        .Height = 180,
        .WindowWidth = 1280,
        .WindowHeight = 720,
    });
}
