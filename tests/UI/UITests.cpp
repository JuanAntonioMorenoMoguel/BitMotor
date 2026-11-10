#include "Printing.h"
#include "Testing.h"

#include "Core/InputInternal.h"
#include "Core/TimeInternal.h"
#include "Render/SpritesInternal.h"
#include "UI/UiInternal.h"

#include <functional>

using namespace BitMotor;

namespace {

// One frame of the loop as the UI sees it: input, the widgets (in Draw), the end of the frame.
void Frame(const std::function<void()>& draw, double seconds = 1.0 / 60) {
    Detail::BeginFrame(seconds);
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
}

void Tap(Key key) {
    Detail::InputKey(key, true);
    Detail::InputKey(key, false); // a quick tap still counts for one frame
}

void Click(Vec2 at) {
    Detail::InputMouseMove(at);
    Detail::InputMouseButton(MouseButton::Left, true);
    Detail::InputMouseButton(MouseButton::Left, false);
}

const Rect First{10, 10, 60, 12}, Second{10, 30, 60, 12};

} // namespace

TEST(ShapesScanPixelCentres) {
    const Vec2 square[4] = {{0, 0}, {4, 0}, {4, 2}, {0, 2}};
    std::vector<Detail::Span> spans = Detail::ScanPolygon(square);
    CHECK_EQ(spans.size(), 2u);
    CHECK(spans[0] == (Detail::Span{0, 4, 0}));
    CHECK(spans[1] == (Detail::Span{0, 4, 1}));
    // A right triangle: each row as wide as the hypotenuse at the pixel's centre.
    const Vec2 triangle[3] = {{0, 0}, {4, 4}, {0, 4}};
    spans = Detail::ScanPolygon(triangle);
    CHECK_EQ(spans.size(), 4u);
    CHECK(spans[0] == (Detail::Span{0, 1, 0}));
    CHECK(spans[3] == (Detail::Span{0, 4, 3}));
    // A concave "U": two spans on the rows of its arms.
    const Vec2 u[8] = {{0, 0}, {1, 0}, {1, 2}, {3, 2}, {3, 0}, {4, 0}, {4, 3}, {0, 3}};
    spans = Detail::ScanPolygon(u);
    CHECK_EQ(spans.size(), 5u);
    CHECK(spans[0] == (Detail::Span{0, 1, 0}));
    CHECK(spans[1] == (Detail::Span{3, 4, 0}));
    CHECK(Detail::ScanPolygon(std::span<const Vec2>(square, 2)).empty());
}

TEST(ShapesQueueWhitePixels) {
    Detail::ResetSprites();
    UI::Rect({1, 2, 30, 4}, Colors::Red);
    CHECK_EQ(Detail::SpriteQueue().size(), 1u);
    const Detail::SpriteInstance& rect = Detail::SpriteQueue()[0].Instance;
    CHECK_EQ(Vec2(rect.X, rect.Y), Vec2(1, 2));
    CHECK_EQ(Vec2(rect.Width, rect.Height), Vec2(30, 4));
    CHECK_EQ(rect.R, 1.0f);
    UI::Rect({0, 0, 0, 5}, Colors::Red);         // empty
    UI::Rect({0, 0, 5, 5}, Colors::Transparent); // invisible
    CHECK_EQ(Detail::SpriteQueue().size(), 1u);
    Detail::ResetSprites();
    UI::RectOutline({0, 0, 10, 10}, Colors::White);
    CHECK_EQ(Detail::SpriteQueue().size(), 4u);
    Detail::ResetSprites();
    UI::Circle({10, 10}, 5, Colors::White);
    CHECK_EQ(Detail::SpriteQueue().size(), 10u); // one row per pixel
    Detail::ResetSprites();
    UI::Line({0, 0}, {10, 10}, Colors::White, 2);
    CHECK_EQ(Detail::SpriteQueue().size(), 1u);
    CHECK_NEAR(Detail::SpriteQueue()[0].Instance.Rotation, Pi / 4, 1e-5f);
    Detail::ResetSprites();
    UI::Line({0, 5}, {10, 5}, Colors::White);
    CHECK_EQ(Detail::SpriteQueue()[0].Instance.Width, 10.0f);
    Detail::ResetSprites();
    UI::Triangle({0, 0}, {8, 0}, {0, 8}, Colors::White, Layer::World, 3);
    CHECK_EQ(Detail::SpriteQueue().size(), 8u);
    CHECK(Detail::SpriteQueue()[0].Where == Layer::World);
    Detail::ResetSprites();
}

