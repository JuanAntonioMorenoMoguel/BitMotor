#pragma once
#include <string>

// Starting a game (provisional in phase 0: opens the window and runs the loop until it is closed).
//     struct MyGame : BitMotor::Game {
//         void Start() override { BitMotor::Log::Info("Hello"); }
//     };
//     int main() { return BitMotor::Run<MyGame>({.Title = "My game"}); }
//
// The window closes with Esc or its close button. `--frames N` on the command line quits after N frames
// (used by the CI to run the examples without anyone closing them).
namespace BitMotor {

struct Config {
    std::string Title = "BitMotor";
    // Together with Title, names the player's save folder (Files::SavePath).
    std::string Organization = "BitMotor";
    int WindowWidth = 1280;
    int WindowHeight = 720;
};

// What each game implements. Phase 2 adds Services and Scenes around it.
class Game {
public:
    virtual ~Game() = default;
    virtual void Start() {}
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
