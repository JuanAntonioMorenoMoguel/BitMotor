// 07_Text: text, translations and shapes.
// - Every text comes from lang/en.json or lang/es.json through Tr; L switches the language.
// - A wrapped paragraph (Tab changes its alignment; + and - change Settings::TextScale), Rich text with
//   colours, waves and shaking letters, and numbers written the way the language does.
// - A dialogue box with a Typewriter: Space shows the whole line, or the next one.
// - Shapes from UI: a spinning star (Polygon), circles, a triangle and lines.
// F11: fullscreen, F12: screenshot, Esc: quit.
#include <BitMotor/BitMotor.h>

#include <cmath>
#include <format>

using namespace BitMotor;

struct TextExample : Game {
    Typewriter dialogue;
    int line = 0;
    Align align = Align::Left;
    double score = 0;
    float time = 0;
    bool autoPlay = false;
    Timer nextLine{2.5f};
    Subscription onLanguage;

    void Start() override {
        autoPlay = Args::Has("--frames");
        // Translated texts are read again when the language changes.
        onLanguage = Events::On<LanguageChanged>([this](const LanguageChanged&) { Say(); });
        Say();
    }

    void Say() { dialogue.Start(Tr(std::format("text.dialogue.{}", line))); }

    void Update() override {
        if (Input::KeyPressed(Key::Escape)) Quit();
        if (Input::KeyPressed(Key::F11)) Window::ToggleFullscreen();
        if (Input::KeyPressed(Key::F12)) Screen::Screenshot();
        if (Input::KeyPressed(Key::L)) Text::SetLanguage(Text::Language() == "es" ? "en" : "es");
        if (Input::KeyPressed(Key::Tab))
            align = align == Align::Left ? Align::Center : align == Align::Center ? Align::Right : Align::Left;
        if (Input::KeyPressed(Key::Equals) || Input::KeyPressed(Key::KeypadPlus)) Settings::Get().TextScale += 1;
        if (Input::KeyPressed(Key::Minus) || Input::KeyPressed(Key::KeypadMinus)) Settings::Get().TextScale -= 1;
        Settings::Get().TextScale = Clamp(Settings::Get().TextScale, 1.0f, 2.0f);

        bool next = Input::KeyPressed(Key::Space) || (autoPlay && dialogue.Done() && nextLine.Update());
        if (next) {
            if (!dialogue.Done()) {
                dialogue.Skip();
            } else {
                line = (line + 1) % 3;
                Say();
                nextLine.Start();
            }
        }
        dialogue.Update(); // returns how many letters appeared: a "blip" sound would go here (phase 5)

        score += Time::Delta() * 1234.5;
        time += Time::Delta();
    }

    void Draw() override {
        Screen::Clear(Color::Hex(0x141826));
        Text::Draw(Tr("text.title"), {160, 6},
                   {.Size = 2,
                    .Color = Colors::Gold,
                    .Align = Align::Center,
                    .Shadow = Colors::Black,
                    .Outline = Colors::Maroon});

        // A paragraph inside a frame.
        BitMotor::Rect box{8, 28, 150, 76};
        UI::Rect(box, Color::Hex(0x0B0E18));
        UI::RectOutline(box, Colors::Slate);
        Text::Paragraph(Tr("text.paragraph"), {box.X + 4, box.Y + 4}, box.Width - 8, {.Align = align});

        // Rich text and numbers on the right.
        float x = 166;
        float y = 28 + Text::Rich(Tr("text.rich"), {x, 28}, {}, 146) + 6;
        TextStyle number{.Color = Colors::LightGray};
        Text::Draw(Tr("text.score", Text::Number(score)), {x, y}, number);
        Text::Draw(Tr("text.short", Text::ShortNumber(score * 100)), {x, y + 10}, number);
        Text::Draw(Tr("text.percent", Text::Percent(std::fmod(time / 8.0, 1.0))), {x, y + 20}, number);
        Text::Draw(Tr("text.clock", Text::Clock(time, true)), {x, y + 30}, number);
        Text::Paragraph(Tr("text.hint"), {x, y + 44}, 146, {.Color = Colors::Gray});

        // Shapes.
        Vec2 star{30, 122};
        Vec2 points[10];
        for (int i = 0; i < 10; ++i) points[i] = star + FromAngle(time + float(i) * Tau / 10) * (i % 2 ? 6.0f : 14.0f);
        UI::Polygon(points, Colors::Gold);
        UI::CircleOutline({68, 122}, 13, Colors::Cyan);
        UI::Circle({68, 122}, 6 + 2 * std::sin(time * 3), Colors::Teal);
        UI::Triangle({90, 135}, {104, 108}, {118, 135}, Colors::Coral);
        for (int i = 0; i < 3; ++i)
            UI::Line({140, 122}, Vec2{140, 122} + FromAngle(time * (1 + float(i))) * 14, Colors::Lime, float(i + 1));

        // The dialogue box.
        BitMotor::Rect talk{8, 140, 304, 34};
        UI::Rect(talk, Color::Hex(0x05070A, 0.92f));
        UI::RectOutline(talk, Colors::Gold);
        Text::Draw(dialogue, {talk.X + 6, talk.Y + 6}, {}, talk.Width - 12);
        if (dialogue.Done() && std::fmod(time, 0.8f) < 0.5f)
            UI::Triangle({talk.Right() - 10, talk.Bottom() - 7}, {talk.Right() - 4, talk.Bottom() - 7},
                         {talk.Right() - 7, talk.Bottom() - 4}, Colors::Gold);
    }
};

int main() {
    return Run<TextExample>({
        .Title = "BitMotor 07 - Text",
        .Width = 320,
        .Height = 180,
        .WindowWidth = 1280,
        .WindowHeight = 720,
    });
}