TEST(ButtonsAnswerClicks) {
    Fresh();
    Click({20, 15});
    bool first = false, second = false;
    Frame([&] {
        first = UI::Button(First, "One");
        second = UI::Button(First, "Same place"); // the click was taken
    });
    CHECK(first);
    CHECK(!second);
    Frame([&] { first = UI::Button(First, "One"); });
    CHECK(!first); // one click, one press
    Click({20, 15});
    Frame([&] { first = UI::Button(First, "One", false); });
    CHECK(!first); // disabled
    Click({200, 200});
    Frame([&] { first = UI::Button(First, "One"); });
    CHECK(!first);
}

TEST(NavigationMovesTheFocus) {
    Fresh();
    int clicks = 0;
    std::string pressed;
    auto menu = [&] {
        if (UI::Button(First, "One")) pressed += "1";
        if (UI::Button(Second, "Two")) pressed += "2";
        UI::Button({0, 0, 0, 0}, "Off", false); // disabled ones are skipped
    };
    UI::OnClick([&] { ++clicks; });
    Frame(menu);
    CHECK_EQ(UI::Focus(), 0);
    Tap(Key::Down);
    Frame(menu);
    CHECK_EQ(UI::Focus(), 1);
    Tap(Key::Enter);
    Frame(menu);
    CHECK_EQ(pressed, "2");
    Tap(Key::Down); // wraps around
    Frame(menu);
    CHECK_EQ(UI::Focus(), 0);
    Tap(Key::Up);
    Frame(menu);
    CHECK_EQ(UI::Focus(), 1);
    // The mouse moving over a widget takes the focus.
    Detail::InputMouseMove({20, 15});
    Frame(menu);
    CHECK_EQ(UI::Focus(), 0);
    CHECK_EQ(clicks, 1);
}

TEST(NavigationRepeatsWhileHeld) {
    Fresh();
    Detail::InputKey(Key::Down, true);
    int fired = 0;
    for (int i = 0; i < 60; ++i) // one second held, at 60 frames per second
        Frame([&] { fired += UI::Pressed(Nav::Down) ? 1 : 0; });
    // The press, then after 0.35 s once every 0.08 s: 1 + 1 + 8.
    CHECK(fired >= 9 && fired <= 10);
    Detail::InputKey(Key::Down, false);
    Frame([&] { fired = UI::Pressed(Nav::Down) ? 1 : 0; });
    CHECK_EQ(fired, 0);
    // Confirm and Back never repeat.
    Detail::InputKey(Key::Enter, true);
    fired = 0;
    for (int i = 0; i < 60; ++i) Frame([&] { fired += UI::Pressed(Nav::Confirm) ? 1 : 0; });
    CHECK_EQ(fired, 1);
}

TEST(NavigationCanUseTheGamesActions) {
    Fresh();
    Input::Bind(7, "accept", {Key::J});
    UI::SetNavigation(Nav::Confirm, 7);
    UI::SetNavigation(Nav::Back, {Key::Q});
    bool confirm = false, back = false;
    Tap(Key::Enter);
    Frame([&] { confirm = UI::Pressed(Nav::Confirm); });
    CHECK(!confirm);
    Tap(Key::J);
    Tap(Key::Q);
    Frame([&] {
        confirm = UI::Pressed(Nav::Confirm);
        back = UI::Pressed(Nav::Back);
    });
    CHECK(confirm);
    CHECK(back);
}

