#include "BitMotor/Core/Input.h"

#include "BitMotor/Core/Data.h"
#include "BitMotor/Core/Log.h"
#include "BitMotor/Math/Functions.h"
#include "Core/InputInternal.h"

#include <algorithm>
#include <array>
#include <map>
#include <utility>

namespace BitMotor {

namespace {

constexpr float DownThreshold = 0.5f;

// A digital control (key, mouse button, pad button) seen per Update, and per frame (for the UI, which
// runs in Draw: it must work when no Update runs, as in a pause with Time::SetScale(0)).
struct Digital {
    bool Down = false;         // right now, from the events
    bool LatchPress = false;   // went down since the previous Update (keeps quick taps)
    bool TickDown = false;     // as this Update sees it
    bool PreviousTick = false; // as the previous Update saw it
    bool FrameLatch = false, FrameDown = false, FrameEdge = false; // the same, per frame

    void Set(bool down) {
        if (down && !Down) LatchPress = FrameLatch = true;
        Down = down;
    }
    void Tick() {
        PreviousTick = TickDown;
        TickDown = Down || LatchPress;
        LatchPress = false;
    }
    // Every press counts, even two taps on consecutive frames.
    void TickFrame() {
        FrameDown = Down || FrameLatch;
        FrameEdge = FrameLatch;
        FrameLatch = false;
    }
    bool Pressed() const { return TickDown && !PreviousTick; }
    bool Released() const { return !TickDown && PreviousTick; }
    bool FramePressed() const { return FrameEdge; }
};

struct ActionData {
    std::string Name;
    bool Bound = false;
    std::vector<Control> Defaults, Controls;
    bool Forced = false;
    float Strength = 0, Previous = 0;
};

struct State {
    std::array<Digital, std::size_t(Key::Count)> Keys{};
    std::array<Digital, std::size_t(MouseButton::Count)> Mouse{};
    std::array<Digital, std::size_t(Button::Count)> Pad{};
    std::array<float, std::size_t(Detail::PadAxis::Count)> Axes{};
    Vec2 MousePosition, PendingMouseDelta, TickMouseDelta;
    float PendingWheel = 0, TickWheel = 0;
    Vec2 FrameMouseStart, FrameMouseDelta;
    float FramePendingWheel = 0, FrameWheel = 0;
    std::array<bool, std::size_t(Button::Count)> AnalogFrame{}, AnalogPreviousFrame{};
    bool UsingGamepad = false, HasGamepad = false;
    bool MouseForced = false; // Input::ForceMouse: the real mouse is ignored
    std::vector<ActionData> Actions;

    int RebindAction = -1;
    bool RebindEnded = false, FrameRebindEnded = false; // the key that ends a Rebind must not reach the UI

    bool Recording = false;
    InputRecording Record;
    std::vector<float> RecordedStrength;

