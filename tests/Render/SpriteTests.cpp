#include "Printing.h"
#include "Testing.h"

#include "BitMotor/Core/Time.h"
#include "BitMotor/Math/Functions.h"
#include "BitMotor/Render/RenderTexture.h"
#include "Core/TimeInternal.h"
#include "Render/RendererInternal.h"
#include "Render/SpritesInternal.h"

using namespace BitMotor;

namespace {

Texture Solid(int size) {
    std::vector<std::uint8_t> pixels(std::size_t(size * size * 4), 200);
    return Texture::FromPixels(size, size, pixels);
}

bool Near(Vec2 a, Vec2 b) { return Distance(a, b) < 1e-3f; }

} // namespace

TEST(CameraCentresItsPosition) {
    // Without a window the screen is 0x0, so its centre is the origin: positions are relative to it.
    Camera2D camera;
    camera.Position = {100, 50};
    CHECK(Near(camera.ToScreen({100, 50}), Screen::Center()));
    CHECK(Near(camera.ToScreen({110, 50}) - camera.ToScreen({100, 50}), {10, 0}));
    camera.Zoom = 2;
    CHECK(Near(camera.ToScreen({110, 50}) - camera.ToScreen({100, 50}), {20, 0}));
    // Turning the camera clockwise makes the world turn the other way.
    camera.Zoom = 1;
    camera.Rotation = Pi / 2;
    CHECK(Near(camera.ToScreen({110, 50}) - camera.ToScreen({100, 50}), {0, -10}));
}

TEST(CameraToWorldUndoesToScreen) {
    Camera2D camera;
    camera.Position = {-30, 12};
    camera.Zoom = 3;
    camera.Rotation = 0.7f;
    camera.Trauma = 0.6f;
    camera.ShakeTime = 4.2f;
    for (Vec2 world : {Vec2(0, 0), Vec2(55, -20), Vec2(-300, 400)})
        CHECK(Near(camera.ToWorld(camera.ToScreen(world)), world));
}

TEST(CameraShakeGrowsAndFades) {
    Detail::ResetTime(60);
    Camera2D camera;
    CHECK_EQ(camera.ShakeOffset(), Vec2(0, 0));
    camera.AddShake(0.5f);
    camera.AddShake(0.8f);
    CHECK_EQ(camera.Trauma, 1.0f); // capped
    camera.ShakeTime = 1.3f;
    CHECK(Length(camera.ShakeOffset()) > 0);
    CHECK(Length(camera.ShakeOffset()) <= camera.MaxShake * 1.5f);
    // Same state, same shake: deterministic.
    Camera2D copy = camera;
    CHECK_EQ(copy.ShakeOffset(), camera.ShakeOffset());
    // 1.5 per second: gone in 0.67 s (45 updates at 60 Hz leave some margin).
    for (int i = 0; i < 45; ++i) {
        Detail::BeginUpdate();
        camera.Update();
    }
    CHECK_EQ(camera.Trauma, 0.0f);
    CHECK_EQ(camera.ShakeOffset(), Vec2(0, 0));
    CHECK_EQ(camera.ShakeAngle(), 0.0f);
}

TEST(SpriteInstanceUsesRegionAndOptions) {
    Texture texture = Solid(32);
    Sprite2D sprite{.Region = Region{texture, Rect(8, 0, 16, 8), {4, 8}}, .Position = {50, 60}, .Scale = {2, 1}};
    sprite.Flip = true;
    sprite.Flash = 3; // clamped
    sprite.Color = Colors::Red;
    Detail::SpriteInstance s = Detail::MakeInstance(sprite, nullptr);
    CHECK_EQ(s.X, 50.0f);
    CHECK_EQ(s.Y, 60.0f);
    CHECK_EQ(s.Width, -32.0f); // flipped
    CHECK_EQ(s.Height, 8.0f);
    CHECK_EQ(s.U, 8.0f);
    CHECK_EQ(s.UWidth, 16.0f);
    CHECK_EQ(s.PivotX, 0.25f);
    CHECK_EQ(s.PivotY, 1.0f);
    CHECK_EQ(s.Flash, 1.0f);
    CHECK_EQ(s.G, 0.0f);
    sprite.Origin = Vec2(0.5f, 0.5f);
    CHECK_EQ(Detail::MakeInstance(sprite, nullptr).PivotX, 0.5f);
}