TEST(TogglesAndSlidersChange) {
    Fresh();
    bool on = false;
    float volume = 0.5f;
    bool changed = false;
    auto options = [&] {
        changed = UI::Toggle(First, "Fullscreen", on);
        changed = UI::Slider(Second, "Volume", volume, 0, 1, 0.1f) || changed;
    };
    Click({20, 15});
    Frame(options);
    CHECK(on);
    CHECK(changed);
    Tap(Key::Right); // the focus is on the toggle: Right switches it
    Frame(options);
    CHECK(!on);
    Tap(Key::Down);
    Frame(options);
    Tap(Key::Right);
    Frame(options);
    CHECK_NEAR(volume, 0.6f, 1e-5f);
    for (int i = 0; i < 10; ++i) {
        Tap(Key::Right);
        Frame(options);
    }
    CHECK_EQ(volume, 1.0f); // clamped
    // Dragging: the track is the right half of the area, minus the padding.
    Detail::InputMouseMove({40, 35});
    Detail::InputMouseButton(MouseButton::Left, true);
    Frame(options);
    CHECK_NEAR(volume, 0.0f, 1e-5f);
    Detail::InputMouseMove({51, 35});
    Frame(options);
    CHECK_NEAR(volume, 0.4f, 1e-5f); // 11 of 26 pixels, to the nearest step
    Detail::InputMouseButton(MouseButton::Left, false);
    Frame(options);
    Detail::InputMouseMove({60, 35});
    Frame(options);
    CHECK_NEAR(volume, 0.4f, 1e-5f); // released: the mouse no longer moves it
}

TEST(ModalWindowsTakeTheInput) {
    Fresh();
    bool behind = false, inside = false;
    const Rect window{0, 50, 100, 60}, ok{10, 60, 40, 12};
    auto screen = [&] {
        behind = UI::Button(First, "Behind");
        UI::BeginModal(window, "Sure?");
        inside = UI::Button(ok, "OK");
        UI::EndModal();
    };
    Frame(screen);
    Click({20, 15});
    Frame(screen);
    CHECK(!behind);
    Click({20, 65});
    Frame(screen);
    CHECK(inside);
    CHECK_EQ(UI::Focus(), 0); // the focus is the window's
    // Closed: the screen reacts again.
    Frame([&] { behind = UI::Button(First, "Behind"); });
    Click({20, 15});
    Frame([&] { behind = UI::Button(First, "Behind"); });
    CHECK(behind);
}

TEST(TooltipsFollowThePointedWidget) {
    Fresh();
    Detail::InputMouseMove({20, 15});
    std::size_t without = 0, with = 0;
    Frame([&] { UI::Button(First, "One"); });
    without = Detail::SpriteQueue().size();
    Frame([&] {
        UI::Button(First, "One");
        UI::Tooltip("Hello");
    });
    with = Detail::SpriteQueue().size();
    CHECK(with > without);
    Frame([&] {
        UI::Button(Second, "Two");
        UI::Tooltip("Hello"); // not pointed at: nothing
    });
    CHECK(Detail::SpriteQueue().size() < with);
    Fresh();
}

TEST(TintedWidgetsCarryTheirTooltip) {
    Fresh();
    Detail::InputMouseMove({20, 15});
    std::size_t plain = 0;
    Frame([&] { UI::FlatButton(First, "One", Colors::Gold, false); });
    plain = Detail::SpriteQueue().size();
    Frame([&] { UI::FlatButton(First, "One", Colors::Gold, false, "Not yet: no money"); });
    CHECK(Detail::SpriteQueue().size() > plain); // disabled, it still says why
    Frame([&] { UI::Button(Second, "Two", Colors::Gold, true, "Far away"); });
    const std::size_t away = Detail::SpriteQueue().size();
    Frame([&] { UI::Button(Second, "Two", Colors::Gold); });
    CHECK_EQ(Detail::SpriteQueue().size(), away); // not pointed at: no tooltip
    Frame([&] {
        if (UI::Hovered(First)) UI::ShowTooltip("Mine");
    });
    CHECK(Detail::SpriteQueue().size() > 0);
    Fresh();
}

TEST(KeyboardFocusCanBeTurnedOff) {
    Fresh();
    Detail::InputMouseMove({300, 300});
    UI::SetKeyboardFocus(false);
    bool pressed = false;
    Frame([&] { UI::Button(First, "One"); });
    Tap(Key::Enter);
    Frame([&] { pressed = UI::Button(First, "One"); });
    CHECK(!pressed);                  // Enter is the game's
    CHECK(UI::Pressed(Nav::Confirm)); // still readable
    UI::SetKeyboardFocus(true);
    Fresh();
}

