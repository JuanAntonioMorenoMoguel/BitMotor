#include "Printing.h"
#include "Testing.h"

#include "BitMotor/UI/JFxStyle.h"
#include "Core/InputInternal.h"
#include "Core/TimeInternal.h"
#include "Render/SpritesInternal.h"
#include "UI/UiInternal.h"

#include <functional>
#include <string>
#include <vector>

using namespace BitMotor;
namespace Fx = UI::JFxStyle;

namespace {

void Frame(const std::function<void()>& draw) {
    Detail::BeginFrame(1.0 / 60);
    Detail::InputFrame();
    Detail::ResetSprites();
    Detail::BeginUi();
    draw();
    Detail::EndUi();
}

void Fresh() {
    Detail::ResetInput();
    Detail::ResetUi();
    Detail::ResetTime(60);
    Detail::ResetSprites();
    Fx::SetPalette(Fx::Modena());
}

void Tap(Key key) {
    Detail::InputKey(key, true);
    Detail::InputKey(key, false);
}

void Click(Vec2 at, MouseButton button = MouseButton::Left) {
    Detail::InputMouseMove(at);
    Detail::InputMouseButton(button, true);
    Detail::InputMouseButton(button, false);
}

bool Near(Color a, Color b) {
    auto close = [](float x, float y) { return std::abs(x - y) <= 1.5f / 255.0f; };
    return close(a.R, b.R) && close(a.G, b.G) && close(a.B, b.B) && close(a.A, b.A);
}

} // namespace

TEST(JFxDeriveMatchesModena) {
    // Modena's shades of -fx-base #ECECEC.
    const Color base = Color::Hex(0xECECEC);
    CHECK(Near(Fx::Derive(base, -0.23f), Color::Hex(0xB5B5B5))); // -fx-outer-border
    CHECK(Near(Fx::Derive(base, 0.264f), Color::Hex(0xF4F4F4))); // -fx-background
    CHECK(Near(Fx::Derive(base, 0.8f), Colors::White));          // -fx-control-inner-background
    CHECK(Near(Fx::Derive(base, -0.06f), Color::Hex(0xDEDEDE))); // -fx-pressed-base
    CHECK_EQ(Fx::Derive(base, 0), base);
    CHECK_EQ(Fx::Derive(base.WithAlpha(0.5f), -1).A, 0.5f);
    // Colours keep their hue: darker red is still red.
    const Color red = Fx::Derive(Colors::Red, -0.5f);
    CHECK(red.R > 0.4f && red.G == 0 && red.B == 0);
}

TEST(JFxPaletteDefaultsAreModena) {
    const Fx::Palette p, m = Fx::Modena();
    for (auto [a, b] : {std::pair{p.Base, m.Base},
                        {p.Background, m.Background},
                        {p.InnerBackground, m.InnerBackground},
                        {p.Hover, m.Hover},
                        {p.Pressed, m.Pressed},
                        {p.OuterBorder, m.OuterBorder},
                        {p.TextBoxBorder, m.TextBoxBorder},
                        {p.BoxBorder, m.BoxBorder},
                        {p.TextColor, m.TextColor},
                        {p.PromptText, m.PromptText},
                        {p.Mark, m.Mark},
                        {p.Accent, m.Accent},
                        {p.Focus, m.Focus},
                        {p.FaintFocus, m.FaintFocus},
                        {p.DefaultButton, m.DefaultButton}})
        CHECK(Near(a, b));
    // A dark base: light text, a dark field.
    const Fx::Palette dark = Fx::Modena(Color::Hex(0x3C3F41), Color::Hex(0x4A88C7));
    CHECK_EQ(dark.TextColor, Colors::White);
    CHECK(dark.InnerBackground.R < dark.Background.R);
    CHECK_EQ(dark.Accent, Color::Hex(0x4A88C7));
    // The UI::Theme for UI's own widgets takes the palette.
    Fx::SetPalette(dark);
    CHECK_EQ(Fx::Theme().Text.Color, Colors::White);
    CHECK_EQ(Fx::Theme().TooltipText, dark.TooltipText);
    Fx::SetPalette(Fx::Modena());
}