TEST(SpriteInstanceFollowsTheCamera) {
    Texture texture = Solid(8);
    Camera2D camera;
    camera.Position = {100, 100};
    camera.Zoom = 2;
    camera.Rotation = 0.5f;
    Sprite2D sprite{.Region = texture.Full(), .Position = {110, 100}, .Rotation = 0.25f};
    Detail::SpriteInstance s = Detail::MakeInstance(sprite, &camera);
    CHECK(Near({s.X, s.Y}, camera.ToScreen({110, 100})));
    CHECK_EQ(s.Width, 16.0f);
    CHECK_NEAR(s.Rotation, -0.25f, 1e-6f);
}

TEST(SpritesSortByLayerAndDepth) {
    Texture a = Solid(4), b = Solid(4);
    auto queued = [](Texture t, Layer layer, float depth, float x) {
        Detail::QueuedSprite q;
        q.Source = t;
        q.Where = layer;
        q.Depth = depth;
        q.Instance.X = x;
        return q;
    };
    std::vector<Detail::QueuedSprite> queue = {
        queued(a, Layer::Hud, 0, 1),        queued(a, Layer::World, 5, 2),
        queued(b, Layer::World, 1, 3),      queued(b, Layer::World, 5, 4),
        queued(a, Layer::Background, 9, 5), queued(a, Layer::Overlay, -100, 6), // depth only orders World
        queued(a, Layer::Overlay, 100, 7),
    };
    std::vector<Detail::SpriteInstance> instances;
    std::vector<Detail::SpriteBatch> batches;
    Detail::BuildSprites(queue, instances, batches);
    std::vector<float> order;
    for (auto& s : instances) order.push_back(s.X);
    // Background, World by depth (equal depths keep their order), Overlay in draw order, Hud.
    CHECK(order == std::vector<float>({5, 3, 2, 4, 6, 7, 1}));
    // Batches change with the texture or the layer: bg(a) | b | a | b | overlay(a) | hud(a).
    CHECK_EQ(batches.size(), std::size_t(6));
    CHECK_EQ(batches[4].Count, 2u);
    CHECK_EQ(batches[4].First, 4u);
    CHECK(batches[1].Source == b);
}

TEST(DrawQueuesUntilTheFrameResets) {
    Detail::ResetSprites();
    Texture texture = Solid(4);
    Screen::Draw(Sprite2D{.Region = texture.Full()});
    Screen::Draw(Sprite2D{}); // empty region: nothing
    Screen::Draw(Sprite2D{.Region = texture.Full(), .Layer = Layer::Hud});
    CHECK_EQ(Detail::SpriteQueue().size(), std::size_t(2));
    Camera2D camera;
    camera.Zoom = 4;
    SetCamera(camera);
    CHECK_EQ(CurrentCamera().Zoom, 4.0f);
    Detail::ResetSprites();
    CHECK(Detail::SpriteQueue().empty());
    CHECK_EQ(CurrentCamera().Zoom, 1.0f);
}

