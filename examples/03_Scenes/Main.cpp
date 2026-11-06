// 03_Scenes: scenes, managers, tasks and a service. There is no drawing yet (phase 3), so the screen colour
// and the window title show what happens:
// - Title (navy): Enter / South button starts a game with a fade; it also starts by itself after 3 s.
// - Play: a "3, 2, 1" countdown (a task), then the colour breathes (a manager) while a clock (another
//   manager) runs for 10 s. P / Start pauses.
// - Pause: the game stays visible behind and frozen (DrawBelow). P resumes, Q goes back to the title.
// - Game over (red): shows the score; back to the title after 3 s or with Enter.
// - A Stats service counts the games played across launches (stats.json).
// - Settings (settings.json): F11 toggles fullscreen and T shows the timer during a game; both are saved,
//   so the next launch opens the same way. The game adds ShowTimer to the standard settings.
// Esc on the title: quit.
#include <BitMotor/BitMotor.h>

#include <cmath>
#include <format>

using namespace BitMotor;

enum class Action { Confirm, Pause, Quit };

struct SavedStats {
    int Games = 0;
};
BITMOTOR_FIELDS(SavedStats, Games)

// Lives for the whole game, across scenes.
struct Stats : Service {
    SavedStats Saved;
    void Start() override { Saved = Data::Load<SavedStats>("stats.json").value_or(SavedStats{}); }
    void Stop() override { Data::Save("stats.json", Saved); }
};

// The standard settings plus this game's own.
struct ExampleSettings : Settings {
    bool ShowTimer = true;
};
BITMOTOR_FIELDS_EXTENDS(ExampleSettings, Settings, ShowTimer)

struct Title;
struct GameOver;

// The background colour of a game: breathes between two colours.
struct Pulse : Manager {
    float Seconds = 0;
    void Update() override { Seconds += Time::Delta(); }
    void Draw() override {
        float breath = 0.5f + 0.5f * std::sin(Seconds * 3.0f);
        Screen::Clear(Colors::DarkGreen.Lerp(Colors::Teal, breath));
    }
};

// Counts down the time of a game and ends it.
struct Clock : Manager {
    float Left = 10;
    int Score = 0;
    void Update() override;
};

struct Play : Scene {
    int Countdown = 3;

    Play() {
        Add<Pulse>();
        Add<Clock>();
    }

    void Enter() override {
        ++Services::Get<Stats>().Saved.Games;
        Get<Pulse>().Active = Get<Clock>().Active = false; // frozen until "go"
        Run(Sequence().Wait(1).Do([this] { --Countdown; }).Repeat(3).Do([this] {
            Get<Pulse>().Active = Get<Clock>().Active = true;
        }));
    }

    void Update() override;

    void Draw() override {
        if (Countdown > 0) Screen::Clear(Colors::Slate);
        if (Countdown > 0)
            Window::SetTitle(std::format("03_Scenes - {}...", Countdown));
        else if (Settings::Get<ExampleSettings>().ShowTimer)
            Window::SetTitle(std::format("03_Scenes - {:.1f} s left (T hides the timer)", Get<Clock>().Left));
        else
            Window::SetTitle("03_Scenes - playing (T shows the timer)");
    }
};

struct Pause : Scene {
    Pause() { DrawBelow = true; }
    void Update() override;
    void Draw() override { Window::SetTitle("03_Scenes - PAUSED (P resume, Q title)"); }
};

struct GameOver : Scene {
    int Score;
    explicit GameOver(int score) : Score(score) {}
    void Enter() override { Run(Sequence().Wait(3).Do(BackToTitle)); }
    void Update() override {
        if (Input::Pressed(Action::Confirm)) BackToTitle();
    }
    void Draw() override {
        Screen::Clear(Colors::DarkRed);
        Window::SetTitle(std::format("03_Scenes - game over, score {}", Score));
    }
    static void BackToTitle();
};

struct Title : Scene {
    void Enter() override { Run(Sequence().Wait(3).Do(StartGame)); }
    void Update() override {
        if (Input::Pressed(Action::Confirm)) StartGame();
        if (Input::Pressed(Action::Quit)) BitMotor::Quit();
    }
    void Draw() override {
        Screen::Clear(Colors::Navy);
        Window::SetTitle(std::format("03_Scenes - press Enter ({} games played)", Services::Get<Stats>().Saved.Games));
    }
    static void StartGame() {
        if (!Scenes::InTransition()) Scenes::Replace(MakeScene<Play>(), Transition::Fade(0.6f));
    }
};

void Clock::Update() {
    Left -= Time::Delta();
    Score = int((10 - Left) * 10);
    if (Left <= 0 && !Scenes::InTransition())
        Scenes::Replace(MakeScene<GameOver>(Score), Transition::Fade(0.8f, Colors::DarkRed));
}

void Play::Update() {
    if (Input::Pressed(Action::Pause)) Scenes::Push(MakeScene<Pause>());
}

void Pause::Update() {
    if (Input::Pressed(Action::Pause)) Scenes::Pop();
    if (Input::Pressed(Action::Quit) || Input::KeyPressed(Key::Q)) {
        Scenes::Clear(Transition::Fade(0.6f));
        Scenes::Push(MakeScene<Title>()); // goes in at the middle of the fade
    }
}

void GameOver::BackToTitle() {
    if (!Scenes::InTransition()) Scenes::Replace(MakeScene<Title>(), Transition::Fade(0.6f));
}

struct ScenesExample : Game {
    void Start() override {
        Input::Bind(Action::Confirm, "confirm", {Key::Enter, Key::Space, Button::South});
        Input::Bind(Action::Pause, "pause", {Key::P, Button::Start});
        Input::Bind(Action::Quit, "quit", {Key::Escape, Button::Back});
        Settings::Use<ExampleSettings>();
        Services::Add<Stats>();
        Scenes::Push(MakeScene<Title>());
    }

    // Global, whatever the scene.
    void Update() override {
        if (Input::KeyPressed(Key::F11)) {
            Window::ToggleFullscreen(); // also updates Settings::Fullscreen
            Settings::Save();
        }
        if (Input::KeyPressed(Key::T)) {
            auto& settings = Settings::Get<ExampleSettings>();
            settings.ShowTimer = !settings.ShowTimer;
            Settings::Save();
        }
    }
};

int main() { return Run<ScenesExample>({.Title = "03_Scenes", .Width = 320, .Height = 180}); }