TEST(JFxRoundedRectsSmoothTheirCorners) {
    Fresh();
    Fx::RoundedRect({10, 10, 40, 20}, 0, Colors::Red);
    CHECK_EQ(Detail::SpriteQueue().size(), 1u); // square and one colour: one rectangle
    Detail::ResetSprites();
    Fx::RoundedRect({10, 10, 40, 20}, 4, Colors::Red);
    bool partial = false, whole = false;
    float area = 0;
    for (const Detail::QueuedSprite& s : Detail::SpriteQueue()) {
        const Detail::SpriteInstance& i = s.Instance;
        CHECK(i.X >= 10 && i.Y >= 10 && i.X + i.Width <= 50 && i.Y + i.Height <= 30); // inside the area
        partial = partial || (i.A > 0 && i.A < 1);
        whole = whole || i.A == 1;
        area += i.Width * i.Height * i.A;
    }
    CHECK(partial); // smoothed corners
    CHECK(whole);
    // A rounded rectangle loses (4 - pi) r^2 to its corners.
    CHECK_NEAR(area, 40.0f * 20.0f - (4 - 3.14159f) * 16, 1.0f);
    // A gradient goes from one colour at the top to the other at the bottom.
    Detail::ResetSprites();
    Fx::RoundedRect({0, 0, 10, 10}, 0, Colors::White, Colors::Black);
    const auto& rows = Detail::SpriteQueue();
    CHECK_EQ(rows.size(), 10u);
    CHECK(rows.front().Instance.R > 0.9f);
    CHECK(rows.back().Instance.R < 0.1f);
}

TEST(JFxButtonsAndTogglesAnswerClicks) {
    Fresh();
    const Rect a{10, 10, 80, 24}, b{10, 40, 80, 24};
    bool pressed = false, mute = false, check = false;
    int wave = 0;
    auto screen = [&] {
        pressed = Fx::Button(a, "Play");
        Fx::ToggleButton(b, "Mute", mute);
        Fx::CheckBox({10, 70, 80, 20}, "Loop", check);
        Fx::RadioButton({10, 95, 80, 20}, "Sine", wave, 0);
        Fx::RadioButton({10, 120, 80, 20}, "Square", wave, 1);
    };
    Click({20, 20});
    Frame(screen);
    CHECK(pressed);
    Click({20, 50});
    Frame(screen);
    CHECK(!pressed);
    CHECK(mute);
    Click({20, 50});
    Frame(screen);
    CHECK(!mute);
    Click({15, 80});
    Frame(screen);
    CHECK(check);
    Click({15, 125});
    Frame(screen);
    CHECK_EQ(wave, 1);
    // The keyboard: the focus is on the radio button; Up and Enter press the check box again.
    Tap(Key::Up);
    Frame(screen);
    Tap(Key::Up);
    Frame(screen);
    Tap(Key::Enter);
    Frame(screen);
    CHECK(!check);
    // Disabled: no press.
    Click({20, 20});
    Frame([&] { pressed = Fx::Button(a, "Play", false); });
    CHECK(!pressed);
}

TEST(JFxSlidersDrag) {
    Fresh();
    const Rect area{10, 10, 120, 24};
    float value = 0;
    Detail::InputMouseMove({12, 20});
    Detail::InputMouseButton(MouseButton::Left, true);
    Frame([&] { Fx::Slider(area, value, 0, 10, 1); });
    Detail::InputMouseMove({200, 20}); // outside, still held: the end
    Frame([&] { Fx::Slider(area, value, 0, 10, 1); });
    CHECK_EQ(value, 10.0f);
    Detail::InputMouseButton(MouseButton::Left, false);
    Frame([&] { Fx::Slider(area, value, 0, 10, 1); });
    Tap(Key::Left);
    Frame([&] { Fx::Slider(area, value, 0, 10, 1); });
    CHECK_EQ(value, 9.0f);
}

