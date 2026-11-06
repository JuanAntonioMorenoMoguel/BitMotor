#pragma once
#include <string>

// Starting a game.
//     struct MyGame : BitMotor::Game {
//         void Start() override { Log::Info("Hello"); }
//         void Update() override { position += velocity * Time::Delta(); }   // TickRate times per second
//         void Draw() override { Screen::Clear(Colors::Navy); }              // once per frame
//     };
//     int main() { return BitMotor::Run<MyGame>({.Title = "My game", .Width = 320, .Height = 180}); }
//
// The window's close button ends the game (Esc does nothing unless the game says so). `--frames N` on the
// command line quits after N frames (used by the CI to run the examples without anyone closing them).
namespace BitMotor {

struct Config {
    std::string Title = "BitMotor";
    // Together with Title, names the player's save folder (Files::SavePath).
    std::string Organization = "BitMotor";
    // The game's own version, shown in the log at start ("1.2.0").
    std::string Version = "";
    // Logical resolution the game draws at, scaled to the window with black bars if the shape differs
    // (Screen::Width/Height). 0: the window's size in pixels, no scaling.
    int Width = 0;
    int Height = 0;
    // Window size at start, in pixels.
    int WindowWidth = 1280;
    int WindowHeight = 720;
    // With Width/Height: grow the image only by whole steps (x2, x3...) so every pixel keeps the same
    // size (pixel art). false: as big as fits, even if some pixels end up wider than others.
    bool IntegerScaling = true;
    bool Fullscreen = false;
    bool VSync = true;
    // Game::Update calls per second, with a fixed Time::Delta (deterministic: same input, same result).
    // 0: one Update per frame with the real frame time.
    int TickRate = 60;
    // PNG for the window and taskbar icon (relative: Files::AssetPath). Empty: the system's default.
    std::string Icon = "";
};

// What each game implements. Start is the place to add the Services and push the first Scene.
// Each Update: Services, global Tasks, Game::Update, then the scenes. Each frame: the scenes draw, then
// Game::Draw on top (global overlays). Without scenes, Game alone works as before.
class Game {
public:
    virtual ~Game() = default;
    virtual void Start() {}
    // Game logic, TickRate times per second (or once per frame if TickRate is 0).
    virtual void Update() {}
    // Drawing, once per frame. Time::Alpha() says how far it is between the last two Updates.
    virtual void Draw() {}
    virtual void Stop() {}
};

namespace Detail {
int Run(const Config& config, Game& game);
void SetSourceDir(const char* folder);
} // namespace Detail

// Opens the window, calls Start, runs until the window closes or Quit() is called, and calls Stop.
// Returns the exit code for main(): 0 if everything went well.
template <class T> int Run(const Config& config = {}) {
#ifdef BITMOTOR_SOURCE_DIR
    // Defined by BitMotor_Prepare for the game: lets Files::AssetPath read from the source tree.
    Detail::SetSourceDir(BITMOTOR_SOURCE_DIR);
#endif
    T game;
    return Detail::Run(config, game);
}

// Asks the loop to finish at the end of the current frame.
void Quit();

} // namespace BitMotor