TEST(RenderTexturesTakeTheirOwnDraws) {
    Detail::ResetSprites();
    RenderTexture minimap = RenderTexture::Create(64, 32);
    CHECK(minimap);
    CHECK_EQ(minimap.Size(), Vec2(64, 32));
    CHECK(minimap.Source().Internal()->Target);
    CHECK(!RenderTexture::Create(0, 10));
    Texture dot = Solid(2);
    Screen::Draw(Sprite2D{.Region = dot.Full(), .Position = {1, 1}, .Layer = Layer::Hud});
    minimap.Begin(Colors::Black);
    // Inside: the camera is centred on the texture, so the World lines up with its pixels.
    CHECK(Near(CurrentCamera().ToScreen({5, 6}), {5, 6}));
    Screen::Draw(Sprite2D{.Region = dot.Full(), .Position = {5, 6}});
    Screen::Draw(Sprite2D{.Region = dot.Full(), .Position = {7, 8}, .Layer = Layer::Hud});
    RenderTexture other = RenderTexture::Create(8, 8);
    other.Begin(); // one at a time: refused
    CHECK_EQ(Detail::TargetDraws().size(), 1u);
    minimap.End();
    CHECK_EQ(Detail::SpriteQueue().size(), 1u); // the screen kept its own
    Screen::Draw(Sprite2D{.Region = dot.Full(), .Position = {2, 2}, .Layer = Layer::Hud});
    CHECK_EQ(Detail::SpriteQueue().size(), 2u);
    const Detail::TargetDraw& draw = Detail::TargetDraws()[0];
    CHECK(draw.Target == minimap.Source());
    CHECK(draw.Clear);
    CHECK_EQ(draw.ClearColor, Colors::Black);
    CHECK_EQ(draw.Queue.size(), 2u);
    CHECK(Near({draw.Queue[0].Instance.X, draw.Queue[0].Instance.Y}, {5, 6}));
    // Continue keeps the image; Screen::Clear inside clears the texture, not the screen.
    minimap.Continue();
    Screen::Draw(Sprite2D{.Region = dot.Full(), .Position = {1, 1}});
    CHECK(!Detail::TargetDraws()[1].Clear);
    Screen::Clear(Colors::Navy);
    CHECK(Detail::TargetDraws()[1].Clear);
    CHECK_EQ(Detail::TargetDraws()[1].ClearColor, Colors::Navy);
    minimap.End();
    CHECK(Detail::TargetDraws()[1].Queue.empty());
    CHECK_EQ(Detail::SpriteQueue().size(), 2u);
    // The sprite of the texture itself draws like any other.
    Screen::Draw(Sprite2D{.Region = minimap.Full(), .Position = {200, 10}, .Layer = Layer::Hud});
    CHECK_EQ(Detail::SpriteQueue().size(), 3u);
    Detail::ResetSprites();
    CHECK(Detail::TargetDraws().empty());
}

TEST(HudSpritesAreClipped) {
    Detail::ResetSprites();
    Texture texture = Solid(4);
    auto queued = [] { return Detail::SpriteQueue().back().Instance; };
    // 40x40 on the screen from a 4x4 texture.
    Sprite2D sprite{.Region = texture.Full(), .Position = {0, 0}, .Scale = {10, 10}, .Layer = Layer::Hud};
    CHECK(!Screen::Clip());
    Screen::PushClip({10, 10, 20, 40});
    Screen::Draw(sprite);
    CHECK_EQ(Detail::SpriteQueue().size(), std::size_t(1));
    Detail::SpriteInstance s = queued();
    CHECK_EQ(s.X, 10.0f);
    CHECK_EQ(s.Y, 10.0f);
    CHECK_EQ(s.Width, 20.0f);
    CHECK_EQ(s.Height, 30.0f);
    CHECK_EQ(s.U, 1.0f); // a quarter of the way in
    CHECK_EQ(s.UWidth, 2.0f);
    CHECK_EQ(s.V, 1.0f);
    CHECK_EQ(s.VHeight, 3.0f);
    CHECK_EQ(s.PivotX, 0.0f);
    // Nested clips are inside each other; wholly outside is left out.
    Screen::PushClip({0, 0, 15, 15});
    CHECK(*Screen::Clip() == Rect(10, 10, 5, 5));
    Screen::Draw(sprite);
    CHECK_EQ(queued().Width, 5.0f);
    Screen::Draw(Sprite2D{.Region = texture.Full(), .Position = {100, 100}, .Layer = Layer::Hud});
    CHECK_EQ(Detail::SpriteQueue().size(), std::size_t(2)); // left out
    Screen::PopClip();
    // The World is not cut.
    Screen::Draw(Sprite2D{.Region = texture.Full(), .Position = {100, 100}});
    CHECK_EQ(Detail::SpriteQueue().size(), std::size_t(3));
    // A rotated sprite is kept whole when it touches the clip.
    sprite.Rotation = 0.5f;
    Screen::Draw(sprite);
    CHECK_EQ(queued().Width, 40.0f);
    Screen::PopClip();
    CHECK(!Screen::Clip());
    // Flipped (its pivot mirrors: at 40 it covers 0..40): the left of the screen shows the right of the texture.
    sprite.Rotation = 0;
    sprite.Flip = true;
    sprite.Position = {40, 0};
    Screen::PushClip({0, 0, 10, 40});
    Screen::Draw(sprite);
    s = queued();
    CHECK_EQ(s.X, 0.0f);
    CHECK_EQ(s.Width, -10.0f);
    CHECK_EQ(s.U, 3.0f);
    CHECK_EQ(s.UWidth, 1.0f);
    CHECK_EQ(s.PivotX, 1.0f);
    Screen::PopClip();
    Screen::PopClip(); // one too many: only a warning
    Detail::ResetSprites();
}