TEST(JFxMenusOpenChooseAndClose) {
    Fresh();
    bool opened = false, chosen = false, under = false;
    auto screen = [&] {
        Fx::BeginMenuBar({0, 0, 320, 24});
        opened = Fx::Menu("File");
        if (opened) {
            chosen = Fx::MenuItem("Open", "Ctrl+O");
            Fx::MenuSeparator();
            Fx::MenuItem("Quit");
            Fx::EndMenu();
        }
        Fx::EndMenuBar();
        under = Fx::Button({0, 30, 200, 100}, "Under the menu");
    };
    Frame(screen);
    CHECK(!opened);
    Click({10, 10}); // the title
    Frame(screen);
    CHECK(opened);
    Frame(screen); // the menu knows its size from here on
    // A click on the item chooses it, closes the menu and does not reach the button under it.
    Click({20, 30 + 8});
    Frame(screen);
    CHECK(chosen);
    CHECK(!under);
    Frame(screen);
    CHECK(!opened);
    // A click outside only closes it.
    Click({10, 10});
    Frame(screen);
    CHECK(opened);
    Click({300, 120});
    Frame(screen);
    CHECK(!chosen);
    CHECK(!under);
    Frame(screen);
    CHECK(!opened);
    // The keyboard: Down and Enter choose the first item, Escape closes.
    Click({10, 10});
    Frame(screen);
    Tap(Key::Down);
    Frame(screen);
    Tap(Key::Enter);
    Frame(screen);
    CHECK(chosen);
    Click({10, 10});
    Frame(screen);
    Tap(Key::Escape);
    Frame(screen);
    CHECK(!UI::Pressed(Nav::Back)); // the menu took it
    Frame(screen);
    CHECK(!opened);
}

TEST(JFxMenusAreDrawnLastAndCheck) {
    Fresh();
    bool dark = false;
    auto screen = [&] {
        Fx::BeginMenuBar({0, 0, 320, 24});
        if (Fx::Menu("View")) {
            Fx::CheckMenuItem("Dark", dark);
            Fx::EndMenu();
        }
        Fx::EndMenuBar();
    };
    Click({10, 10});
    Frame(screen);
    Frame(screen);
    Click({20, 38});
    Frame(screen);
    CHECK(dark);
    // A menu that is not called any more closes.
    Click({10, 10});
    Frame(screen);
    CHECK(Detail::PopupOpen());
    Frame([] {});
    CHECK(!Detail::PopupOpen());
}

TEST(JFxContextMenusOpenWhereRightClicked) {
    Fresh();
    const Rect area{0, 0, 300, 160};
    bool open = false, copied = false;
    auto screen = [&] {
        open = Fx::BeginContextMenu(area);
        if (open) {
            copied = Fx::MenuItem("Copy");
            Fx::EndMenu();
        }
    };
    Click({100, 50});
    Frame(screen);
    CHECK(!open); // the left button does not open it
    Click({100, 50}, MouseButton::Right);
    Frame(screen);
    CHECK(open);
    Frame(screen);
    Click({110, 50 + 8});
    Frame(screen);
    CHECK(copied);
}

TEST(JFxComboBoxesChoose) {
    Fresh();
    const Rect area{10, 10, 120, 24};
    int wave = 0;
    bool changed = false;
    auto screen = [&] { changed = Fx::ComboBox(area, wave, {"Sine", "Square", "Saw"}); };
    Frame(screen);
    Tap(Key::Right); // without opening it
    Frame(screen);
    CHECK(changed);
    CHECK_EQ(wave, 1);
    Click({20, 20});
    Frame(screen);
    CHECK(Detail::PopupOpen());
    Tap(Key::Down);
    Frame(screen);
    Tap(Key::Enter);
    Frame(screen);
    CHECK(changed);
    CHECK_EQ(wave, 2);
    CHECK(!Detail::PopupOpen());
}

TEST(JFxListViewsSelectAndScroll) {
    Fresh();
    std::vector<std::string> items;
    for (int i = 0; i < 40; ++i) items.push_back("Item " + std::to_string(i));
    const Rect area{10, 10, 150, 100};
    int selected = 0, scroll = 0;
    Click({30, 12 + 30}); // the second row (rows are ~24 pixels with Font::Pixel)
    Frame([&] { Fx::ListView(area, selected, items, scroll); });
    CHECK(selected > 0);
    Detail::InputMouseMove({30, 50});
    Detail::InputMouseWheel(-2); // towards the player: down
    Frame([&] { Fx::ListView(area, selected, items, scroll); });
    CHECK_EQ(scroll, 6);
    Detail::InputMouseWheel(-100);
    Frame([&] { Fx::ListView(area, selected, items, scroll); });
    CHECK(scroll < 40); // never past the end
}

