// 14_JFxStyle: a desktop editor with UI::JFxStyle, the look of JavaFX (Modena) in BitMotor's immediate UI.
// - The menu bar (File, Edit, View) opens its menus with a click; inside, the mouse or Up/Down and Enter
//   choose, Left/Right go to the next menu and Esc closes. View > Dark theme switches to a dark palette made
//   from another -fx-base; View > About opens a dialog.
// - Drag the bar between the two halves (a SplitPane). On the left: a ListView of instruments with Add and
//   Remove, and a Panel with a ComboBox, Sliders, a CheckBox and RadioButtons.
// - On the right, a TabPane: "Pattern" has Mute/Solo ToggleButtons and a context menu (right-click the
//   notes); "Song" has a TextField, a Slider, a ProgressBar and a DefaultButton.
// - Tab/arrows and Enter move the focus and press, as in every BitMotor UI: the blue ring shows it.
// - Text in Inter (assets/fonts, SIL Open Font License), loaded with Font::Load at 14 pixels.
// F12: screenshot. Esc closes the menu or dialog that is open, or quits.
#include <BitMotor/BitMotor.h>

#include <cmath>
#include <string>
#include <vector>

using namespace BitMotor;
namespace Fx = UI::JFxStyle;

struct Editor : Game {
    Font Inter;
    bool dark = false, about = false;
    float split = 0.3f;
    std::vector<std::string> instruments = {"Lead",  "Bass",    "Kick",  "Snare", "Hi-hat", "Pad",   "Arp",
                                            "Pluck", "Strings", "Brass", "Choir", "Bell",   "Organ", "Sweep"};
    int instrument = 0, scroll = 0, wave = 1, pan = 1, tab = 0;
    float volume = 0.5f, attack = 0.1f, release = 0.3f, bpm = 120, exported = 0;
    bool lowPass = false, loop = true, exporting = false;
    bool mute[4] = {}, solo[4] = {};
    int transpose = 0;
    std::string title;

    void Start() override {
        Inter = Font::Load("14_JFxStyle/Assets/fonts/Inter-Regular.otf", {.Size = 14});
        UsePalette();
    }

    void UsePalette() {
        Fx::Palette palette = dark ? Fx::Modena(Color::Hex(0x3C3F41), Color::Hex(0x4A88C7)) : Fx::Modena();
        palette.Text = {.Font = Inter};
        Fx::SetPalette(palette);
        UI::SetTheme(Fx::Theme()); // tooltips (and any UI:: widget) in the same colours
    }

    void Update() override {
        if (Input::KeyPressed(Key::F12)) Screen::Screenshot();
        if (exporting) {
            exported += float(Time::Delta()) * 0.25f;
            if (exported >= 1) exporting = false;
        }
    }

    void Draw() override {
        const float width = Screen::Width(), height = Screen::Height();
        Fx::Background();
        const float bar = 28, status = 26;
        DrawMenus({0, 0, width, bar});

        // Below the menu bar: the SplitPane, then a status bar.
        BitMotor::Rect left, right;
        Fx::SplitPane({0, bar, width, height - bar - status}, split, left, right);
        DrawInstruments(left);
        DrawSong(right);
        Fx::Separator({0, height - status, width, 1});
        Fx::Label({12, height - status, width - 24, status}, Tr("jfx.status"));

        if (about) {
            const BitMotor::Rect window = Rect::FromCenter(Screen::Center(), {440, 190});
            const BitMotor::Rect inside = Fx::BeginDialog(window, Tr("jfx.about.title"));
            Text::Paragraph(Tr("jfx.about.text"), inside.Position(), inside.Width,
                            {.Font = Inter, .Color = Fx::GetPalette().TextColor});
            if (Fx::DefaultButton({inside.Right() - 100, inside.Bottom() - 30, 100, 30}, Tr("jfx.close")) ||
                UI::Pressed(Nav::Back))
                about = false;
            Fx::EndDialog();
        } else if (UI::Pressed(Nav::Back) && !UI::Typing()) {
            Quit();
        }
    }

    void DrawMenus(BitMotor::Rect area) {
        Fx::BeginMenuBar(area);
        if (Fx::Menu(Tr("jfx.file"))) {
            Fx::MenuItem(Tr("jfx.new"), "Ctrl+N");
            Fx::MenuItem(Tr("jfx.open"), "Ctrl+O");
            Fx::MenuItem(Tr("jfx.save"), "Ctrl+S");
            Fx::MenuSeparator();
            if (Fx::MenuItem(Tr("jfx.quit"), "Ctrl+Q")) Quit();
            Fx::EndMenu();
        }
        if (Fx::Menu(Tr("jfx.edit"))) {
            if (Fx::MenuItem(Tr("jfx.undo"), "Ctrl+Z", transpose != 0)) transpose = 0;
            Fx::MenuItem(Tr("jfx.copy"), "Ctrl+C");
            Fx::MenuItem(Tr("jfx.paste"), "Ctrl+V");
            Fx::EndMenu();
        }
        if (Fx::Menu(Tr("jfx.view"))) {
            if (Fx::CheckMenuItem(Tr("jfx.dark"), dark, "Ctrl+D")) UsePalette();
            Fx::MenuSeparator();
            if (Fx::MenuItem(Tr("jfx.about"))) about = true;
            Fx::EndMenu();
        }
        Fx::EndMenuBar();
    }

