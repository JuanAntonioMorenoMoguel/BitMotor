// 08_UI: an options menu made with the immediate UI, on the player's Settings.
// - Mouse, keyboard (arrows or WASD, Enter, Esc) or gamepad (D-pad or stick, A, B).
// - Volumes, fullscreen, text size and language change at once; "Save" writes settings.json, so the game
//   starts with them next time. "Reset" asks first, in a modal window.
// - Hover a widget (or move the focus onto it) to see its tooltip.
// - Below the menu: a health bar, an LED meter and the prompt of an action for the device in use.
// F12: screenshot. Esc closes the window that is open, or quits.
#include <BitMotor/BitMotor.h>

#include <algorithm>
#include <cmath>

using namespace BitMotor;

enum class Action { Accept };

struct UiExample : Game {
    bool confirmReset = false;
    Timer saved{2};
    float health = 1;

    void Start() override {
        Input::Bind(Action::Accept, "accept", {Key::Enter, Button::South});
        saved.Elapsed = saved.Duration; // not shown until the first save
        // A sound would play here for every press (phase 5).
        UI::OnClick([] {});
    }

    void Update() override {
        if (Input::KeyPressed(Key::F12)) Screen::Screenshot();
        health = 0.5f + 0.5f * std::sin(float(Time::Total()) * 0.8f);
        saved.Update();
    }

    void Draw() override {
        Screen::Clear(Color::Hex(0x0B0E14));
        Settings& settings = Settings::Get();

        BitMotor::Rect panel{40, 14, 240, 124};
        UI::Panel(panel, Tr("ui.title"));
        const float x = 48, w = 224, h = 13, gap = 15;
        float y = 24;
        auto row = [&] {
            BitMotor::Rect r{x, y, w, h};
            y += gap;
            return r;
        };

        bool changed = false;
        changed |= UI::Slider(row(), Tr("ui.master"), settings.MasterVolume);
        UI::Tooltip(Tr("ui.volume.tip"));
        changed |= UI::Slider(row(), Tr("ui.music"), settings.MusicVolume);
        UI::Tooltip(Tr("ui.volume.tip"));
        changed |= UI::Slider(row(), Tr("ui.sound"), settings.SoundVolume);
        UI::Tooltip(Tr("ui.volume.tip"));
        changed |= UI::Toggle(row(), Tr("ui.fullscreen"), settings.Fullscreen);
        changed |= UI::Slider(row(), Tr("ui.textscale", Text::Number(settings.TextScale)), settings.TextScale, 1, 2, 1);
        if (UI::Button(row(), Tr("ui.language", Text::LanguageName(Text::Language())))) NextLanguage();
        UI::Tooltip(Tr("ui.language.tip"));
        if (changed) Settings::Apply();

        // Save / Reset / Quit side by side: Left and Right move between them.
        float third = std::floor((w - 4) / 3);
        if (UI::Button({x, y, third, h}, Tr("ui.save"))) {
            Settings::Save();
            saved.Start();
        }
        if (UI::Button({x + third + 2, y, third, h}, Tr("ui.reset"))) confirmReset = true;
        if (UI::Button({x + 2 * (third + 2), y, third, h}, Tr("ui.quit"))) Quit();
        if (!saved.Done())
            Text::Draw(Tr("ui.saved"), {160, panel.Bottom() + 4}, {.Color = Colors::Lime, .Align = Align::Center});

        // Widgets that only show something.
        Text::Draw(Tr("ui.health"), {8, 152});
        UI::Bar({48, 152, 80, 7}, health, Colors::Red);
        UI::Segments({48, 163, 80, 7}, health, Colors::Lime);
        UI::Prompt({200, 156}, Action::Accept, Tr("ui.accept"));

        if (confirmReset) {
            BitMotor::Rect window{80, 56, 160, 54};
            UI::BeginModal(window, Tr("ui.reset.title"));
            Text::Paragraph(Tr("ui.reset.ask"), {window.X + 8, window.Y + 10}, window.Width - 16);
            if (UI::Button({window.X + 8, window.Y + 34, 68, h}, Tr("ui.yes"))) {
                Settings::Reset();
                confirmReset = false;
            }
            if (UI::Button({window.Right() - 76, window.Y + 34, 68, h}, Tr("ui.no")) || UI::Pressed(Nav::Back))
                confirmReset = false;
            UI::EndModal();
        } else if (UI::Pressed(Nav::Back)) {
            Quit();
        }
    }

    void NextLanguage() {
        std::vector<std::string> codes = Text::Languages();
        if (codes.empty()) return;
        auto at = std::find(codes.begin(), codes.end(), Text::Language());
        Text::SetLanguage(at == codes.end() || at + 1 == codes.end() ? codes.front() : *(at + 1));
    }
};

int main() {
    return Run<UiExample>({
        .Title = "BitMotor 08 - UI",
        .Width = 320,
        .Height = 180,
        .WindowWidth = 1280,
        .WindowHeight = 720,
    });
}
