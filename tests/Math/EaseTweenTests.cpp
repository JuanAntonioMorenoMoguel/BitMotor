#include "Printing.h"
#include "Testing.h"

using namespace BitMotor;

TEST(EaseCurvesStartAtZeroAndEndAtOne) {
    Ease::Function all[] = {Ease::Linear,    Ease::InSine,     Ease::OutSine,    Ease::InOutSine,    Ease::InQuad,
                            Ease::OutQuad,   Ease::InOutQuad,  Ease::InCubic,    Ease::OutCubic,     Ease::InOutCubic,
                            Ease::InExpo,    Ease::OutExpo,    Ease::InOutExpo,  Ease::InBack,       Ease::OutBack,
                            Ease::InOutBack, Ease::InElastic,  Ease::OutElastic, Ease::InOutElastic, Ease::InBounce,
                            Ease::OutBounce, Ease::InOutBounce};
    for (Ease::Function ease : all) {
        CHECK_NEAR(ease(0.0f), 0.0, 1e-4);
        CHECK_NEAR(ease(1.0f), 1.0, 1e-4);
        CHECK_NEAR(ease(-3.0f), 0.0, 1e-4); // clamped
        CHECK_NEAR(ease(7.0f), 1.0, 1e-4);
    }
    CHECK_NEAR(Ease::InQuad(0.5f), 0.25, 1e-6);
    CHECK_NEAR(Ease::OutQuad(0.5f), 0.75, 1e-6);
    CHECK_NEAR(Ease::InOutCubic(0.5f), 0.5, 1e-6);
    CHECK(Ease::OutBack(0.7f) > 1.0f); // overshoots on purpose
}

TEST(TweenGoesFromStartToEnd) {
    Tween<float> t{10, 20, 2.0f};
    CHECK_NEAR(t.Value(), 10, 1e-6);
    t.Update(1.0f);
    CHECK_NEAR(t.Progress(), 0.5, 1e-6);
    CHECK_NEAR(t.Value(), 15, 1e-6);
    CHECK(!t.Done());
    t.Update(5.0f);
    CHECK(t.Done());
    CHECK_NEAR(t.Value(), 20, 1e-6);
    t.Start();
    CHECK_NEAR(t.Value(), 10, 1e-6);
}

TEST(TweenWorksWithVectorsColorsAndRetarget) {
    Tween<Vec2> slide{{0, 0}, {100, 50}, 1.0f, Ease::InQuad};
    slide.Update(0.5f);
    CHECK_EQ(slide.Value(), Vec2(25, 12.5f));
    slide.Retarget({0, 0});
    CHECK_EQ(slide.Value(), Vec2(25, 12.5f)); // no jump
    slide.Update(1.0f);
    CHECK_EQ(slide.Value(), Vec2(0, 0));

    Tween<Color> fade{Colors::Black, Colors::White, 1.0f};
    fade.Update(0.5f);
    CHECK_NEAR(fade.Value().R, 0.5, 1e-6);

    Tween<float> instant{1, 2, 0.0f};
    CHECK(instant.Done());
    CHECK_NEAR(instant.Value(), 2, 1e-6);
}
