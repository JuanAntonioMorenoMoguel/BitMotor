// 00_Window: the smallest BitMotor program. Opens a window and waits until you close it (Esc or the X).
// Run it with `--frames 60` to make it quit by itself after 60 frames.
#include <BitMotor/BitMotor.h>

using namespace BitMotor;

struct HelloWindow : Game {
    void Start() override { Log::Info("BitMotor {}: window open, press Esc to close it", Version()); }
    void Update() override {
        if (Input::KeyPressed(Key::Escape)) Quit();
    }
    void Stop() override { Log::Info("Bye"); }
};

int main() {
    return Run<HelloWindow>({
        .Title = "BitMotor 00 - Window",
        .WindowWidth = 960,
        .WindowHeight = 540,
    });
}
