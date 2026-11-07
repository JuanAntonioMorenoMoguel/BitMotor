// 04_Sprites: an atlas, animations, layers and a camera that follows the player.
// - WASD, the arrows or the left stick walk the hero around a room bigger than the screen.
// - World sprites are sorted by Depth (the y of their feet): the hero walks in front of and behind the columns.
// - Animations play as authored in atlas.json: the walk loops and sends "step" events, the chest opens once.
// - Space shakes the camera and flashes the hero; Q/E turn the camera and Z zooms; F12 saves a screenshot.
// - The orb glows (Layer::Glow adds light), the coins counter lives in the Hud (fixed on the screen).
// The atlas (Assets/atlas.json + atlas.png) is in nyiengisprite's format. With `--frames N` the hero walks
// by itself, so `--frames 240 --screenshot sprites.png` shows something.
#include <BitMotor/BitMotor.h>

#include <cmath>

using namespace BitMotor;

enum class Action { Left, Right, Up, Down, Shake };

constexpr Vec2 RoomSize = {480, 288};
constexpr Vec2 Chest = {360, 120};
constexpr Vec2 Orb = {240, 200};

struct SpritesExample : Game {
    Atlas atlas;
    Animation idle, walk, coin, chestOpen, orb;
    Camera2D camera;
    Vec2 hero = {120, 150};
    bool facingLeft = false, walking = false, autoWalk = false;
    float time = 0, lastTime = 0, chestTime = 0, flash = 0;
    int steps = 0;

    void Start() override {
        atlas = Atlas::Load("04_Sprites/Assets/atlas.json");
        idle = atlas.Animation("hero_idle");
        walk = atlas.Animation("hero_walk");
        coin = atlas.Animation("coin");
        chestOpen = atlas.Animation("chest_open");
        orb = atlas.Animation("orb");
        Input::Bind(Action::Left, "left", {Key::A, Key::Left, Button::DpadLeft, Button::LeftStickLeft});
        Input::Bind(Action::Right, "right", {Key::D, Key::Right, Button::DpadRight, Button::LeftStickRight});
        Input::Bind(Action::Up, "up", {Key::W, Key::Up, Button::DpadUp, Button::LeftStickUp});
        Input::Bind(Action::Down, "down", {Key::S, Key::Down, Button::DpadDown, Button::LeftStickDown});
        Input::Bind(Action::Shake, "shake", {Key::Space, Button::South});
        autoWalk = Args::Has("--frames");
        camera.Position = hero;
    }

    void Update() override {
        if (Input::KeyPressed(Key::Escape)) Quit();
        if (Input::KeyPressed(Key::F11)) Window::ToggleFullscreen();
        if (Input::KeyPressed(Key::F12)) Screen::Screenshot();
        lastTime = time;
        time += Time::Delta();

        Vec2 move = Input::Axis(Action::Left, Action::Right, Action::Up, Action::Down);
        if (autoWalk) move = {std::cos(time * 0.9f), std::sin(time * 1.3f) * 0.8f}; // a demo walk
        hero.X = Clamp(hero.X + move.X * 70 * Time::Delta(), 16.0f, RoomSize.X - 16);
        hero.Y = Clamp(hero.Y + move.Y * 70 * Time::Delta(), 40.0f, RoomSize.Y - 8);
        walking = Length(move) > 0.1f;
        if (std::abs(move.X) > 0.1f) facingLeft = move.X < 0;
        if (walking && walk.Fired("step", lastTime, time)) ++steps;

        // The chest opens (a one-shot animation) while the hero is near.
        chestTime = Distance(hero, Chest) < 40 ? chestTime + Time::Delta() : 0.0f;

        bool demoHit = autoWalk && std::floor(time / 1.5f) != std::floor(lastTime / 1.5f);
        if (Input::Pressed(Action::Shake) || demoHit) {
            camera.AddShake(0.6f);
            flash = 1;
        }
        flash = Max(0.0f, flash - 4 * Time::Delta());

        // The camera follows smoothly, stays inside the room, and can turn and zoom.
        if (Input::KeyDown(Key::Q)) camera.Rotation -= Time::Delta();
        if (Input::KeyDown(Key::E)) camera.Rotation += Time::Delta();
        camera.Zoom = Input::KeyDown(Key::Z) ? 2.0f : 1.0f;
        Vec2 half = Screen::Size() * (0.5f / camera.Zoom);
        Vec2 target = {Clamp(hero.X, half.X, RoomSize.X - half.X), Clamp(hero.Y - 8, half.Y, RoomSize.Y - half.Y)};
        camera.Position = Damp(camera.Position, target, 6, Time::Delta());
        camera.Update();
    }

    void Draw() override {
        Screen::Clear(Color::Hex(0x1B1F24));
        SetCamera(camera);

        // Floor tiles in the Background layer: always behind the world.
        Region tile = atlas.Sprite("tile");
        for (float y = 8; y < RoomSize.Y; y += 16)
            for (float x = 8; x < RoomSize.X; x += 16)
                Screen::Draw(Sprite2D{
                    .Region = tile, .Position = {x, y}, .Flip = int(x + y) % 48 == 0, .Layer = Layer::Background});

        // World: everything sorted by the y of its feet.
        Region column = atlas.Sprite("column");
        for (Vec2 at : {Vec2(80, 90), Vec2(200, 90), Vec2(320, 90), Vec2(80, 230), Vec2(200, 230), Vec2(320, 230)})
            Screen::Draw(Sprite2D{.Region = column, .Position = at, .Depth = at.Y});
        Screen::Draw(Sprite2D{.Region = chestOpen.Frame(chestTime), .Position = Chest, .Depth = Chest.Y});
        for (int i = 0; i < 5; ++i) {
            Vec2 at = {150.0f + 40 * float(i), 60};
            Screen::Draw(Sprite2D{.Region = coin.Frame(time + 0.1f * float(i)), .Position = at, .Depth = at.Y});
        }
        float pulse = 1 + 0.15f * std::sin(time * 4);
        Screen::Draw(Sprite2D{.Region = orb.Frame(time), .Position = Orb, .Rotation = time, .Depth = Orb.Y});
        Screen::Draw(Sprite2D{.Region = atlas.Sprite("shadow"), .Position = hero + Vec2(0, 1), .Depth = hero.Y - 0.5f});
        Screen::Draw(Sprite2D{.Region = (walking ? walk : idle).Frame(time),
                              .Position = hero,
                              .Flip = facingLeft,
                              .Flash = flash,
                              .Depth = hero.Y});

        // Glow: added on top of the world, it brightens what is under it.
        Screen::Draw(Sprite2D{.Region = atlas.Sprite("glow"),
                              .Position = Orb,
                              .Scale = {3 * pulse, 3 * pulse},
                              .Color = Colors::Violet,
                              .Layer = Layer::Glow});

        // Hud: screen pixels, the camera does not move it. One coin per 4 steps walked (up to 12).
        for (int i = 0; i < Min(steps / 4, 12); ++i)
            Screen::Draw(Sprite2D{.Region = coin.Frame(0), .Position = {8.0f + 10 * float(i), 8}, .Layer = Layer::Hud});
    }
};

int main() {
    return Run<SpritesExample>({
        .Title = "BitMotor 04 - Sprites",
        .Width = 320,
        .Height = 180,
        .WindowWidth = 1280,
        .WindowHeight = 720,
    });
}
