#pragma once
#include "BitMotor/Core/Fields.h"
#include "BitMotor/Core/Keys.h"
#include "BitMotor/Math/Vector.h"

#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// Keyboard, mouse and gamepad. The game names its actions with its own enum and binds them once:
//     enum class Action { Left, Right, Up, Down, Jump };
//
//     void Start() override {
//         Input::Bind(Action::Jump, "jump", {Key::Space, Key::W, Button::South});
//         Input::Bind(Action::Left, "left", {Key::A, Key::Left, Button::DpadLeft, Button::LeftStickLeft});
//         ...
//         Input::LoadBindings(); // the player's remapped controls, if any
//     }
//     void Update() override {
//         if (Input::Pressed(Action::Jump)) Jump();
//         velocity.X = Input::Axis(Action::Left, Action::Right) * speed;   // analog with a stick
//     }
//
// Down/Pressed/Released are per Update: with TickRate 60 a press is seen by exactly one Update, even if
// the frame runs several Updates or none. Read them in Update, not in Draw.
namespace BitMotor {

// Sent when a gamepad is plugged in or removed.
struct GamepadConnected {
    std::string Name;
};
struct GamepadDisconnected {
    std::string Name;
};

// Action changes recorded with Input::StartRecording, to replay them (demos, bug reports, tests).
// Saveable with Data::Save. Only actions are recorded, not raw keys or the mouse.
struct InputChange {
    std::uint64_t Tick = 0; // Update number since the recording started
    int Action = 0;
    float Strength = 0; // 0 released, 1 fully pressed, in between for sticks and triggers
};
BITMOTOR_FIELDS(InputChange, Tick, Action, Strength)

struct InputRecording {
    std::vector<std::string> Actions; // names at recording time, to check the replay matches
    std::vector<InputChange> Changes;
    std::uint64_t Ticks = 0; // length in Updates
};
BITMOTOR_FIELDS(InputRecording, Actions, Changes, Ticks)

namespace Input {

namespace Detail {
template <class A>
concept ActionId = std::is_enum_v<A> || std::is_integral_v<A>;
void Bind(int action, std::string_view name, std::initializer_list<Control> controls);
} // namespace Detail

// ---------------------------------------------------------------- actions

// Gives an action its default controls. `name` identifies it in the saved controls file.
template <Detail::ActionId A> void Bind(A action, std::string_view name, std::initializer_list<Control> controls) {
    Detail::Bind(int(action), name, controls);
}

bool Down(int action);
bool Pressed(int action);
bool Released(int action);
// How much: 0..1 (sticks and triggers give values in between; keys give 0 or 1).
float Strength(int action);
template <Detail::ActionId A> bool Down(A action) { return Down(int(action)); }
template <Detail::ActionId A> bool Pressed(A action) { return Pressed(int(action)); }
template <Detail::ActionId A> bool Released(A action) { return Released(int(action)); }
template <Detail::ActionId A> float Strength(A action) { return Strength(int(action)); }

// -1..1 from two opposite actions: Strength(positive) - Strength(negative).
float Axis(int negative, int positive);
template <Detail::ActionId A> float Axis(A negative, A positive) { return Axis(int(negative), int(positive)); }
// Movement from four actions, length at most 1. +Y is down, like the screen.
Vec2 Axis(int left, int right, int up, int down);
template <Detail::ActionId A> Vec2 Axis(A left, A right, A up, A down) {
    return Axis(int(left), int(right), int(up), int(down));
}

// Holds an action down (true) or lets it go (false), whatever the devices say. For bots, tutorials, tests.
void Force(int action, bool down);
template <Detail::ActionId A> void Force(A action, bool down) { Force(int(action), down); }

// ---------------------------------------------------------------- raw devices

bool KeyDown(Key key);
bool KeyPressed(Key key);
bool KeyReleased(Key key);

// Mouse position in logical screen pixels (Screen::ToLogical).
Vec2 Mouse();
// Movement since the previous Update, in logical pixels.
Vec2 MouseDelta();
// Wheel turns since the previous Update (positive: away from the player).
float MouseWheel();
bool MouseDown(MouseButton button);
bool MousePressed(MouseButton button);
bool MouseReleased(MouseButton button);

// Sticks with the dead zone applied: 0 at rest, length at most 1.
Vec2 LeftStick();
Vec2 RightStick();
bool ButtonDown(Button button);
bool ButtonPressed(Button button);

// true after the player touched the gamepad, false after the keyboard or mouse (to show the right prompts).
bool UsingGamepad();
bool HasGamepad();
// Vibration of the gamepad in use; `low` and `high` motors in 0..1.
void Rumble(float low, float high, float seconds);

// ---------------------------------------------------------------- remapping

// Waits for the next key, mouse button or gamepad control and binds it to the action, replacing the
// action's controls of the same kind (keyboard and mouse, or gamepad). Escape cancels.
void Rebind(int action);
template <Detail::ActionId A> void Rebind(A action) { Rebind(int(action)); }
bool IsRebinding();
void CancelRebind();

// Name of the action's first keyboard/mouse control and first gamepad control ("-" if none), for prompts.
std::string KeyName(int action);
std::string ButtonName(int action);
template <Detail::ActionId A> std::string KeyName(A action) { return KeyName(int(action)); }
template <Detail::ActionId A> std::string ButtonName(A action) { return ButtonName(int(action)); }
std::vector<Control> Controls(int action);
template <Detail::ActionId A> std::vector<Control> Controls(A action) { return Controls(int(action)); }

// The player's controls, as {"jump": ["Space", "Pad South"], ...}. Relative paths go to Files::SavePath.
// Load returns false if the file does not exist yet (the defaults stay).
bool LoadBindings(const std::filesystem::path& path = "controls.json");
bool SaveBindings(const std::filesystem::path& path = "controls.json");
// Back to the controls given to Bind.
void ResetBindings();

// ---------------------------------------------------------------- recording

void StartRecording();
InputRecording StopRecording();
bool IsRecording();
// Plays a recording from the next Update: actions come from it and the devices are ignored until it ends.
void Replay(const InputRecording& recording);
bool IsReplaying();
void StopReplay();

} // namespace Input

} // namespace BitMotor
