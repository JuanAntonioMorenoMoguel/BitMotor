// 06_Particles: styles of particles on the CPU, with the lighting of 05.
// - A brazier burns in the middle: fire (Glow layer, it shines), smoke and embers that fall with gravity.
// - Click (or Space) for a burst of sparks where the mouse is; with `--frames N` they go off by themselves.
// - Coins rain from the top as sprites from the atlas, spinning, with drag.
// - The title shows how many particles are alive. F12 saves a screenshot.
#include <BitMotor/BitMotor.h>

#include <cmath>
#include <format>

using namespace BitMotor;

constexpr Vec2 Brazier = {160, 130};

struct ParticlesExample : Game {
    Atlas atlas;
    Particles particles{4096, 42};
    Random random{5};
    ParticleStyle fire, smoke, embers, sparks, coins;
    Camera2D camera;
    Timer titleTimer{0.5f};
    float time = 0, lastTime = 0;
    bool autoPlay = false;

    void Start() override {
        atlas = Atlas::Load("04_Sprites/Assets/atlas.json");
        autoPlay = Args::Has("--frames");
        camera.Position = Screen::Center();
        Lighting::Enable(true);
        Lighting::SetAmbient(Color::Hex(0x20243A));
        Lighting::SetBloom(0.8f);
        Screen::SetEffects({.Vignette = 0.35f});

        // Fire: short-lived, rising, from yellow to a transparent red, added on top (Glow).
        fire = {.Region = atlas.Sprite("glow"),
                .Life = {0.4f, 0.8f},
                .Speed = {20, 45},
                .Spread = 0.6f,
                .Gravity = {0, -30},
                .StartSize = {0.5f, 0.9f},
                .EndSize = 0.1f,
                .StartColor = Color::Hex(0xFFD060),
                .EndColor = Color::Hex(0xFF3010).WithAlpha(0),
                .Layer = Layer::Glow};
        // Smoke: slow, big and dark, over the world.
        smoke = {.Life = {1.5f, 2.5f},
                 .Speed = {8, 20},
                 .Spread = 0.8f,
                 .Gravity = {6, -4},
                 .Drag = 0.3f,
                 .StartSize = {3, 4},
                 .EndSize = 9,
                 .StartColor = Color::Hex(0x404048).WithAlpha(0.5f),
                 .EndColor = Color::Hex(0x202028).WithAlpha(0),
                 .Layer = Layer::Overlay};
        // Embers: tiny, thrown up, falling back.
        embers = {.Life = {0.8f, 1.4f},
                  .Speed = {40, 80},
                  .Spread = 1.2f,
                  .Gravity = {0, 140},
                  .StartSize = 1,
                  .EndSize = 1,
                  .StartColor = Color::Hex(0xFFB040),
                  .EndColor = Color::Hex(0xFF4000).WithAlpha(0),
                  .Layer = Layer::Glow};
        // Sparks: a burst in every direction that slows down.
        sparks = {.Life = {0.3f, 0.7f},
                  .Speed = {60, 160},
                  .Spread = Tau,
                  .Drag = 4,
                  .StartSize = {1, 2},
                  .EndSize = 0,
                  .StartColor = Colors::White,
                  .EndColor = Color::Hex(0x60A0FF).WithAlpha(0),
                  .Layer = Layer::Glow};
        // Coins: atlas sprites falling and spinning.
        coins = {.Region = atlas.Sprite("coin_0"),
                 .Life = 3,
                 .Speed = {5, 20},
                 .Direction = Pi / 2,
                 .Spread = 0.5f,
                 .Gravity = {0, 40},
                 .Drag = 0.5f,
                 .StartSize = 1,
                 .EndSize = 1,
                 .EndColor = Colors::White.WithAlpha(0),
                 .Spin = {-4, 4},
                 .RandomAngle = true,
                 .Depth = 1000};
    }

    void Update() override {
        if (Input::KeyPressed(Key::Escape)) Quit();
        if (Input::KeyPressed(Key::F11)) Window::ToggleFullscreen();
        if (Input::KeyPressed(Key::F12)) Screen::Screenshot();
        lastTime = time;
        time += Time::Delta();

        particles.Emit(fire, Brazier + Vec2(random.Range(-5, 5), -4), 3);
        if (random.Chance(0.3f)) particles.Emit(smoke, Brazier + Vec2(0, -20));
        if (random.Chance(0.15f)) particles.Emit(embers, Brazier + Vec2(0, -6), 2);
        if (random.Chance(0.05f)) particles.Emit(coins, {random.Range(10, 310), -6});

        bool demoBurst = autoPlay && std::floor(time / 0.7f) != std::floor(lastTime / 0.7f);
        if (Input::MousePressed(MouseButton::Left) || Input::KeyPressed(Key::Space) || demoBurst) {
            Vec2 at = autoPlay ? Vec2(random.Range(40, 280), random.Range(30, 120)) : camera.ToWorld(Input::Mouse());
            particles.Emit(sparks, at, 40);
            camera.AddShake(0.25f);
        }
        particles.Update();
        camera.Update();

        if (titleTimer.Update()) {
            Window::SetTitle(std::format("BitMotor 06 - Particles ({} alive)", particles.Count()));
            titleTimer.Start();
        }
    }

    void Draw() override {
        Screen::Clear(Colors::Black);
        SetCamera(camera);
        Region tile = atlas.Sprite("tile");
        for (float y = 8; y < 180; y += 16)
            for (float x = 8; x < 320; x += 16)
                Screen::Draw(Sprite2D{.Region = tile, .Position = {x, y}, .Depth = -1000});
        Screen::Draw(Sprite2D{.Region = atlas.Sprite("column"),
                              .Position = Brazier + Vec2(0, 2),
                              .Scale = {1, 0.5f},
                              .Color = Color::Hex(0x806050),
                              .Depth = Brazier.Y});
        float flicker = 1 + 0.1f * std::sin(time * 23) + 0.05f * std::sin(time * 37);
        Screen::Draw(Light2D{.Position = Brazier + Vec2(0, -12),
                             .Height = 20,
                             .Radius = 150 * flicker,
                             .Color = Color::Hex(0xFF9040),
                             .Intensity = 1.8f});
        particles.Draw();
    }
};

int main() {
    return Run<ParticlesExample>({
        .Title = "BitMotor 06 - Particles",
        .Width = 320,
        .Height = 180,
        .WindowWidth = 1280,
        .WindowHeight = 720,
    });
}