TEST(NineSlicesKeepTheirCorners) {
    Fresh();
    Frame([&] { UI::NineSlice({10, 10, 40, 20}, Region{}, 4, Colors::Red); });
    CHECK_EQ(Detail::SpriteQueue().size(), 0u); // no image: nothing
    std::vector<std::uint8_t> pixels(12 * 12 * 4, 255);
    Region image{Texture::FromPixels(12, 12, pixels), {0, 0, 12, 12}, {0, 0}, 1};
    Frame([&] { UI::NineSlice({10, 10, 40, 20}, image, 4, Colors::Red); });
    CHECK_EQ(Detail::SpriteQueue().size(), 9u);
    bool corner = false;
    for (const Detail::QueuedSprite& s : Detail::SpriteQueue())
        if (s.Instance.X == 46 && s.Instance.Y == 26 && s.Instance.Width == 4 && s.Instance.Height == 4) corner = true;
    CHECK(corner); // the bottom right corner, at its own size
    Fresh();
}

TEST(UiIgnoresTheKeyThatFinishesARebind) {
    Fresh();
    Input::Bind(3, "jump", {Key::Space});
    bool pressed = false;
    Frame([&] { UI::Button(First, "Jump"); });
    Input::Rebind(3);
    Tap(Key::Enter); // becomes the new control, and must not press the focused button
    Frame([&] { pressed = UI::Button(First, "Jump"); });
    CHECK(!pressed);
    CHECK_EQ(Input::KeyName(3), "Enter");
    Fresh();
}

namespace {

// The colours of what the frame queued, in order.
std::vector<Color> QueuedColors() {
    std::vector<Color> colors;
    for (const Detail::QueuedSprite& s : Detail::SpriteQueue())
        colors.push_back({s.Instance.R, s.Instance.G, s.Instance.B, s.Instance.A});
    return colors;
}

bool Queued(const Color& color) {
    for (const Color& c : QueuedColors())
        if (c == color) return true;
    return false;
}

} // namespace

TEST(TintedWidgetsTakeTheirColour) {
    Fresh();
    const Color amber = Color::Hex(0xFFB447);
    Detail::InputMouseMove({200, 200});
    Frame([&] { UI::Button(First, "SCRAM", amber); });
    CHECK(Queued(amber)); // frame and label
    CHECK(!Queued(UI::GetTheme().TextHover));
    Detail::InputMouseMove({20, 15});
    Frame([&] { UI::Button(First, "SCRAM", amber); });
    CHECK(Queued(UI::GetTheme().TextHover)); // pointed at: the hover colour
    CHECK(Queued(amber.WithAlpha(0.18f)));   // and a faint fill inside
    Frame([&] { UI::Button(First, "SCRAM", amber, false); });
    CHECK(Queued(UI::GetTheme().Faint));
    CHECK(!Queued(amber));
    Frame([&] { UI::FlatButton(First, "+1000", amber); });
    CHECK(Queued(amber.WithAlpha(0.25f)));
    Detail::InputMouseMove({200, 200});
    Frame([&] { UI::FlatButton(First, "+1000", amber); });
    CHECK(Queued(amber.WithAlpha(0.7f))); // the outline, dimmer when not pointed at
}

TEST(TintedPanelsDrawTheirTitle) {
    Fresh();
    UI::Theme theme;
    theme.TitleTab = true;
    UI::SetTheme(theme);
    const Color green = Color::Hex(0x7DFF99);
    Frame([&] { UI::Panel({10, 10, 100, 50}, "REACTOR", green); });
    CHECK(Queued(green));
    CHECK(Queued(theme.TitleBack)); // the title, over the tab
    bool tab = false;
    for (const Detail::QueuedSprite& s : Detail::SpriteQueue())
        if (s.Instance.X == 15 && s.Instance.Y == 7 && s.Instance.Height == 9) tab = true;
    CHECK(tab);
    UI::SetTheme({});
}