TEST(JFxSplitPanesDragTheirBar) {
    Fresh();
    const Rect area{0, 0, 300, 100};
    float ratio = 0.5f;
    Rect first, second;
    Frame([&] { Fx::SplitPane(area, ratio, first, second); });
    CHECK_EQ(first.X, 0.0f);
    CHECK(second.X > first.Right());
    CHECK_EQ(second.Right(), 300.0f);
    const float bar = first.Right() + 1;
    Detail::InputMouseMove({bar, 50});
    Detail::InputMouseButton(MouseButton::Left, true);
    Frame([&] { Fx::SplitPane(area, ratio, first, second); });
    Detail::InputMouseMove({bar - 60, 50});
    bool moved = false;
    Frame([&] { moved = Fx::SplitPane(area, ratio, first, second); });
    CHECK(moved);
    CHECK(ratio < 0.4f);
    Detail::InputMouseButton(MouseButton::Left, false);
    Frame([&] { Fx::SplitPane(area, ratio, first, second, Fx::Orientation::Vertical); });
    CHECK_EQ(first.Width, 300.0f);
    CHECK(second.Y > first.Bottom());
}

TEST(JFxTabsAndTextFields) {
    Fresh();
    int tab = 0;
    std::string title;
    Rect page;
    auto screen = [&] {
        page = Fx::TabPane({0, 0, 300, 200}, tab, {"Pattern", "Song"});
        if (tab == 1) Fx::TextField({page.X + 10, page.Y + 10, 200, 26}, {}, title, {.Placeholder = "Untitled"});
    };
    Frame(screen);
    CHECK(page.Y > 0);
    CHECK_EQ(tab, 0);
    // The second tab starts after the first one's text: click a little to its right.
    Click({80, 15});
    Frame(screen);
    CHECK_EQ(tab, 1);
    Click({page.X + 30, page.Y + 20});
    Frame(screen);
    CHECK(UI::Typing());
    Detail::InputText("Tema");
    Frame(screen);
    CHECK_EQ(title, "Tema");
    // The theme UI::TextField drew with is put back.
    CHECK_EQ(UI::GetTheme().Text.Color, UI::Theme{}.Text.Color);
}

TEST(JFxDialogsAreModal) {
    Fresh();
    bool behind = false, inside = false;
    Rect content;
    auto screen = [&] {
        behind = Fx::Button({0, 0, 60, 20}, "Behind");
        content = Fx::BeginDialog({50, 50, 200, 120}, "About");
        inside = Fx::DefaultButton({content.X, content.Y, 80, 24}, "Close");
        Fx::EndDialog();
    };
    Frame(screen);
    CHECK(content.Y > 50); // under the title
    Click({10, 10});
    Frame(screen);
    CHECK(!behind);
    Click({content.X + 5, content.Y + 5});
    Frame(screen);
    CHECK(inside);
}

namespace {

void Hold(Vec2 at) {
    Detail::InputMouseMove(at);
    Detail::InputMouseButton(MouseButton::Left, true);
}

void Let(Vec2 at) {
    Detail::InputMouseMove(at);
    Detail::InputMouseButton(MouseButton::Left, false);
}

} // namespace

TEST(JFxClipsCutWidgetsAndTheirClicks) {
    Fresh();
    bool pressed = false;
    auto screen = [&] {
        Screen::PushClip({0, 0, 100, 50});
        pressed = Fx::Button({0, 30, 100, 40}, "Half hidden");
        Screen::PopClip();
    };
    Click({50, 60}); // on the button, outside the clip
    Frame(screen);
    CHECK(!pressed);
    Click({50, 40});
    Frame(screen);
    CHECK(pressed);
    // Nothing of the button is queued below the clip.
    for (const Detail::QueuedSprite& s : Detail::SpriteQueue()) CHECK(s.Instance.Y + s.Instance.Height <= 50.0f);
}

