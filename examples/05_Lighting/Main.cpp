// 05_Lighting: 2D lights with normal maps, glow, bloom and the screen effects, in a dark room.
// - The hero carries a flickering torch; three coloured wisps circle the room.
// - The atlas has a normal map (atlas_n.png): edges catch the light, the orbs glow by themselves.
// - Space: a hit (camera shake + Screen::Flash, which the player can turn off in Settings).
// - L: lighting on/off. B: bloom on/off. T: smooth or stepped light. V: screen effects on/off.
// - WASD / arrows / left stick walk. F12 saves a screenshot. With `--frames N` the hero walks by itself.
// It reuses the atlas of 04_Sprites.
#include <BitMotor/BitMotor.h>

#include <cmath>

using namespace BitMotor;

enum class Action { Left, Right, Up, Down, Hit };

constexpr Vec2 RoomSize = {320, 180};

struct LightingExample : Game {
    Atlas atlas;
    Animation idle, walk, orb;
    Camera2D camera;
    Random random{7};
    Vec2 hero = {160, 110};
    bool facingLeft = false, walking = false, autoWalk = false, effectsOn = true;
    float time = 0, lastTime = 0, flicker = 1;

    void Start() override {
        atlas = Atlas::Load("04_Sprites/Assets/atlas.json");
        idle = atlas.Animation("hero_idle");
        walk = atlas.Animation("hero_walk");
        orb = atlas.Animation("orb");
        Input::Bind(Action::Left, "left", {Key::A, Key::Left, Button::DpadLeft, Button::LeftStickLeft});
        Input::Bind(Action::Right, "right", {Key::D, Key::Right, Button::DpadRight, Button::LeftStickRight});
        Input::Bind(Action::Up, "up", {Key::W, Key::Up, Button::DpadUp, Button::LeftStickUp});
        Input::Bind(Action::Down, "down", {Key::S, Key::Down, Button::DpadDown, Button::LeftStickDown});
        Input::Bind(Action::Hit, "hit", {Key::Space, Button::South});
        autoWalk = Args::Has("--frames");
        camera.Position = RoomSize * 0.5f;

        // The look: a dark blue room, banded light (pixel art), bloom on the bright parts and a vignette.
        Lighting::Enable(true);
        Lighting::SetAmbient(Color::Hex(0x1A2038));
        Lighting::SetStepped(true);
        Lighting::SetBloom(0.7f);
        Screen::SetEffects({.Vignette = 0.4f, .Grain = 0.03f, .Aberration = 0.6f});
    }

    void Update() override {
        if (Input::KeyPressed(Key::Escape)) Quit();
        if (Input::KeyPressed(Key::F11)) Window::ToggleFullscreen();
        if (Input::KeyPressed(Key::F12)) Screen::Screenshot();
        if (Input::KeyPressed(Key::L)) Lighting::Enable(!Lighting::Enabled());
        if (Input::KeyPressed(Key::B)) Lighting::SetBloom(Lighting::Bloom() > 0 ? 0.0f : 0.7f);
        if (Input::KeyPressed(Key::T)) Lighting::SetStepped(!Lighting::Stepped());
        if (Input::KeyPressed(Key::V)) {
            effectsOn = !effectsOn;
            Screen::SetEffects(effectsOn ? ScreenEffects{.Vignette = 0.4f, .Grain = 0.03f, .Aberration = 0.6f}
                                         : ScreenEffects{});
        }
        lastTime = time;
        time += Time::Delta();

        Vec2 move = Input::Axis(Action::Left, Action::Right, Action::Up, Action::Down);
        if (autoWalk) move = {std::cos(time * 0.8f), std::sin(time * 1.1f) * 0.7f};
        hero.X = Clamp(hero.X + move.X * 60 * Time::Delta(), 16.0f, RoomSize.X - 16);
        hero.Y = Clamp(hero.Y + move.Y * 60 * Time::Delta(), 30.0f, RoomSize.Y - 8);
        walking = Length(move) > 0.1f;
        if (std::abs(move.X) > 0.1f) facingLeft = move.X < 0;

        // A torch flickers: a few random steps per second, smoothed.
        flicker = Damp(flicker, random.Range(0.75f, 1.1f), 12, Time::Delta());

        bool demoHit = autoWalk && std::floor(time / 2.0f) != std::floor(lastTime / 2.0f);
        if (Input::Pressed(Action::Hit) || demoHit) {
            camera.AddShake(0.5f);
            Screen::Flash(Color::Hex(0xFFE0C0), 0.12f);
        }
        camera.Update();
    }

