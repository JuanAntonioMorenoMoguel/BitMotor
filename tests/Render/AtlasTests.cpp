#include "Printing.h"
#include "Testing.h"

#include "BitMotor/Math/Functions.h"
#include "BitMotor/Render/Atlas.h"

using namespace BitMotor;

namespace {

Texture Blank() {
    std::vector<std::uint8_t> pixels(64 * 64 * 4, 255);
    return Texture::FromPixels(64, 64, pixels);
}

// walk: 3 frames, the middle one held for 2 ticks, "step" on frames 0 and 2. open: one-shot.
constexpr const char* Json = R"({
  "filter": "nearest",
  "sprites": {
    "coin": [0, 0, 8, 8, 4, 4],
    "walk_2": [32, 0, 16, 16, 8, 16],
    "walk_0": [0, 16, 16, 16, 8, 16],
    "walk_1": [16, 16, 16, 16, 8, 16],
    "walk_10": [48, 48, 16, 16, 8, 16],
    "big": [0, 32, 32, 32, 16, 32, 2],
    "open_0": [0, 0, 4, 4, 0, 0],
    "open_1": [4, 0, 4, 4, 0, 0]
  },
  "animations": {
    "walk": {"fps": 10, "durations": [1, 2, 1], "events": {"step": [0, 2]}},
    "open": {"fps": 4, "loop": false}
  },
  "sockets": {"walk_1": {"hand": [6, -10, 90]}, "big": {"tip": [8, 0]}}
})";

} // namespace

TEST(AtlasReadsSprites) {
    Atlas atlas = Atlas::Parse(Json, Blank(), "test");
    CHECK(bool(atlas));
    CHECK(atlas.Has("coin"));
    CHECK(!atlas.Has("nope"));
    Region coin = atlas.Sprite("coin");
    CHECK(bool(coin));
    CHECK_EQ(coin.Area, Rect(0, 0, 8, 8));
    CHECK_EQ(coin.Pivot, Vec2(4, 4));
    CHECK_EQ(coin.Size(), Vec2(8, 8));
    // Density 2: drawn at half the texture size.
    CHECK_EQ(atlas.Sprite("big").Size(), Vec2(16, 16));
    CHECK(!atlas.Sprite("nope")); // warns once, draws nothing
    CHECK_EQ(atlas.Names().size(), std::size_t(8));
}

TEST(AtlasRejectsBrokenJson) {
    CHECK(!Atlas::Parse("{ not json", Blank()));
    CHECK(!Atlas::Parse(R"({"frames": {}})", Blank()));
    CHECK(!Atlas().Sprite("coin"));
}

TEST(AnimationGathersNumberedFrames) {
    Atlas atlas = Atlas::Parse(Json, Blank(), "test");
    Animation walk = atlas.Animation("walk");
    // walk_0..walk_2 in numeric order; walk_10 is not consecutive, so it is not part of it.
    CHECK_EQ(walk.Count(), 3);
    CHECK_EQ(walk.Fps(), 10.0f);
    CHECK(walk.Loops());
    CHECK_EQ(walk.Frame(0).Area, Rect(0, 16, 16, 16));
    // Single sprites work as one-frame animations.
    CHECK_EQ(atlas.Animation("coin").Count(), 1);
    CHECK(!atlas.Animation("nope"));
}

TEST(AnimationTimingFollowsTheDurations) {
    Animation walk = Atlas::Parse(Json, Blank(), "test").Animation("walk");
    // 4 ticks of 0.1 s: frame 0, frame 1 (two ticks), frame 2, then it loops.
    CHECK_NEAR(walk.Duration(), 0.4f, 1e-5f);
    CHECK_EQ(walk.FrameIndex(0.05f), 0);
    CHECK_EQ(walk.FrameIndex(0.15f), 1);
    CHECK_EQ(walk.FrameIndex(0.25f), 1);
    CHECK_EQ(walk.FrameIndex(0.35f), 2);
    CHECK_EQ(walk.FrameIndex(0.45f), 0);
    CHECK_EQ(walk.FrameIndex(0.45f, false), 2); // forced to play once: stays on the last
    CHECK(!walk.Done(10));
}

TEST(AnimationOnceStopsAtTheEnd) {
    Animation open = Atlas::Parse(Json, Blank(), "test").Animation("open");
    CHECK(!open.Loops());
    CHECK_NEAR(open.Duration(), 0.5f, 1e-5f);
    CHECK_EQ(open.FrameIndex(3.0f), 1);
    CHECK(!open.Done(0.4f));
    CHECK(open.Done(0.5f));
}

TEST(AnimationEventsFireOncePerPass) {
    Animation walk = Atlas::Parse(Json, Blank(), "test").Animation("walk");
    // "step" at frame 0 (0 s) and frame 2 (0.3 s), every 0.4 s.
    CHECK(walk.Fired("step", 0.0f, 0.01f));
    CHECK(!walk.Fired("step", 0.01f, 0.29f));
    CHECK(walk.Fired("step", 0.29f, 0.31f));
    CHECK(walk.Fired("step", 0.39f, 0.41f)); // the next pass
    CHECK(!walk.Fired("step", 1.12f, 1.18f));
    CHECK(walk.Fired("step", 1.1f, 1.2f)); // 0.4 * 2 + 0.3
    CHECK(!walk.Fired("jump", 0, 10));
    // Stepping through time in small pieces sees each event exactly once.
    int steps = 0;
    for (float t = 0; t < 0.8f - 1e-4f; t += 0.01f) steps += walk.Fired("step", t, t + 0.01f) ? 1 : 0;
    CHECK_EQ(steps, 4);
}

TEST(AtlasSocketsBelongToFrames) {
    Atlas atlas = Atlas::Parse(Json, Blank(), "test");
    Animation walk = atlas.Animation("walk");
    auto hand = atlas.Socket(walk.Frame(0.15f), "hand");
    CHECK(hand.has_value());
    CHECK_EQ(hand->Offset, Vec2(6, -10));
    CHECK_NEAR(hand->Angle, Pi / 2, 1e-5f);
    CHECK(!atlas.Socket(walk.Frame(0.0f), "hand"));
    CHECK(!atlas.Socket(walk.Frame(0.15f), "foot"));
    // Offsets are in screen pixels: density 2 halves them.
    CHECK_EQ(atlas.Socket(atlas.Sprite("big"), "tip")->Offset, Vec2(4, 0));
    // A region from another texture never matches.
    CHECK(!atlas.Socket(Blank().Full(), "hand"));
}
