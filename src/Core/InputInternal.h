#pragma once
#include "BitMotor/Core/Keys.h"
#include "BitMotor/Math/Vector.h"

union SDL_Event;

// Input is split in two: Input.cpp keeps the state and knows nothing about SDL (so the tests can feed it),
// InputSdl.cpp turns SDL events into these calls and owns the gamepads.
namespace BitMotor::Detail {

enum class PadAxis { LeftX, LeftY, RightX, RightY, LeftTrigger, RightTrigger, Count };

void ResetInput();
void InputKey(Key key, bool down);
void InputMouseButton(MouseButton button, bool down);
// Absolute position in logical pixels.
void InputMouseMove(Vec2 position);
void InputMouseWheel(float amount);
void InputPadButton(Button button, bool down); // digital buttons only (not triggers or stick directions)
void InputPadAxis(PadAxis axis, float value);  // sticks -1..1 (+Y down), triggers 0..1
void InputPadConnected(bool connected);
// Before each Update: turns everything since the previous one into this Update's state.
void InputTick();
// Once per frame, before the Updates: the same for the frame (the UI reads it in Draw).
void InputFrame();
bool FrameDown(Control control);
bool FramePressed(Control control);
Vec2 FrameMouseDelta();
float FrameWheel();
// A Rebind took a control (or was cancelled) during this frame's events.
bool RebindEndedThisFrame();

// SDL side (InputSdl.cpp).
void StartInput();
void StopInput();
void HandleInputEvent(const SDL_Event& event);

inline constexpr float StickDeadZone = 0.2f;

} // namespace BitMotor::Detail