    bool Replaying = false;
    InputRecording Playback;
    std::size_t PlaybackNext = 0;
    std::uint64_t PlaybackTick = 0;
    std::vector<float> PlaybackStrength;
};
State Now;

ActionData* Find(int action) {
    if (action < 0 || std::size_t(action) >= Now.Actions.size() || !Now.Actions[std::size_t(action)].Bound)
        return nullptr;
    return &Now.Actions[std::size_t(action)];
}

Vec2 DeadZone(Vec2 stick) {
    float length = Length(stick);
    if (length <= Detail::StickDeadZone) return {};
    float scaled = Min((length - Detail::StickDeadZone) / (1.0f - Detail::StickDeadZone), 1.0f);
    return stick / length * scaled;
}

Vec2 Stick(bool right) {
    using Detail::PadAxis;
    return DeadZone(right ? Vec2{Now.Axes[std::size_t(PadAxis::RightX)], Now.Axes[std::size_t(PadAxis::RightY)]}
                          : Vec2{Now.Axes[std::size_t(PadAxis::LeftX)], Now.Axes[std::size_t(PadAxis::LeftY)]});
}

// 0..1 for analog pad controls (triggers and stick directions); -1 if the button is digital.
float Analog(Button button) {
    using Detail::PadAxis;
    switch (button) {
    case Button::LeftTrigger:
        return Now.Axes[std::size_t(PadAxis::LeftTrigger)];
    case Button::RightTrigger:
        return Now.Axes[std::size_t(PadAxis::RightTrigger)];
    case Button::LeftStickUp:
        return Max(-Stick(false).Y, 0.0f);
    case Button::LeftStickDown:
        return Max(Stick(false).Y, 0.0f);
    case Button::LeftStickLeft:
        return Max(-Stick(false).X, 0.0f);
    case Button::LeftStickRight:
        return Max(Stick(false).X, 0.0f);
    case Button::RightStickUp:
        return Max(-Stick(true).Y, 0.0f);
    case Button::RightStickDown:
        return Max(Stick(true).Y, 0.0f);
    case Button::RightStickLeft:
        return Max(-Stick(true).X, 0.0f);
    case Button::RightStickRight:
        return Max(Stick(true).X, 0.0f);
    default:
        return -1.0f;
    }
}

// The strength a control gives this Update.
float StrengthOf(Control control) {
    switch (control.Type) {
    case Control::Kind::Key:
        return Now.Keys[std::size_t(control.Code)].TickDown ? 1.0f : 0.0f;
    case Control::Kind::Mouse:
        return Now.Mouse[std::size_t(control.Code)].TickDown ? 1.0f : 0.0f;
    case Control::Kind::Button: {
        float analog = Analog(Button(control.Code));
        if (analog >= 0) return analog;
        return Now.Pad[std::size_t(control.Code)].TickDown ? 1.0f : 0.0f;
    }
    }
    return 0;
}

void Assign(Control control) {
    ActionData* action = Find(Now.RebindAction);
    Now.RebindAction = -1;
    Now.RebindEnded = true;
    if (!action) return;
    std::erase_if(action->Controls, [&](const Control& c) { return c.IsGamepad() == control.IsGamepad(); });
    action->Controls.insert(action->Controls.begin(), control);
    Log::Info("'{}' is now {}", action->Name, Name(control));
}

void ProcessReplay() {
    if (Now.PlaybackTick >= Now.Playback.Ticks) {
        Now.Replaying = false;
        return;
    }
    auto& changes = Now.Playback.Changes;
    while (Now.PlaybackNext < changes.size() && changes[Now.PlaybackNext].Tick <= Now.PlaybackTick) {
        const InputChange& change = changes[Now.PlaybackNext++];
        if (change.Action >= 0 && std::size_t(change.Action) < Now.PlaybackStrength.size())
            Now.PlaybackStrength[std::size_t(change.Action)] = change.Strength;
    }
    ++Now.PlaybackTick;
}

std::string FirstName(int action, bool gamepad) {
    ActionData* data = Find(action);
    if (!data) return "-";
    for (const Control& control : data->Controls)
        if (control.IsGamepad() == gamepad) return Name(control);
    return "-";
}

} // namespace

// ---------------------------------------------------------------- feeding (InputSdl.cpp and tests)

namespace Detail {

void ResetInput() { Now = State{}; }

void InputKey(Key key, bool down) {
    if (key == Key::Unknown || key >= Key::Count) return;
    Now.Keys[std::size_t(key)].Set(down);
    if (!down) return;
    Now.UsingGamepad = false;
    if (Now.RebindAction >= 0) {
        if (key == Key::Escape)
            Input::CancelRebind();
        else
            Assign(key);
    }
}

void InputMouseButton(MouseButton button, bool down) {
    if (button >= MouseButton::Count) return;
    Now.Mouse[std::size_t(button)].Set(down);
    if (!down) return;
    Now.UsingGamepad = false;
    if (Now.RebindAction >= 0) Assign(button);
}

void InputMouseMove(Vec2 position) {
    Vec2 delta = position - Now.MousePosition;
    Now.PendingMouseDelta += delta;
    Now.MousePosition = position;
    if (LengthSquared(delta) > 4.0f) Now.UsingGamepad = false;
}

void InputMouseWheel(float amount) {
    Now.PendingWheel += amount;
    Now.FramePendingWheel += amount;
}

void InputPadButton(Button button, bool down) {
    if (button >= Button::Count || Analog(button) >= 0) return;
    Now.Pad[std::size_t(button)].Set(down);
    if (!down) return;
    Now.UsingGamepad = true;
    if (Now.RebindAction >= 0) Assign(button);
}

void InputPadAxis(PadAxis axis, float value) {
    if (axis >= PadAxis::Count) return;
    Now.Axes[std::size_t(axis)] = Clamp(value, -1.0f, 1.0f);
    // Moving a stick or trigger clearly counts as using the gamepad, and can answer a Rebind.
    for (int b = int(Button::LeftTrigger); b < int(Button::Count); ++b) {
        if (Analog(Button(b)) < DownThreshold) continue;
        Now.UsingGamepad = true;
        if (Now.RebindAction >= 0) Assign(Button(b));
        break;
    }
}

void InputPadConnected(bool connected) {
    Now.HasGamepad = connected;
    if (!connected) {
        Now.Axes = {};
        for (auto& button : Now.Pad) button.Set(false);
        Now.UsingGamepad = false;
    }
}

void InputFrame() {
    for (auto& key : Now.Keys) key.TickFrame();
    for (auto& button : Now.Mouse) button.TickFrame();
    for (auto& button : Now.Pad) button.TickFrame();
    Now.FrameMouseDelta = Now.MousePosition - Now.FrameMouseStart;
    Now.FrameMouseStart = Now.MousePosition;
    Now.FrameWheel = std::exchange(Now.FramePendingWheel, 0.0f);
    Now.FrameRebindEnded = std::exchange(Now.RebindEnded, false);
    Now.AnalogPreviousFrame = Now.AnalogFrame;
    for (std::size_t b = 0; b < Now.AnalogFrame.size(); ++b) Now.AnalogFrame[b] = Analog(Button(b)) >= DownThreshold;
}

bool FrameDown(Control control) {
    switch (control.Type) {
    case Control::Kind::Key:
        return control.Code >= 0 && control.Code < int(Key::Count) && Now.Keys[std::size_t(control.Code)].FrameDown;
    case Control::Kind::Mouse:
        return control.Code >= 0 && control.Code < int(MouseButton::Count) &&
               Now.Mouse[std::size_t(control.Code)].FrameDown;
    case Control::Kind::Button:
        if (control.Code < 0 || control.Code >= int(Button::Count)) return false;
        if (Analog(Button(control.Code)) >= 0) return Now.AnalogFrame[std::size_t(control.Code)];
        return Now.Pad[std::size_t(control.Code)].FrameDown;
    }
    return false;
}

bool FramePressed(Control control) {
    switch (control.Type) {
    case Control::Kind::Key:
        return control.Code >= 0 && control.Code < int(Key::Count) &&
               Now.Keys[std::size_t(control.Code)].FramePressed();
    case Control::Kind::Mouse:
        return control.Code >= 0 && control.Code < int(MouseButton::Count) &&
               Now.Mouse[std::size_t(control.Code)].FramePressed();
    case Control::Kind::Button: {
        if (control.Code < 0 || control.Code >= int(Button::Count)) return false;
        auto b = std::size_t(control.Code);
        if (Analog(Button(control.Code)) >= 0) return Now.AnalogFrame[b] && !Now.AnalogPreviousFrame[b];
        return Now.Pad[b].FramePressed();
    }
    }
    return false;
}

Vec2 FrameMouseDelta() { return Now.FrameMouseDelta; }
bool RebindEndedThisFrame() { return Now.FrameRebindEnded; }
float FrameWheel() { return Now.FrameWheel; }

void InputTick() {
    for (auto& key : Now.Keys) key.Tick();
    for (auto& button : Now.Mouse) button.Tick();
    for (auto& button : Now.Pad) button.Tick();
    Now.TickMouseDelta = std::exchange(Now.PendingMouseDelta, Vec2{});
    Now.TickWheel = std::exchange(Now.PendingWheel, 0.0f);

    if (Now.Replaying) ProcessReplay();

    for (std::size_t i = 0; i < Now.Actions.size(); ++i) {
        ActionData& action = Now.Actions[i];
        action.Previous = action.Strength;
        float strength = 0;
        if (Now.Replaying) {
            strength = i < Now.PlaybackStrength.size() ? Now.PlaybackStrength[i] : 0.0f;
        } else {
            for (const Control& control : action.Controls) strength = Max(strength, StrengthOf(control));
            if (action.Forced) strength = 1;
        }
        action.Strength = strength;
    }

    if (Now.Recording) {
        auto tick = Now.Record.Ticks++;
        Now.RecordedStrength.resize(Now.Actions.size(), 0.0f);
        for (std::size_t i = 0; i < Now.Actions.size(); ++i) {
            if (Now.Actions[i].Strength == Now.RecordedStrength[i]) continue;
            Now.RecordedStrength[i] = Now.Actions[i].Strength;
            Now.Record.Changes.push_back({tick, int(i), Now.Actions[i].Strength});
        }
    }
}

} // namespace Detail

// ---------------------------------------------------------------- public API

namespace Input {

void Detail::Bind(int action, std::string_view name, std::initializer_list<Control> controls) {
    if (action < 0) {
        Log::Error("Input::Bind: action {} is negative", action);
        return;
    }
    for (std::size_t i = 0; i < Now.Actions.size(); ++i)
        if (Now.Actions[i].Bound && Now.Actions[i].Name == name && int(i) != action)
            Log::Warn("Input::Bind: two actions are called '{}'; saved controls will mix them", name);
    if (std::size_t(action) >= Now.Actions.size()) Now.Actions.resize(std::size_t(action) + 1);
    ActionData& data = Now.Actions[std::size_t(action)];
    data.Name = name;
    data.Bound = true;
    data.Defaults.assign(controls.begin(), controls.end());
    data.Controls = data.Defaults;
}

float Strength(int action) {
    ActionData* data = Find(action);
    return data ? data->Strength : 0.0f;
}
bool Down(int action) { return Strength(action) >= DownThreshold; }
bool Pressed(int action) {
    ActionData* data = Find(action);
    return data && data->Strength >= DownThreshold && data->Previous < DownThreshold;
}
bool Released(int action) {
    ActionData* data = Find(action);
    return data && data->Strength < DownThreshold && data->Previous >= DownThreshold;
}

float Axis(int negative, int positive) { return Strength(positive) - Strength(negative); }

Vec2 Axis(int left, int right, int up, int down) {
    Vec2 value{Axis(left, right), Axis(up, down)};
    float length = Length(value);
    return length > 1.0f ? value / length : value;
}

void Force(int action, bool down) {
    if (ActionData* data = Find(action)) data->Forced = down;
}

void ForceMouse(Vec2 position) {
    Now.MouseForced = true;
    BitMotor::Detail::InputMouseMove(position);
}

void ForceMouseButton(MouseButton button, bool down) {
    if (button >= MouseButton::Count) return;
    Now.Mouse[std::size_t(button)].Set(down);
}

void ReleaseMouse() { Now.MouseForced = false; }
bool MouseForced() { return Now.MouseForced; }

bool KeyDown(Key key) { return key < Key::Count && Now.Keys[std::size_t(key)].TickDown; }
bool KeyPressed(Key key) { return key < Key::Count && Now.Keys[std::size_t(key)].Pressed(); }
bool KeyReleased(Key key) { return key < Key::Count && Now.Keys[std::size_t(key)].Released(); }

Vec2 Mouse() { return Now.MousePosition; }
Vec2 MouseDelta() { return Now.TickMouseDelta; }
float MouseWheel() { return Now.TickWheel; }
bool MouseDown(MouseButton button) { return button < MouseButton::Count && Now.Mouse[std::size_t(button)].TickDown; }
bool MousePressed(MouseButton button) {
    return button < MouseButton::Count && Now.Mouse[std::size_t(button)].Pressed();
}
bool MouseReleased(MouseButton button) {
    return button < MouseButton::Count && Now.Mouse[std::size_t(button)].Released();
}

Vec2 LeftStick() { return Stick(false); }
Vec2 RightStick() { return Stick(true); }
bool ButtonDown(Button button) {
    if (button >= Button::Count) return false;
    float analog = Analog(button);
    return analog >= 0 ? analog >= DownThreshold : Now.Pad[std::size_t(button)].TickDown;
}
bool ButtonPressed(Button button) {
    return button < Button::Count && Analog(button) < 0 && Now.Pad[std::size_t(button)].Pressed();
}

bool UsingGamepad() { return Now.UsingGamepad; }
bool HasGamepad() { return Now.HasGamepad; }

void Rebind(int action) {
    if (!Find(action)) {
        Log::Warn("Input::Rebind: action {} has no Bind", action);
        return;
    }
    Now.RebindAction = action;
}
bool IsRebinding() { return Now.RebindAction >= 0; }
void CancelRebind() {
    if (Now.RebindAction >= 0) Now.RebindEnded = true;
    Now.RebindAction = -1;
}

std::string KeyName(int action) { return FirstName(action, false); }
std::string ButtonName(int action) { return FirstName(action, true); }

std::vector<Control> Controls(int action) {
    ActionData* data = Find(action);
    return data ? data->Controls : std::vector<Control>{};
}

bool LoadBindings(const std::filesystem::path& path) {
    Document document = BitMotor::Detail::OpenFile(BitMotor::Detail::SaveFilePath(path), false);
    if (!document.Ok()) return false;
    for (ActionData& action : Now.Actions) {
        if (!action.Bound || !document.Has(action.Name)) continue;
        std::vector<Control> controls;
        for (const std::string& name : document.Get<std::vector<std::string>>(action.Name)) {
            Control control = Key::Unknown;
            if (ControlFromName(name, control))
                controls.push_back(control);
            else
                document.Warn(action.Name, "unknown control '" + name + "' for '" + action.Name + "'");
        }
        action.Controls = std::move(controls);
    }
    return true;
}

bool SaveBindings(const std::filesystem::path& path) {
    std::map<std::string, std::vector<std::string>> file;
    for (const ActionData& action : Now.Actions) {
        if (!action.Bound) continue;
        auto& names = file[action.Name];
        for (const Control& control : action.Controls) names.push_back(Name(control));
    }
    return Data::Save(path, file);
}

void ResetBindings() {
    for (ActionData& action : Now.Actions) action.Controls = action.Defaults;
}

void StartRecording() {
    Now.Record = {};
    for (const ActionData& action : Now.Actions) Now.Record.Actions.push_back(action.Name);
    Now.RecordedStrength.assign(Now.Actions.size(), 0.0f);
    Now.Recording = true;
}

InputRecording StopRecording() {
    Now.Recording = false;
    return std::move(Now.Record);
}

bool IsRecording() { return Now.Recording; }

void Replay(const InputRecording& recording) {
    for (std::size_t i = 0; i < recording.Actions.size() && i < Now.Actions.size(); ++i)
        if (recording.Actions[i] != Now.Actions[i].Name)
            Log::Warn("Input::Replay: action {} was '{}' when recorded and is '{}' now", i, recording.Actions[i],
                      Now.Actions[i].Name);
    Now.Playback = recording;
    std::stable_sort(Now.Playback.Changes.begin(), Now.Playback.Changes.end(),
                     [](const InputChange& a, const InputChange& b) { return a.Tick < b.Tick; });
    Now.PlaybackNext = 0;
    Now.PlaybackTick = 0;
    Now.PlaybackStrength.assign(Now.Actions.size(), 0.0f);
    Now.Replaying = true;
}

bool IsReplaying() { return Now.Replaying; }
void StopReplay() { Now.Replaying = false; }

} // namespace Input

} // namespace BitMotor