TEST(DisabledWidgetsSwallowTheClick) {
    Fresh();
    bool below = false;
    Click({20, 15});
    Frame([&] {
        UI::Button(First, "Off", false);
        below = UI::Clicked(First);
    });
    CHECK(!below);
}

TEST(RightClicksAreTheirOwn) {
    Fresh();
    Detail::InputMouseMove({20, 15});
    Detail::InputMouseButton(MouseButton::Right, true);
    Detail::InputMouseButton(MouseButton::Right, false);
    bool left = true, right = false, again = true;
    Frame([&] {
        left = UI::Clicked(First);
        right = UI::RightClicked(First);
        again = UI::RightClicked(First);
    });
    CHECK(!left);
    CHECK(right);
    CHECK(!again); // taken
}

TEST(ModalsTheGameDrawsItself) {
    Fresh();
    bool behind = false, inside = false;
    const Rect ok{10, 60, 40, 12};
    auto screen = [&] {
        behind = UI::Button(First, "Behind");
        UI::BeginModal();
        inside = UI::Button(ok, "OK");
        UI::EndModal();
    };
    Frame(screen);
    Click({20, 15});
    Frame(screen);
    CHECK(!behind);
    const std::size_t drawn = Detail::SpriteQueue().size();
    Click({20, 65});
    Frame(screen);
    CHECK(inside);
    CHECK_EQ(Detail::SpriteQueue().size(), drawn); // no shade, no window: only the buttons
}

TEST(SegmentsCanHaveTheirOwnBack) {
    Fresh();
    const Color back = Color::Hex(0x10201A);
    Frame([&] { UI::Segments({0, 0, 15, 4}, 0.5f, Colors::Green, back); });
    CHECK_EQ(Detail::SpriteQueue().size(), 4u); // 3 px blocks with a 1 px gap
    CHECK(Queued(back));
    Frame([&] { UI::Bar({0, 0, 100, 4}, 0.001f, Colors::Green); });
    CHECK_EQ(Detail::SpriteQueue()[1].Instance.Width, 1.0f); // a sliver: not empty
}

TEST(LabelsSitInTheMiddleRow) {
    Fresh();
    // 7-pixel capitals in a 16-pixel button: 5 rows above them and 4 below (as nyiengilib did).
    Frame([&] { UI::Button({0, 0, 60, 16}, "I", Colors::White); });
    float top = 1e9f;
    for (const Detail::QueuedSprite& s : Detail::SpriteQueue())
        if (s.Instance.Height == 11) top = std::min(top, s.Instance.Y);
    CHECK_EQ(top, 5.0f - 2.0f); // the glyph's image starts 2 rows above its capitals
}

TEST(ForcedMouseDrivesTheUi) {
    Fresh();
    Input::ForceMouse({20, 15});
    CHECK(Input::MouseForced());
    CHECK_EQ(Input::Mouse(), Vec2(20, 15));
    Input::ForceMouseButton(MouseButton::Left, true);
    Input::ForceMouseButton(MouseButton::Left, false);
    bool pressed = false;
    Frame([&] { pressed = UI::Button(First, "Bot"); });
    CHECK(pressed);
    Input::ReleaseMouse();
    CHECK(!Input::MouseForced());
}

