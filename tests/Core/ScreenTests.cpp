#include "Printing.h"
#include "Testing.h"

#include "Core/DisplayInternal.h"

using namespace BitMotor;

TEST(ScreenLetterboxKeepsTheShape) {
    // Same shape: fills the window.
    CHECK_EQ(Detail::Letterbox({1280, 720}, {320, 180}), Rect(0, 0, 1280, 720));
    // Wider window: bars on the left and right.
    CHECK_EQ(Detail::Letterbox({1000, 360}, {320, 180}), Rect(180, 0, 640, 360));
    // Taller window: bars on top and bottom.
    CHECK_EQ(Detail::Letterbox({640, 600}, {320, 180}), Rect(0, 120, 640, 360));
    // No logical size: the window itself.
    CHECK_EQ(Detail::Letterbox({800, 600}, {0, 0}), Rect(0, 0, 800, 600));
}

TEST(ScreenIntegerScalingUsesWholeSteps) {
    // 1000x700 fits 320x180 at 3.125: whole steps keep x3, centred on whole pixels.
    CHECK_EQ(Detail::Letterbox({1000, 700}, {320, 180}, true), Rect(20, 80, 960, 540));
    // Exactly x4.
    CHECK_EQ(Detail::Letterbox({1280, 720}, {320, 180}, true), Rect(0, 0, 1280, 720));
    // Odd leftovers round down instead of landing on half pixels.
    CHECK_EQ(Detail::Letterbox({641, 361}, {320, 180}, true), Rect(0, 0, 640, 360));
    // A window smaller than the game shrinks it as usual (there is no whole step below x1).
    CHECK_EQ(Detail::Letterbox({160, 90}, {320, 180}, true), Rect(0, 0, 160, 90));
}

TEST(ScreenWithoutWindowIsHarmless) {
    // Before Run opens the window nothing crashes.
    CHECK_EQ(Window::Size(), Vec2(0, 0));
    Screen::Clear(Colors::Red);
    Screen::Screenshot("nothing.png"); // only a warning
    Window::SetTitle("nothing");
    CHECK_EQ(Window::Title(), std::string());
    CHECK(!Window::IsFullscreen());
}