TEST(JFxSubMenusOpenBesideTheirMenu) {
    Fresh();
    bool opened = false, inside = false, wav = false, plain = false;
    auto screen = [&] {
        Fx::BeginMenuBar({0, 0, 320, 24});
        opened = Fx::Menu("File");
        inside = false;
        if (opened) {
            plain = Fx::MenuItem("New");
            if (Fx::BeginSubMenu("Export")) {
                inside = true;
                CHECK(!Fx::BeginSubMenu("Deeper")); // one level only
                wav = Fx::MenuItem("WAV") || wav;
                Fx::MenuItem("MIDI");
                Fx::EndMenu();
            }
            Fx::EndMenu();
        }
        Fx::EndMenuBar();
    };
    Click({10, 10});
    Frame(screen);
    Frame(screen);
    CHECK(opened);
    CHECK(!inside);
    // Pointing at "Export" (the second row) opens its submenu; pointing back at "New" closes it.
    Detail::InputMouseMove({20, 60});
    Frame(screen);
    CHECK(inside);
    Detail::InputMouseMove({20, 38});
    Frame(screen);
    CHECK(!inside);
    CHECK(opened);
    // The keyboard: Down to "Export", Right into the submenu, Left back out, Right again and Enter.
    Tap(Key::Down);
    Frame(screen);
    CHECK(!inside);
    Tap(Key::Right);
    Frame(screen);
    CHECK(inside);
    CHECK(opened);
    Tap(Key::Left);
    Frame(screen);
    CHECK(!inside);
    CHECK(opened); // Left only left the submenu
    Tap(Key::Right);
    Frame(screen);
    Tap(Key::Enter);
    Frame(screen);
    CHECK(wav);
    CHECK(!plain);
    Frame(screen);
    CHECK(!opened); // choosing in the submenu closes everything
}

TEST(JFxSpinnersStep) {
    Fresh();
    int bpm = 120;
    float volume = 0.5f;
    bool changed = false;
    auto screen = [&] {
        changed = Fx::Spinner({0, 0, 100, 24}, bpm, 20, 125, 1);
        Fx::Spinner({0, 40, 100, 24}, volume, 0, 1, 0.05f, 2);
    };
    Frame(screen);
    Click({93, 4}); // the up arrow
    Frame(screen);
    CHECK(changed);
    CHECK_EQ(bpm, 121);
    Click({93, 20}); // the down arrow
    Frame(screen);
    CHECK_EQ(bpm, 120);
    Detail::InputMouseMove({40, 10});
    Detail::InputMouseWheel(3);
    Frame(screen);
    CHECK_EQ(bpm, 123);
    Detail::InputMouseWheel(30);
    Frame(screen);
    CHECK_EQ(bpm, 125); // kept within range
    // Dragging down: a step every 4 pixels.
    Hold({40, 12});
    Frame(screen);
    Detail::InputMouseMove({40, 21});
    Frame(screen);
    CHECK_EQ(bpm, 123);
    Let({40, 21});
    Frame(screen);
    Click({93, 44});
    Frame(screen);
    CHECK_NEAR(volume, 0.55f, 1e-5f);
}

TEST(JFxKnobsTurn) {
    Fresh();
    float pan = 0;
    auto screen = [&] { Fx::Knob({0, 0, 40, 40}, pan, -1, 1, 0.01f, 0.0f); };
    Frame(screen);
    // 150 pixels for the whole range: 75 up is half of it.
    Hold({20, 20});
    Frame(screen);
    Detail::InputMouseMove({20, -55});
    Frame(screen);
    CHECK_NEAR(pan, 1.0f, 1e-4f);
    Detail::InputMouseMove({20, 57.5f});
    Frame(screen);
    CHECK_NEAR(pan, -0.5f, 1e-4f);
    Let({20, 57.5f});
    Frame(screen);
    Detail::InputMouseMove({20, 20});
    Detail::InputMouseWheel(5);
    Frame(screen);
    CHECK_NEAR(pan, -0.45f, 1e-4f);
}

