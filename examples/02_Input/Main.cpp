// 02_Input: actions, remapping and replays. There is no drawing yet (phase 3), so the screen colour and the
// window title show what happens:
// - Move with WASD, the arrows, the d-pad or the left stick: the colour follows the position.
// - Jump (Space / South button) flashes the screen white.
// - R: rebind Jump (press any key, mouse button or pad control; Esc cancels).
// - F5: start / stop recording. F6: replay the recording (the devices are ignored while it plays).
// - F11: fullscreen. Esc: quit. The controls are saved on exit and loaded on start (controls.json).
#include <BitMotor/BitMotor.h>

#include <format>

using namespace BitMotor;

enum class Action { Left, Right, Up, Down, Jump };

struct InputExample : Game {
    Vec2 position{0.5f, 0.5f}; // 0..1 in both axes
    float flash = 0;
    InputRecording recording;
    Subscription onPad;

    void Start() override {
        Input::Bind(Action::Left, "left", {Key::A, Key::Left, Button::DpadLeft, Button::LeftStickLeft});
        Input::Bind(Action::Right, "right", {Key::D, Key::Right, Button::DpadRight, Button::LeftStickRight});
        Input::Bind(Action::Up, "up", {Key::W, Key::Up, Button::DpadUp, Button::LeftStickUp});
        Input::Bind(Action::Down, "down", {Key::S, Key::Down, Button::DpadDown, Button::LeftStickDown});
        Input::Bind(Action::Jump, "jump", {Key::Space, Button::South});
        if (Input::LoadBindings()) Log::Info("Loaded your controls from {}", Files::SavePath("controls.json").string());

        onPad = Events::On<GamepadConnected>([](const GamepadConnected& e) { Log::Info("Hello, {}", e.Name); });
    }

    void Update() override {
        if (Input::KeyPressed(Key::Escape) && !Input::IsRebinding()) Quit();
        if (Input::KeyPressed(Key::F11)) Window::ToggleFullscreen();
        if (Input::KeyPressed(Key::R) && !Input::IsRebinding()) {
            Input::Rebind(Action::Jump);
            Log::Info("Press the new control for Jump (Esc cancels)");
        }
        if (Input::KeyPressed(Key::F5)) {
            if (Input::IsRecording()) {
                recording = Input::StopRecording();
                Log::Info("Recorded {} updates, {} changes", recording.Ticks, recording.Changes.size());
            } else {
                Input::StartRecording();
                Log::Info("Recording...");
            }
        }
        if (Input::KeyPressed(Key::F6) && !recording.Changes.empty()) {
            position = {0.5f, 0.5f};
            Input::Replay(recording);
            Log::Info("Replaying");
        }

        // Analog with a stick, 0 or 1 with keys; length at most 1 on diagonals.
        Vec2 move = Input::Axis(Action::Left, Action::Right, Action::Up, Action::Down);
        position.X = Clamp(position.X + move.X * 0.6f * Time::Delta(), 0.0f, 1.0f);
        position.Y = Clamp(position.Y + move.Y * 0.6f * Time::Delta(), 0.0f, 1.0f);

        if (Input::Pressed(Action::Jump)) {
            flash = 1;
            Input::Rumble(0.3f, 0.6f, 0.15f);
        }
        flash = Approach(flash, 0.0f, 4.0f * Time::Delta());
    }

    void Draw() override {
        Color place = Colors::Navy.Lerp(Colors::Crimson, position.X).Lerp(Colors::Gold, position.Y * 0.6f);
        Screen::Clear(place.Lerp(Colors::White, flash));

        std::string state = Input::IsRebinding()   ? "press a control for Jump"
                            : Input::IsReplaying() ? "replaying"
                            : Input::IsRecording() ? "recording"
                                                   : "R rebind, F5 record, F6 replay";
        Window::SetTitle(std::format("BitMotor 02 - Input | Jump: {} / {} | {} | {}", Input::KeyName(Action::Jump),
                                     Input::ButtonName(Action::Jump), Input::UsingGamepad() ? "gamepad" : "keyboard",
                                     state));
    }

    void Stop() override { Input::SaveBindings(); }
};

int main() {
    return Run<InputExample>({
        .Title = "BitMotor 02 - Input",
        .Width = 320,
        .Height = 180,
        .WindowWidth = 960,
        .WindowHeight = 540,
    });
}