TEST(DropdownsOpenAndChoose) {
    Fresh();
    const Rect area{10, 10, 120, 12};
    int choice = 0;
    bool changed = false, button = false;
    auto menu = [&] {
        changed = UI::Dropdown(area, "Language", choice, {"Spanish", "English", "French"});
        button = UI::Button({10, 30, 120, 12}, "Under the list");
    };
    Frame(menu);
    // Left/Right go through the options while it is closed.
    Tap(Key::Right);
    Frame(menu);
    CHECK(changed);
    CHECK_EQ(choice, 1);
    Tap(Key::Left);
    Tap(Key::Left); // wraps around... once per frame: two taps in a frame are one press
    Frame(menu);
    CHECK_EQ(choice, 0);
    // A click on the box opens the list (rows of 11 pixels from y = 24, on the right half); the button under
    // it gets nothing.
    Click({100, 15});
    Frame(menu);
    CHECK(!changed);
    Click({100, 38}); // the second row, over the button
    Frame(menu);
    CHECK(changed);
    CHECK_EQ(choice, 1);
    CHECK(!button);
    // With the keyboard: Confirm opens, Down moves, Confirm chooses; the focus does not move meanwhile.
    Tap(Key::Enter);
    Frame(menu);
    Tap(Key::Down);
    Frame(menu);
    CHECK_EQ(UI::Focus(), 0);
    Tap(Key::Enter);
    Frame(menu);
    CHECK(changed);
    CHECK_EQ(choice, 2);
    // Back closes it without choosing, and UI::Pressed does not see the Back meanwhile.
    Tap(Key::Enter);
    Frame(menu);
    Tap(Key::Down);
    Frame(menu);
    Tap(Key::Escape);
    bool back = true;
    Frame([&] {
        menu();
        back = UI::Pressed(Nav::Back);
    });
    CHECK(!back);
    CHECK(!changed);
    CHECK_EQ(choice, 2);
    // Closed again: Down moves the focus to the button.
    Tap(Key::Down);
    Frame(menu);
    CHECK_EQ(UI::Focus(), 1);
    // A click outside closes it and does not reach what is under.
    Click({100, 15});
    Frame(menu);
    Click({20, 33});
    Frame(menu);
    CHECK(!button);
    CHECK_EQ(choice, 2);
    Click({20, 33});
    Frame(menu);
    CHECK(button);
}

TEST(TextFieldsTakeTyping) {
    Fresh();
    const Rect area{10, 10, 200, 12};
    std::string name = "Ana";
    bool changed = false, typing = false, nav = false;
    auto form = [&] {
        changed = UI::TextField(area, "Name", name, {.MaxLength = 6});
        typing = UI::Typing();
        nav = UI::Pressed(Nav::Down) || UI::Pressed(Nav::Confirm);
    };
    Frame(form);
    CHECK(!typing);
    // Confirm on the focused field starts typing at the end.
    Tap(Key::Enter);
    Frame(form);
    CHECK(typing);
    Detail::InputText("ís");
    Frame(form);
    CHECK(changed);
    CHECK_EQ(name, "Anaís");
    // Keys edit, and S (Down in the navigation) or Space reach neither the focus nor UI::Pressed.
    Tap(Key::Left);
    Tap(Key::Backspace);
    Detail::InputKey(Key::S, true);
    Detail::InputText("s");
    Frame(form);
    CHECK(!nav);
    CHECK_EQ(name, "Anass");
    Detail::InputKey(Key::S, false);
    // The limit, in characters (not bytes).
    Detail::InputText("xyz");
    Frame(form);
    CHECK_EQ(name, "Anasxs");
    Tap(Key::Home);
    Tap(Key::Delete);
    Frame(form);
    CHECK_EQ(name, "nasxs");
    // Auto-repeated keys count too.
    Detail::InputKeyRepeat(Key::Delete);
    Detail::InputKeyRepeat(Key::Delete);
    Frame(form);
    CHECK_EQ(name, "sxs");
    // Control characters are not typed; Enter finishes.
    Detail::InputText("\t");
    Tap(Key::Enter);
    Frame(form);
    CHECK(!typing);
    CHECK_EQ(name, "sxs");
    // A click inside starts typing; a click elsewhere stops it, and reaches what is there.
    Click({150, 15});
    Frame(form);
    CHECK(typing);
    bool button = false;
    Click({20, 40});
    Frame([&] {
        form();
        button = UI::Button({10, 35, 60, 12}, "Other");
    });
    CHECK(!typing);
    CHECK(button);
}

TEST(TextFieldsStopWhenTheyAreGone) {
    Fresh();
    std::string text;
    Frame([&] { UI::TextField({10, 10, 100, 12}, {}, text, {.Placeholder = "..."}); });
    Tap(Key::Enter);
    Frame([&] { UI::TextField({10, 10, 100, 12}, {}, text); });
    CHECK(UI::Typing());
    Frame([] {}); // its screen closed
    CHECK(!UI::Typing());
    // Disabled: no typing.
    Tap(Key::Enter);
    Frame([&] { UI::TextField({10, 10, 100, 12}, {}, text, {.Enabled = false}); });
    CHECK(!UI::Typing());
}
