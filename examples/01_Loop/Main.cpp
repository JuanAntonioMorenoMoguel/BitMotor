// 01_Loop: the game loop and the core modules that do not need drawing yet.
// - Update runs 60 times per second with a fixed Delta; Draw runs once per frame.
// - The background colour breathes with Time, and every 3 seconds a hit stop freezes it for a moment.
// - The title shows the frames and updates per second.
// - A small save (BITMOTOR_FIELDS) counts how many times the example has been opened.
// - Window events (resize, focus) arrive as Events.
// Run it with `--frames 120` to make it quit by itself; `--tickrate 30` changes the updates per second.
#include <BitMotor/BitMotor.h>

#include <cmath>
#include <format>

using namespace BitMotor;

struct Stats {
    int Launches = 0;
    double SecondsPlayed = 0;
};
BITMOTOR_FIELDS(Stats, Launches, SecondsPlayed)

struct LoopExample : Game {
    Stats stats;
    Timer titleTimer{0.5f};
    Timer hitTimer{3.0f};
    std::uint64_t lastFrame = 0, lastTick = 0;
    float pulse = 0;
    Subscription onResize, onFocus;

    void Start() override {
        stats = Data::Load<Stats>("stats.json").value_or(Stats{});
        ++stats.Launches;
        Log::Info("Opened {} times, {:.1f} s played. Save: {}", stats.Launches, stats.SecondsPlayed,
                  Files::SavePath("stats.json").string());

        onResize = Events::On<WindowResized>(
            [](const WindowResized& e) { Log::Info("Window resized to {}x{} pixels", e.Width, e.Height); });
        onFocus = Events::On<FocusChanged>([](const FocusChanged& e) {
            Log::Info("{}", e.Focused ? "Focus back: carry on" : "Focus lost: a real game would pause here");
        });
    }

    void Update() override {
        // Fixed step: Time::Delta() is always 1 / TickRate here.
        pulse += Time::Delta();
        if (hitTimer.Update()) {
            Time::HitStop(0.25f);
            hitTimer.Start();
            Log::Info("Hit stop at {:.2f} s of game time", Time::Total());
        }
    }

    void Draw() override {
        // Draw sees real frames: interpolate with Alpha so the motion is smooth between Updates.
        float t = pulse + Time::Alpha() / 60.0f;
        float breath = 0.5f + 0.5f * std::sin(t * 2.0f);
        Color background = Time::InHitStop() ? Colors::White : Colors::Navy.Lerp(Colors::Teal, breath);
        Screen::Clear(background);

        if (titleTimer.Update(Time::RealDelta())) {
            float fps = float(Time::Frame() - lastFrame) / titleTimer.Duration;
            float ups = float(Time::Tick() - lastTick) / titleTimer.Duration;
            Window::SetTitle(std::format("BitMotor 01 - Loop | {:.0f} FPS | {:.0f} updates/s", fps, ups));
            lastFrame = Time::Frame();
            lastTick = Time::Tick();
            titleTimer.Start();
        }
    }

    void Stop() override {
        stats.SecondsPlayed += Time::RealTotal();
        if (Data::Save("stats.json", stats)) Log::Info("Saved: {}", Data::ToJson(stats));
    }
};

int main() {
    return Run<LoopExample>({
        .Title = "BitMotor 01 - Loop",
        .Version = "1.0",
        .Width = 320,
        .Height = 180,
        .WindowWidth = 960,
        .WindowHeight = 540,
        .TickRate = Args::Get("--tickrate", 60),
    });
}
