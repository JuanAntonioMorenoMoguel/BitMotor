#pragma once
#include "BitMotor/Colors/Color.h"
#include "BitMotor/Core/Input.h"
#include "BitMotor/Math/Shapes.h"
#include "BitMotor/Render/Sprite.h"

#include <functional>
#include <span>
#include <vector>

namespace BitMotor::Detail {

// The loop calls them around the frame's Draw: BeginUi reads this frame's input (clicks, navigation with
// its repeat, which moves the focus), EndUi draws the tooltip and remembers what the widgets were.
void BeginUi();
void EndUi();
// Back to the defaults: theme, navigation controls, focus (tests).
void ResetUi();

// A rectangle of the white pixel: every shape is made of these.
void FillSpan(float left, float right, float y, float height, Color color, Layer layer, float depth);

// One row of a filled polygon: pixels [Left, Right) of row Y.
struct Span {
    float Left, Right, Y;
    bool operator==(const Span&) const = default;
};
std::vector<Span> ScanPolygon(std::span<const Vec2> points);

// ---------------------------------------------------------------- widgets made in other files (UI::JFxStyle)
// They follow the same focus, click and modal rules as UI's own widgets, so both mix on one screen.

struct WidgetInput {
    int Index = -1;
    bool Hovered = false;   // the mouse is over it
    bool Focused = false;   // it has the focus
    bool ShowFocus = false; // the focus is drawn (the keyboard or the gamepad moved it)
    bool Down = false;      // pointed at with the left button held
    bool Pressed = false;   // clicked, or confirmed with the focus on it, this frame
    int Adjust = 0;         // Left/Right on it (only adjustable widgets get them)
};
// Takes the next focus number, as UI::Button does (a disabled widget only swallows the click).
WidgetInput Interact(Rect area, bool enabled, bool adjustable = false);
// Whether the widget registered next will have the focus, and whether it is the TextField being typed in;
// for drawing behind a UI widget before calling it.
bool NextFocused();
bool NextEditing();
bool ShowingFocus();
Vec2 UiMouse();
bool UiMouseDown();
bool UiMouseMoved();
// Runs the UI::OnClick callback.
void FireClick();
// The widget drawn last, for UI::Tooltip.
void RememberWidget(Rect area, bool hovered, bool focused);

// One popup (a menu, the list of a combo box) at a time. While it is open it takes every click and the
// navigation when the frame starts: UI::Hovered and UI::Clicked see nothing, and its owner reads them with
// Popup(). It closes when a frame does not keep it, or when a click (either button) nobody used is outside.
struct PopupInput {
    bool Click = false, RightClick = false;
    bool Nav[6] = {}; // Up, Down, Left, Right, Confirm, Back, with repeat
};
void OpenPopup();
void KeepPopup();
void ClosePopup();
bool PopupOpen();
const PopupInput& Popup();
// The click was for the popup: it does not close it.
void UsePopupClick();
// Drawn at the end of the frame, over every widget (popups).
void DrawLast(std::function<void()> draw);

} // namespace BitMotor::Detail
