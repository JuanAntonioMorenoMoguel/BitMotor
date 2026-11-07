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