    void DrawInstruments(BitMotor::Rect area) {
        const BitMotor::Rect box = area.Expand(-10);
        BitMotor::Rect list = Fx::Panel({box.X, box.Y, box.Width, 250}, Tr("jfx.instruments"));
        Fx::ListView({list.X, list.Y, list.Width, list.Height - 38}, instrument, instruments, scroll);
        const float half = std::floor((list.Width - 8) / 2);
        if (Fx::Button({list.X, list.Bottom() - 28, half, 28}, Tr("jfx.add")))
            instruments.push_back(Tr("jfx.instrument") + " " + std::to_string(instruments.size() + 1));
        if (Fx::Button({list.Right() - half, list.Bottom() - 28, half, 28}, Tr("jfx.remove"), instruments.size() > 1))
            instruments.erase(instruments.begin() + instrument);

        const BitMotor::Rect panel = Fx::Panel({box.X, box.Y + 262, box.Width, box.Height - 262}, Tr("jfx.instrument"));
        float y = panel.Y;
        const float labelWidth = 90, field = panel.Width - labelWidth;
        auto row = [&](std::string_view label, float rowHeight = 28) {
            Fx::Label({panel.X, y, labelWidth, rowHeight}, label);
            BitMotor::Rect r{panel.X + labelWidth, y, field, rowHeight};
            y += rowHeight + 8;
            return r;
        };
        Fx::ComboBox(row(Tr("jfx.wave")), wave, {"Sine", "Square", "Saw", "Triangle", "Noise"});
        Fx::Slider(row(Tr("jfx.volume")), volume, 0, 1, 0.05f);
        Fx::Slider(row(Tr("jfx.attack")), attack, 0, 1, 0.01f);
        Fx::Slider(row(Tr("jfx.release")), release, 0, 1, 0.01f);
        Fx::CheckBox({panel.X, y, panel.Width, 24}, Tr("jfx.lowpass"), lowPass);
        y += 32;
        Fx::Label({panel.X, y, labelWidth, 24}, Tr("jfx.pan"));
        const float third = std::floor(field / 3);
        Fx::RadioButton({panel.X + labelWidth, y, third, 24}, Tr("jfx.left"), pan, 0);
        Fx::RadioButton({panel.X + labelWidth + third, y, third, 24}, Tr("jfx.center"), pan, 1);
        Fx::RadioButton({panel.X + labelWidth + 2 * third, y, third, 24}, Tr("jfx.right"), pan, 2);
    }

    void DrawSong(BitMotor::Rect area) {
        const BitMotor::Rect page = Fx::TabPane(area.Expand(-10), tab, {Tr("jfx.pattern"), Tr("jfx.song")});
        const BitMotor::Rect inside = page.Expand(-12);
        if (tab == 0) {
            // Four tracks: Mute and Solo over a column of notes each.
            const float column = std::floor((inside.Width - 3 * 10) / 4);
            const BitMotor::Rect notes{inside.X, inside.Y + 76, inside.Width, inside.Height - 76};
            static const char* names[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
            for (int t = 0; t < 4; ++t) {
                rowNames[t].clear();
                for (int r = 0; r < 32; ++r) {
                    const int note = 48 + (r * (t + 3) + t * 5 + transpose) % 24;
                    rowNames[t].push_back(r % (t + 2) == 0
                                              ? std::string(names[note % 12]) + std::to_string(note / 12 - 1)
                                              : std::string("---"));
                }
            }
            for (int t = 0; t < 4; ++t) {
                const float x = inside.X + float(t) * (column + 10);
                Fx::Label({x, inside.Y, column, 24}, Tr("jfx.track", t + 1), Align::Center);
                const float half = std::floor((column - 6) / 2);
                Fx::ToggleButton({x, inside.Y + 30, half, 28}, Tr("jfx.mute"), mute[t]);
                Fx::ToggleButton({x + column - half, inside.Y + 30, half, 28}, Tr("jfx.solo"), solo[t]);
                // The four lists share their selected row and scroll: a cursor across the tracks.
                Fx::ListView({x, notes.Y, column, notes.Height}, noteRow, rowNames[t], noteScroll);
            }
            if (Fx::BeginContextMenu(notes)) {
                if (Fx::MenuItem(Tr("jfx.transpose.up"))) ++transpose;
                if (Fx::MenuItem(Tr("jfx.transpose.down"))) --transpose;
                Fx::MenuSeparator();
                Fx::MenuItem(Tr("jfx.copy"), "Ctrl+C");
                Fx::MenuItem(Tr("jfx.paste"), "Ctrl+V");
                Fx::EndMenu();
            }
        } else {
            float y = inside.Y;
            const float w = Min(inside.Width, 520.0f);
            Fx::TextField({inside.X, y, w, 28}, Tr("jfx.title"), title, {.Placeholder = Tr("jfx.title.hint")});
            y += 40;
            Fx::Label({inside.X, y, w * 0.5f, 28}, Tr("jfx.bpm", int(bpm)));
            Fx::Slider({inside.X + w * 0.5f, y, w * 0.5f, 28}, bpm, 40, 240, 1);
            y += 40;
            Fx::CheckBox({inside.X, y, w, 24}, Tr("jfx.loop"), loop);
            y += 40;
            if (Fx::Button({inside.X, y, 120, 30}, Tr("jfx.play"))) exported = 0;
            UI::Tooltip(Tr("jfx.tooltip"));
            if (Fx::DefaultButton({inside.X + 130, y, 160, 30}, Tr("jfx.export"), !exporting)) {
                exporting = true;
                exported = 0;
            }
            y += 46;
            Fx::ProgressBar({inside.X, y, w, 18}, exported);
            Fx::Label({inside.X, y + 22, w, 24}, Tr("jfx.exporting", Text::Percent(Min(exported, 1.0f))));
        }
    }

    std::vector<std::string> rowNames[4];
    int noteRow = 0, noteScroll = 0;
};

int main() {
    return Run<Editor>({
        .Title = "BitMotor 14 - JFxStyle",
        .Width = 1280,
        .Height = 720,
        .WindowWidth = 1280,
        .WindowHeight = 720,
    });
}