    Vec2 Wisp(int i) const {
        float angle = time * 0.6f + float(i) * Tau / 3;
        return RoomSize * 0.5f + Vec2(std::cos(angle) * 110, std::sin(angle) * 55);
    }

    void Draw() override {
        Screen::Clear(Colors::Black);
        SetCamera(camera);

        // The floor and the columns are World sprites: they are lit.
        Region tile = atlas.Sprite("tile");
        for (float y = 8; y < RoomSize.Y; y += 16)
            for (float x = 8; x < RoomSize.X; x += 16)
                Screen::Draw(Sprite2D{.Region = tile, .Position = {x, y}, .Depth = -1000});
        Region column = atlas.Sprite("column");
        for (Vec2 at : {Vec2(60, 60), Vec2(260, 60), Vec2(60, 150), Vec2(260, 150), Vec2(160, 40)})
            Screen::Draw(Sprite2D{.Region = column, .Position = at, .Depth = at.Y});

        Screen::Draw(Sprite2D{.Region = atlas.Sprite("shadow"), .Position = hero + Vec2(0, 1), .Depth = hero.Y - 0.5f});
        Screen::Draw(Sprite2D{
            .Region = (walking ? walk : idle).Frame(time), .Position = hero, .Flip = facingLeft, .Depth = hero.Y});

        // The torch, a little in front of the hero and above the floor.
        Vec2 torch = hero + Vec2(facingLeft ? -5.0f : 5.0f, -9);
        Screen::Draw(Light2D{.Position = torch,
                             .Height = 18,
                             .Radius = 110 * flicker,
                             .Color = Color::Hex(0xFFB060),
                             .Intensity = 1.6f * flicker});
        Screen::Draw(Sprite2D{.Region = atlas.Sprite("glow"),
                              .Position = torch,
                              .Scale = {1.2f, 1.2f},
                              .Color = Color::Hex(0xFFB060).WithAlpha(0.8f),
                              .Layer = Layer::Glow});

        // Three wisps: an emissive orb (it glows in the normal map), its light and a halo in the Glow layer.
        const Color colours[3] = {Color::Hex(0x60A0FF), Color::Hex(0xFF5080), Color::Hex(0x70FF90)};
        for (int i = 0; i < 3; ++i) {
            Vec2 at = Wisp(i);
            Screen::Draw(Sprite2D{.Region = orb.Frame(time + float(i)),
                                  .Position = at,
                                  .Scale = {0.6f, 0.6f},
                                  .Color = colours[i],
                                  .Depth = at.Y + 20});
            Screen::Draw(Light2D{.Position = at, .Height = 30, .Radius = 80, .Color = colours[i], .Intensity = 1.2f});
            Screen::Draw(Sprite2D{.Region = atlas.Sprite("glow"),
                                  .Position = at,
                                  .Scale = {2.5f, 2.5f},
                                  .Color = colours[i].WithAlpha(0.5f),
                                  .Layer = Layer::Glow});
        }

        // The Hud is never lit, never blurred by the effects and stays over the flash.
        Region coin = atlas.Sprite("coin_0");
        for (int i = 0; i < 3; ++i)
            Screen::Draw(Sprite2D{.Region = coin, .Position = {10.0f + 10 * float(i), 10}, .Layer = Layer::Hud});
    }
};

int main() {
    return Run<LightingExample>({
        .Title = "BitMotor 05 - Lighting",
        .Width = 320,
        .Height = 180,
        .WindowWidth = 1280,
        .WindowHeight = 720,
    });
}