TEST(JFxScrollAreasMoveAndCut) {
    Fresh();
    Vec2 scroll{0, 0}, at{0, 0};
    Rect view;
    bool pressed = false;
    std::optional<Rect> clip;
    auto screen = [&] {
        at = Fx::BeginScrollArea({0, 0, 100, 100}, {400, 400}, scroll);
        view = Fx::ScrollView();
        clip = Screen::Clip();
        pressed = Fx::Button({at.X, at.Y + 80, 50, 40}, "Inside");
        Fx::EndScrollArea();
    };
    Frame(screen);
    CHECK(clip.has_value());
    if (clip) CHECK(*clip == view);
    CHECK(!Screen::Clip());
    CHECK(view.Width < 98.0f); // room for the bars
    CHECK(view.Height < 98.0f);
    CHECK_EQ(at.X, 1.0f);
    // The wheel scrolls down; with Shift across; with Ctrl not at all.
    Detail::InputMouseMove({50, 50});
    Detail::InputMouseWheel(-1);
    Frame(screen);
    CHECK(scroll.Y > 0);
    CHECK_EQ(scroll.X, 0.0f);
    const float down = scroll.Y;
    Detail::InputKey(Key::LeftShift, true);
    Detail::InputMouseWheel(-1);
    Frame(screen);
    Detail::InputKey(Key::LeftShift, false);
    CHECK_EQ(scroll.X, down);
    Detail::InputKey(Key::LeftCtrl, true);
    Detail::InputMouseWheel(-1);
    Frame(screen);
    Detail::InputKey(Key::LeftCtrl, false);
    CHECK_EQ(scroll.Y, down);
    Detail::InputMouseWheel(-100);
    Frame(screen);
    CHECK_EQ(scroll.Y, 400 - view.Height); // kept within range
    // Back to the top: the button is half outside the view, and does not answer there.
    scroll = {0, 0};
    Frame(screen);
    Click({10, view.Bottom() + 5});
    Frame(screen);
    CHECK(!pressed);
    Click({10, view.Bottom() - 3});
    Frame(screen);
    CHECK(pressed);
}

TEST(JFxGridsAnswerTheMouse) {
    Fresh();
    Fx::Grid grid{.Cell = {10, 10}, .Columns = 16, .Rows = 8, .MajorColumns = 4};
    Fx::GridInput in;
    Rect cell;
    auto screen = [&] {
        in = Fx::BeginGrid({0, 0, 200, 100}, grid);
        cell = Fx::GridCell(2, 1);
        Fx::GridSelection({1, 1}, {3, 2});
        Fx::EndGrid();
    };
    Frame(screen);
    CHECK(cell == Rect(21, 11, 10, 10));
    Hold({25, 15});
    Frame(screen);
    CHECK(in.Pressed == (Fx::GridPoint{2, 1}));
    CHECK(in.Dragging);
    Detail::InputMouseMove({55, 35});
    Frame(screen);
    CHECK(!in.Pressed.Valid());
    CHECK(in.Dragging);
    CHECK(in.DragFrom == (Fx::GridPoint{2, 1}));
    CHECK(in.Hovered == (Fx::GridPoint{5, 3}));
    Detail::InputMouseMove({500, 500}); // out of the grid: the edge
    Frame(screen);
    CHECK(in.Hovered == (Fx::GridPoint{15, 7}));
    Let({500, 500});
    Frame(screen);
    CHECK(in.Released);
    CHECK(!in.Dragging);
    Frame(screen);
    CHECK(!in.Released);
    CHECK(!in.Hovered.Valid());
    Click({35, 75}, MouseButton::Right);
    Frame(screen);
    CHECK(in.RightPressed == (Fx::GridPoint{3, 7}));
}

TEST(JFxDragAndDrop) {
    Fresh();
    bool dragging = false, dropped = false;
    std::string got, seen;
    auto screen = [&] {
        dragging = Fx::DragSource({0, 0, 50, 20}, "pattern:intro", "intro");
        dropped = Fx::DropTarget({100, 0, 50, 50}, got);
        seen = std::string(Fx::Dragged());
    };
    Hold({10, 10});
    Frame(screen);
    CHECK(!dragging);
    Detail::InputMouseMove({12, 10}); // less than 4 pixels: not yet
    Frame(screen);
    CHECK(!dragging);
    Detail::InputMouseMove({120, 20});
    Frame(screen);
    CHECK(dragging);
    CHECK_EQ(seen, std::string("pattern:intro"));
    Let({120, 20});
    Frame(screen);
    CHECK(dropped);
    CHECK_EQ(got, std::string("pattern:intro"));
    Frame(screen);
    CHECK(!dropped);
    CHECK_EQ(seen, std::string(""));
    // Let go anywhere else: nothing.
    got.clear();
    Hold({10, 10});
    Frame(screen);
    Detail::InputMouseMove({70, 80});
    Frame(screen);
    Let({70, 80});
    Frame(screen);
    CHECK(!dropped);
    Frame(screen);
    CHECK(got.empty());
    CHECK_EQ(seen, std::string(""));
}
