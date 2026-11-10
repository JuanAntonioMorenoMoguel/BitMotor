#include "BitMotor/UI/UI.h"

#include "BitMotor/Core/Screen.h"
#include "BitMotor/Core/Time.h"
#include "BitMotor/Math/Functions.h"
#include "Core/InputInternal.h"
#include "Text/FontInternal.h"
#include "UI/UiInternal.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace BitMotor {

namespace {

constexpr float RepeatDelay = 0.35f, RepeatRate = 0.08f;
constexpr int NavCount = 6;

struct NavBinding {
    std::vector<Control> Controls;
    int Action = -1; // >= 0: the controls of this game action

    std::vector<Control> Current() const { return Action >= 0 ? Input::Controls(Action) : Controls; }
};

std::array<NavBinding, NavCount> DefaultNavigation() {
    return {{
        {{Key::Up, Key::W, Button::DpadUp, Button::LeftStickUp}},
        {{Key::Down, Key::S, Button::DpadDown, Button::LeftStickDown}},
        {{Key::Left, Key::A, Button::DpadLeft, Button::LeftStickLeft}},
        {{Key::Right, Key::D, Button::DpadRight, Button::LeftStickRight}},
        {{Key::Enter, Key::Space, Key::KeypadEnter, Button::South}},
        {{Key::Escape, Key::Backspace, Button::East}},
    }};
}

// The focus lives in one of two scopes: the screen, or the modal window.
struct Scope {
    int Focus = 0;
    int Count = 0, LastCount = 0;
    std::vector<bool> Adjustable, LastAdjustable; // sliders and toggles take Left/Right
};

struct UiState {
    UI::Theme Look;
    std::array<NavBinding, NavCount> Navigation = DefaultNavigation();
    std::function<void()> OnClick;

    // This frame's input.
    Vec2 Mouse;
    bool MouseMoved = false, Click = false, MouseDown = false, ClickTaken = false;
    bool RightClick = false, RightClickTaken = false;
    std::array<bool, NavCount> Fired{};
    std::array<float, NavCount> Held{}, Repeat{};
    bool Blocked = false;
    // The focused widget gets these (from Confirm and Left/Right).
    bool Confirm = false;
    int Adjust = 0;
    // Draw the focus: after the keyboard or the gamepad moved it (the mouse hides it).
    bool ShowFocus = false;
    // Off: the keyboard and gamepad never move the focus nor press widgets.
    bool KeyboardFocus = true;

    Scope Screen, Modal;
    bool InModal = false, ModalNow = false, ModalLast = false;
    int Dragging = -1;
    bool DraggingInModal = false;

    // The open Dropdown: it takes every click and the navigation until it closes.
    struct {
        bool Open = false, Seen = false, InModal = false;
        int Index = -1;
        int Hot = 0, Selected = 0;
        Rect List;
        float Row = 0;
        std::vector<std::string> Items;
        bool Click = false;   // this frame's click, kept for it
        bool Capture = false; // it was open when the frame started: the other widgets get no mouse
    } Drop;
    // The TextField being edited: it takes the keyboard until Enter, Escape or a click elsewhere.
    struct {
        bool Active = false, Seen = false, InModal = false;
        int Index = -1;
        int Caret = 0;    // in characters
        double Since = 0; // the caret blinks from here
    } Edit;
    // The popup of a widget made in another file (UI::JFxStyle menus): like the open Dropdown, it takes every
    // click and the navigation until it closes.
    struct {
        bool Open = false, Seen = false, Used = false;
        bool Capture = false; // it was open when the frame started
        Detail::PopupInput Input;
    } Pop;
    std::vector<std::function<void()>> DrawLast;
    // The navigation the open Dropdown or the edited TextField gets instead of the rest.
    std::array<bool, NavCount> Captured{};

    // The widget drawn last, for Tooltip.
    Rect LastArea;
    bool LastHovered = false, LastFocused = false;
    std::string Tip;
    Vec2 TipAt;
    bool TipBelow = false;
} Ui;

Scope& Active() { return Ui.ModalLast ? Ui.Modal : Ui.Screen; }
Scope& Current() { return Ui.InModal ? Ui.Modal : Ui.Screen; }
// Widgets react only in the scope that has the input: the modal window while there is one.
bool Live() { return !Ui.Blocked && Ui.InModal == Ui.ModalLast; }

TextStyle Style(Color color) {
    TextStyle style = Ui.Look.Text;
    style.Color = color;
    style.Layer = Layer::Hud;
    style.Depth = 0;
    return style;
}

float CapHeight(const TextStyle& style) { return Text::Measure("H", style).Y; }

// Text centred vertically in the area, from its left (Align::Left), centre or right.
void Label(Rect area, std::string_view text, Color color, Align align) {
    TextStyle style = Style(color);
    style.Align = align;
    float y = std::floor(area.Y + area.Height * 0.5f - std::floor((CapHeight(style) - 1) * 0.5f));
    float x = align == Align::Left    ? area.X + Ui.Look.Padding
              : align == Align::Right ? area.Right() - Ui.Look.Padding
                                      : area.X + std::floor(area.Width * 0.5f);
    Text::Draw(text, {x, y}, style);
}

void Fire() {
    if (Ui.OnClick) Ui.OnClick();
}

// Corners kept, edges and centre stretched.
void DrawNineSlice(Rect area, const Region& image, float border, Color tint) {
    float b = Min(border, Min(image.Area.Width, image.Area.Height) * 0.5f);
    float screenB = Min(b / (image.Density > 0 ? image.Density : 1.0f), Min(area.Width, area.Height) * 0.5f);
    const float sx[3] = {image.Area.X, image.Area.X + b, image.Area.Right() - b};
    const float sw[3] = {b, image.Area.Width - 2 * b, b};
    const float sy[3] = {image.Area.Y, image.Area.Y + b, image.Area.Bottom() - b};
    const float sh[3] = {b, image.Area.Height - 2 * b, b};
    const float dx[3] = {area.X, area.X + screenB, area.Right() - screenB};
    const float dw[3] = {screenB, area.Width - 2 * screenB, screenB};
    const float dy[3] = {area.Y, area.Y + screenB, area.Bottom() - screenB};
    const float dh[3] = {screenB, area.Height - 2 * screenB, screenB};
    for (int j = 0; j < 3; ++j)
        for (int i = 0; i < 3; ++i) {
            if (sw[i] <= 0 || sh[j] <= 0 || dw[i] <= 0 || dh[j] <= 0) continue;
            Region piece{image.Source, Rect(sx[i], sy[j], sw[i], sh[j]), {0, 0}, 1};
            Screen::Draw(Sprite2D{.Region = piece,
                                  .Position = {dx[i], dy[j]},
                                  .Scale = {dw[i] / sw[i], dh[j] / sh[j]},
                                  .Origin = Vec2(0, 0),
                                  .Color = tint,
                                  .Layer = Layer::Hud});
        }
}

// A box: the nine-slice image tinted, or flat with a border.
void Box(Rect area, const Region& image, Color fill, Color border) {
    if (image) {
        DrawNineSlice(area, image, Ui.Look.ImageBorder, fill);
        if (border != Ui.Look.Border) UI::RectOutline(area, border);
        return;
    }
    UI::Rect(area, fill);
    UI::RectOutline(area, border);
}

// A focusable widget takes the next number of its scope. Returns it and whether it has the focus.
int Register(bool adjustable, bool& focused) {
    Scope& scope = Current();
    int index = scope.Count++;
    scope.Adjustable.push_back(adjustable);
    focused = Live() && index == scope.Focus;
    return index;
}

// A disabled widget still takes the click, so it does not fall through to what is under it.
void Swallow(BitMotor::Rect area) {
    if (Ui.Click && !Ui.ClickTaken && Live() && area.Contains(Ui.Mouse)) Ui.ClickTaken = true;
}

// The mouse moving onto a widget gives it the focus.
void Hover(int index, bool hovered) {
    if (hovered && Ui.MouseMoved) {
        Current().Focus = index;
        Ui.ShowFocus = false;
    }
}

void Remember(Rect area, bool hovered, bool focused) {
    Ui.LastArea = area;
    Ui.LastHovered = hovered;
    Ui.LastFocused = focused;
}

void UpdateNavigation(float dt) {
    for (int n = 0; n < NavCount; ++n) {
        bool down = false, pressed = false;
        for (const Control& control : Ui.Navigation[std::size_t(n)].Current()) {
            down = down || Detail::FrameDown(control);
            pressed = pressed || Detail::FramePressed(control);
        }
        bool fire = pressed;
        float& held = Ui.Held[std::size_t(n)];
        float& repeat = Ui.Repeat[std::size_t(n)];
        bool direction = n <= int(Nav::Right);
        if (pressed || !down) {
            held = 0;
            repeat = 0;
        } else if (direction) {
            float before = held;
            held += dt;
            if (before < RepeatDelay && held >= RepeatDelay) {
                fire = true;
                repeat = 0;
            } else if (held >= RepeatDelay) {
                repeat += dt;
                if (repeat >= RepeatRate) {
                    fire = true;
                    repeat = std::fmod(repeat, RepeatRate);
                }
            }
        }
        Ui.Fired[std::size_t(n)] = fire && !Ui.Blocked;
    }
}

// The open Dropdown's list, over everything.
void DrawDropList() {
    const UI::Theme& look = Ui.Look;
    const Rect list = Ui.Drop.List;
    UI::Rect(list, look.Panel.WithAlpha(1));
    UI::RectOutline(list, look.Accent);
    for (std::size_t i = 0; i < Ui.Drop.Items.size(); ++i) {
        Rect row(list.X + 1, list.Y + 1 + float(i) * Ui.Drop.Row, list.Width - 2, Ui.Drop.Row);
        if (int(i) == Ui.Drop.Hot) UI::Rect(row, look.ButtonHover);
        if (int(i) == Ui.Drop.Selected) UI::Rect({row.X, row.Y, 2, row.Height}, look.Accent);
        Label(row, Ui.Drop.Items[i], look.Text.Color, Align::Left);
    }
}

// Where the list of a dropdown goes: under it, or above if it does not fit below.
Rect DropListArea(Rect box, std::size_t count, float row) {
    const float height = float(count) * row + 2;
    Rect list(box.X, box.Bottom() + 1, box.Width, height);
    if (list.Bottom() > Screen::Height() && box.Y - height - 1 >= 0) list.Y = box.Y - height - 1;
    return list;
}

std::size_t Characters(std::string_view text) { return Detail::Decode(text).size(); }

// The UTF-8 text with `count` characters from `first` (in characters).
std::string Slice(const std::u32string& text, std::size_t first, std::size_t count = std::u32string::npos) {
    if (first >= text.size()) return {};
    return Detail::Encode(std::u32string_view(text).substr(first, count));
}

void StopEditing() {
    Ui.Edit.Active = false;
    Detail::SetTextInput(false);
}

} // namespace

namespace Detail {

void BeginUi() {
    // While waiting for a Rebind, and the key that finishes it, must not press widgets.
    Ui.Blocked = Input::IsRebinding() || RebindEndedThisFrame();

    Ui.Mouse = Input::Mouse();
    Ui.MouseMoved = LengthSquared(FrameMouseDelta()) > 0;
    Ui.Click = !Ui.Blocked && FramePressed(MouseButton::Left);
    Ui.MouseDown = FrameDown(MouseButton::Left);
    Ui.ClickTaken = false;
    Ui.RightClick = !Ui.Blocked && FramePressed(MouseButton::Right);
    Ui.RightClickTaken = false;
    UpdateNavigation(Time::RealDelta());

    // An open Dropdown or an edited TextField keeps the input for itself: nothing else moves or is pressed.
    Ui.Captured = {};
    Ui.Drop.Click = false;
    Ui.Drop.Capture = Ui.Drop.Open;
    Ui.Drop.Seen = Ui.Edit.Seen = false;
    Ui.Pop.Input = {};
    Ui.Pop.Capture = Ui.Pop.Open;
    Ui.Pop.Seen = Ui.Pop.Used = false;
    const bool captured = Ui.Drop.Open || Ui.Edit.Active || Ui.Pop.Open;
    if (captured) {
        Ui.Captured = Ui.Fired;
        Ui.Fired = {};
        if (Ui.Drop.Open && Ui.Click) {
            Ui.Drop.Click = true;
            Ui.ClickTaken = true;
        }
        Ui.RightClickTaken = Ui.Drop.Open;
        if (Ui.Pop.Open) {
            Ui.Pop.Input.Click = Ui.Click;
            Ui.Pop.Input.RightClick = Ui.RightClick;
            for (int n = 0; n < NavCount; ++n) Ui.Pop.Input.Nav[n] = Ui.Captured[std::size_t(n)];
            Ui.ClickTaken = Ui.RightClickTaken = true;
        }
    }

    // The focus moves among last frame's widgets of the scope that has the input.
    Scope& scope = Active();
    Ui.Confirm = false;
    Ui.Adjust = 0;
    if (scope.LastCount > 0 && Ui.KeyboardFocus && !captured) {
        auto at = [&](Nav nav) { return Ui.Fired[std::size_t(nav)]; };
        bool adjustable =
            scope.Focus >= 0 && scope.Focus < scope.LastCount && scope.LastAdjustable[std::size_t(scope.Focus)];
        int move = 0;
        if (at(Nav::Up)) --move;
        if (at(Nav::Down)) ++move;
        if (at(Nav::Left)) adjustable ? --Ui.Adjust : --move;
        if (at(Nav::Right)) adjustable ? ++Ui.Adjust : ++move;
        if (move != 0 || Ui.Adjust != 0 || at(Nav::Confirm)) Ui.ShowFocus = true;
        if (move != 0) scope.Focus = ((scope.Focus + move) % scope.LastCount + scope.LastCount) % scope.LastCount;
        Ui.Confirm = at(Nav::Confirm);
    }

    for (Scope* s : {&Ui.Screen, &Ui.Modal}) {
        s->Count = 0;
        s->Adjustable.clear();
    }
    Ui.InModal = false;
    Ui.ModalNow = false;
    Ui.Tip.clear();
    Ui.LastHovered = Ui.LastFocused = false;
    if (!Ui.MouseDown) Ui.Dragging = -1;
}

void EndUi() {
    if (Ui.InModal) UI::EndModal();
    if (Ui.Drop.Open && !Ui.Drop.Seen) Ui.Drop.Open = false; // its screen is gone
    if (Ui.Edit.Active && !Ui.Edit.Seen) {
        Ui.Edit.Active = false;
        SetTextInput(false);
    }
    if (Ui.Drop.Open) DrawDropList();
    // A popup nobody kept this frame, or one clicked outside, closes.
    const bool clickedOutside = (Ui.Pop.Input.Click || Ui.Pop.Input.RightClick) && !Ui.Pop.Used;
    if (Ui.Pop.Open && (!Ui.Pop.Seen || clickedOutside)) Ui.Pop.Open = false;
    for (const std::function<void()>& draw : Ui.DrawLast) draw();
    Ui.DrawLast.clear();
    if (Ui.ModalNow && !Ui.ModalLast) Ui.Modal.Focus = 0; // a window that just opened starts at its first widget
    Ui.ModalLast = Ui.ModalNow;
    for (Scope* s : {&Ui.Screen, &Ui.Modal}) {
        s->LastCount = s->Count;
        s->LastAdjustable = s->Adjustable;
        if (s->Count > 0) s->Focus = Clamp(s->Focus, 0, s->Count - 1);
    }

    if (Ui.Tip.empty()) return;
    const UI::Theme& look = Ui.Look;
    TextStyle style = Style(look.TooltipText.A > 0 ? look.TooltipText : look.Text.Color);
    if (look.TooltipLineHeight > 0) style.LineHeight = look.TooltipLineHeight;
    Vec2 size = Text::Measure(Ui.Tip, style, look.TooltipWidth);
    Rect box(Ui.TipAt + (Ui.TipBelow ? Vec2(0, 2) : look.TooltipOffset),
             size + Vec2(look.Padding * 2, look.Padding * 2));
    box.X = Clamp(box.X, 0.0f, Max(0.0f, Screen::Width() - box.Width));
    box.Y = Clamp(box.Y, 0.0f, Max(0.0f, Screen::Height() - box.Height));
    if (!Ui.TipBelow && box.Contains(Ui.Mouse)) box.Y = Max(0.0f, Ui.TipAt.Y - box.Height - 4); // not under the cursor
    UI::Rect(box, look.Tooltip);
    UI::RectOutline(box, look.TooltipBorder);
    Text::Paragraph(Ui.Tip, box.Position() + Vec2(look.Padding, look.Padding), look.TooltipWidth, style);
}

void ResetUi() { Ui = UiState{}; }

WidgetInput Interact(Rect area, bool enabled, bool adjustable) {
    WidgetInput in;
    if (!enabled) {
        Swallow(area);
        return in;
    }
    in.Index = Register(adjustable, in.Focused);
    in.Hovered = UI::Hovered(area);
    Hover(in.Index, in.Hovered);
    if (in.Hovered) in.Focused = Live() && in.Index == Current().Focus;
    in.Pressed = UI::Clicked(area) || (in.Focused && Ui.Confirm);
    in.ShowFocus = in.Focused && Ui.ShowFocus;
    in.Down = in.Hovered && Ui.MouseDown;
    if (in.Focused) in.Adjust = Ui.Adjust;
    return in;
}

bool NextFocused() { return Live() && Current().Count == Current().Focus; }

bool NextEditing() { return Ui.Edit.Active && Ui.Edit.Index == Current().Count && Ui.Edit.InModal == Ui.InModal; }

bool ShowingFocus() { return Ui.ShowFocus; }
Vec2 UiMouse() { return Ui.Mouse; }
bool UiMouseDown() { return Ui.MouseDown; }
bool UiMouseMoved() { return Ui.MouseMoved; }
void FireClick() { Fire(); }
void RememberWidget(Rect area, bool hovered, bool focused) { Remember(area, hovered, focused); }

void OpenPopup() {
    Ui.Pop.Open = Ui.Pop.Seen = true;
    Ui.Pop.Used = true; // the click that opened it
}

void KeepPopup() { Ui.Pop.Seen = true; }
void ClosePopup() { Ui.Pop.Open = false; }
bool PopupOpen() { return Ui.Pop.Open; }
const PopupInput& Popup() { return Ui.Pop.Input; }
void UsePopupClick() { Ui.Pop.Used = true; }
void DrawLast(std::function<void()> draw) { Ui.DrawLast.push_back(std::move(draw)); }

} // namespace Detail

namespace UI {

void SetTheme(const Theme& theme) { Ui.Look = theme; }
const Theme& GetTheme() { return Ui.Look; }

bool Hovered(BitMotor::Rect area) { return Live() && !Ui.Drop.Capture && !Ui.Pop.Capture && area.Contains(Ui.Mouse); }

bool Clicked(BitMotor::Rect area) {
    if (!Ui.Click || Ui.ClickTaken || !Hovered(area)) return false;
    Ui.ClickTaken = true;
    return true;
}

bool RightClicked(BitMotor::Rect area) {
    if (!Ui.RightClick || Ui.RightClickTaken || !Hovered(area)) return false;
    Ui.RightClickTaken = true;
    return true;
}

void OnClick(std::function<void()> callback) { Ui.OnClick = std::move(callback); }

void Panel(BitMotor::Rect area, std::string_view title) {
    const Theme& look = Ui.Look;
    Box(area, look.PanelImage, look.Panel, look.Border);
    if (!title.empty()) {
        TextStyle style = Style(look.Text.Color);
        Vec2 size = Text::Measure(title, style);
        Vec2 at(area.X + look.Padding + 2, area.Y - std::floor(size.Y * 0.5f));
        UI::Rect({at.X - 2, at.Y - 1, size.X + 4, size.Y + 2}, look.PanelImage ? look.Panel : look.Panel.WithAlpha(1));
        Text::Draw(title, at, style);
    }
    Remember(area, Hovered(area), false);
}

bool Button(BitMotor::Rect area, std::string_view label, bool enabled) {
    const Theme& look = Ui.Look;
    bool focused = false, hovered = false, pressed = false;
    if (enabled) {
        int index = Register(false, focused);
        hovered = Hovered(area);
        Hover(index, hovered);
        if (hovered) focused = Live() && index == Current().Focus;
        pressed = Clicked(area) || (focused && Ui.Confirm);
    } else {
        Swallow(area);
    }
    bool lit = hovered || (focused && Ui.ShowFocus);
    Color fill = !enabled                  ? look.Button.WithAlpha(look.Button.A * 0.5f)
                 : hovered && Ui.MouseDown ? look.ButtonDown
                 : lit                     ? look.ButtonHover
                                           : look.Button;
    Box(area, look.ButtonImage, fill, focused && Ui.ShowFocus ? look.Accent : look.Border);
    Label(area, label, enabled ? look.Text.Color : look.TextDisabled, Align::Center);
    Remember(area, hovered, focused);
    if (pressed) Fire();
    return pressed;
}

void Panel(BitMotor::Rect area, std::string_view title, Color tint) {
    const Theme& look = Ui.Look;
    if (look.PanelImage) {
        DrawNineSlice(area, look.PanelImage, look.ImageBorder, tint);
    } else {
        UI::Rect(area, look.Panel);
        UI::RectOutline(area, tint);
    }
    if (!title.empty()) {
        const float width = Text::Measure(title, Style(tint)).X;
        if (look.TitleTab) {
            UI::Rect({area.X + 5, area.Y - 3, width + 8, 9}, tint);
            UI::Rect({area.X + 5 + width + 8, area.Y - 3, 1, 9}, tint.WithAlpha(tint.A * 0.5f));
            Text::Draw(title, {area.X + 9, area.Y - 2}, Style(look.TitleBack));
        } else {
            UI::Rect({area.X + 6, area.Y - 1, width + 6, 3}, look.TitleBack);
            Text::Draw(title, {area.X + 9, area.Y - 2}, Style(tint));
        }
    }
    Remember(area, Hovered(area), false);
}

namespace {

// The pressing part of a tinted widget. Returns whether it is lit (pointed at or focused).
bool Press(BitMotor::Rect area, bool enabled, bool& pressed, bool& hovered, bool& focused) {
    pressed = hovered = focused = false;
    if (!enabled) {
        Swallow(area);
        return false;
    }
    int index = Register(false, focused);
    hovered = Hovered(area);
    Hover(index, hovered);
    if (hovered) focused = Live() && index == Current().Focus;
    pressed = Clicked(area) || (focused && Ui.Confirm);
    return hovered || (focused && Ui.ShowFocus);
}

void TintedLabel(BitMotor::Rect area, std::string_view label, Color color) { Label(area, label, color, Align::Center); }

// Pointed at, enabled or not.
void TintedTooltip(BitMotor::Rect area, std::string_view tooltip) {
    if (!tooltip.empty() && Hovered(area)) ShowTooltip(tooltip);
}

} // namespace

bool Button(BitMotor::Rect area, std::string_view label, Color tint, bool enabled, std::string_view tooltip) {
    const Theme& look = Ui.Look;
    bool pressed, hovered, focused;
    const bool lit = Press(area, enabled, pressed, hovered, focused);
    const Color frame = !enabled ? look.Faint : lit ? look.TextHover : tint;
    if (look.ButtonImage)
        DrawNineSlice(area, look.ButtonImage, look.ImageBorder, frame);
    else {
        UI::Rect(area, look.Button);
        UI::RectOutline(area, frame);
    }
    if (lit) UI::Rect(area.Expand(-2), tint.WithAlpha(tint.A * 0.18f));
    TintedLabel(area, label, !enabled ? look.TextDisabled : lit ? look.TextHover : tint);
    Remember(area, hovered, focused);
    TintedTooltip(area, tooltip);
    if (pressed) Fire();
    return pressed;
}

bool FlatButton(BitMotor::Rect area, std::string_view label, Color tint, bool enabled, std::string_view tooltip) {
    const Theme& look = Ui.Look;
    bool pressed, hovered, focused;
    const bool lit = Press(area, enabled, pressed, hovered, focused);
    if (lit) UI::Rect(area, tint.WithAlpha(tint.A * 0.25f));
    UI::RectOutline(area, !enabled ? look.Faint : lit ? look.TextHover : tint.WithAlpha(tint.A * 0.7f));
    TintedLabel(area, label, !enabled ? look.TextDisabled : lit ? look.TextHover : tint);
    Remember(area, hovered, focused);
    TintedTooltip(area, tooltip);
    if (pressed) Fire();
    return pressed;
}

bool Toggle(BitMotor::Rect area, std::string_view label, bool& value, bool enabled) {
    const Theme& look = Ui.Look;
    bool focused = false, hovered = false, changed = false;
    if (enabled) {
        int index = Register(true, focused);
        hovered = Hovered(area);
        Hover(index, hovered);
        if (hovered) focused = Live() && index == Current().Focus;
        if (Clicked(area) || (focused && (Ui.Confirm || Ui.Adjust != 0))) {
            value = !value;
            changed = true;
        }
    }
    if (hovered || (focused && Ui.ShowFocus)) UI::Rect(area, look.ButtonHover.WithAlpha(0.5f));
    if (focused && Ui.ShowFocus) UI::RectOutline(area, look.Accent);
    Color text = enabled ? look.Text.Color : look.TextDisabled;
    Label(area, label, text, Align::Left);
    float side = Max(5.0f, CapHeight(Style(text)) + 2);
    BitMotor::Rect box(area.Right() - look.Padding - side, area.Y + std::floor((area.Height - side) * 0.5f), side,
                       side);
    UI::Rect(box, look.BarBack);
    UI::RectOutline(box, enabled ? look.Border : look.TextDisabled);
    if (value) UI::Rect(box.Expand(-2), enabled ? look.Accent : look.TextDisabled);
    Remember(area, hovered, focused);
    if (changed) Fire();
    return changed;
}

bool Slider(BitMotor::Rect area, std::string_view label, float& value, float min, float max, float step, bool enabled) {
    const Theme& look = Ui.Look;
    if (max < min) std::swap(min, max);
    const float before = value;
    value = Clamp(value, min, max);
    bool focused = false, hovered = false;
    BitMotor::Rect track(area.X + std::floor(area.Width * 0.5f), area.Y + std::floor(area.Height * 0.5f) - 2,
                         std::floor(area.Width * 0.5f) - look.Padding, 4);
    if (enabled) {
        int index = Register(true, focused);
        hovered = Hovered(area);
        Hover(index, hovered);
        if (hovered) focused = Live() && index == Current().Focus;
        if (Clicked(area)) {
            Ui.Dragging = index;
            Ui.DraggingInModal = Ui.InModal;
            Current().Focus = index;
            Fire();
        }
        bool dragging = Live() && Ui.Dragging == index && Ui.DraggingInModal == Ui.InModal && Ui.MouseDown;
        if (dragging && track.Width > 0) {
            float t = Clamp((Ui.Mouse.X - track.X) / track.Width, 0.0f, 1.0f);
            value = min + t * (max - min);
            if (step > 0) value = Clamp(min + std::round((value - min) / step) * step, min, max);
        }
        if (focused && Ui.Adjust != 0) {
            float amount = step > 0 ? step : (max - min) * 0.05f;
            value = Clamp(value + amount * float(Ui.Adjust), min, max);
            if (step > 0) value = Clamp(min + std::round((value - min) / step) * step, min, max);
            if (value != before) Fire();
        }
    }
    if (hovered || (focused && Ui.ShowFocus)) UI::Rect(area, look.ButtonHover.WithAlpha(0.5f));
    if (focused && Ui.ShowFocus) UI::RectOutline(area, look.Accent);
    Label({area.X, area.Y, area.Width * 0.5f, area.Height}, label, enabled ? look.Text.Color : look.TextDisabled,
          Align::Left);
    float t = max > min ? (value - min) / (max - min) : 0.0f;
    Bar(track, t, enabled ? look.Accent : look.TextDisabled, look.BarBack);
    float knob = track.X + std::round(t * (track.Width - 3));
    UI::Rect({knob, track.Y - 2, 3, track.Height + 4}, enabled ? look.Text.Color : look.TextDisabled);
    Remember(area, hovered, focused);
    return value != before;
}

void Bar(BitMotor::Rect area, float fraction, Color color) { Bar(area, fraction, color, Ui.Look.BarBack); }

void Bar(BitMotor::Rect area, float fraction, Color color, Color back) {
    UI::Rect(area, back);
    const float t = Clamp(fraction, 0.0f, 1.0f);
    float filled = t > 0 ? Max(1.0f, std::round(area.Width * t)) : 0.0f; // a sliver shows it is not empty
    UI::Rect({area.X, area.Y, filled, area.Height}, color);
    Remember(area, Hovered(area), false);
}

void Segments(BitMotor::Rect area, float fraction, Color color, float segment) {
    Segments(area, fraction, color, Ui.Look.BarBack, segment);
}

void Segments(BitMotor::Rect area, float fraction, Color color, Color back, float segment) {
    segment = Max(segment, 1.0f);
    int count = Max(1, int((area.Width + 1) / (segment + 1)));
    int lit = int(std::round(Clamp(fraction, 0.0f, 1.0f) * float(count)));
    for (int i = 0; i < count; ++i)
        UI::Rect({area.X + float(i) * (segment + 1), area.Y, segment, area.Height}, i < lit ? color : back);
    Remember(area, Hovered(area), false);
}

void Image(BitMotor::Rect area, const Region& image, Color tint) {
    Vec2 size = image.Size();
    if (image && size.X > 0 && size.Y > 0)
        Screen::Draw(Sprite2D{.Region = image,
                              .Position = area.Position(),
                              .Scale = {area.Width / size.X, area.Height / size.Y},
                              .Origin = Vec2(0, 0),
                              .Color = tint,
                              .Layer = Layer::Hud});
    Remember(area, Hovered(area), false);
}

void NineSlice(BitMotor::Rect area, const Region& image, float border, Color tint) {
    if (image) DrawNineSlice(area, image, border, tint);
}

void ShowTooltip(std::string_view text) {
    Ui.Tip = text;
    Ui.TipAt = Ui.Mouse;
    Ui.TipBelow = false;
}

void Tooltip(std::string_view text) {
    if (Ui.LastHovered) {
        Ui.Tip = text;
        Ui.TipAt = Ui.Mouse;
        Ui.TipBelow = false;
    } else if (Ui.LastFocused && Ui.ShowFocus) {
        Ui.Tip = text;
        Ui.TipAt = {Ui.LastArea.X, Ui.LastArea.Bottom()};
        Ui.TipBelow = true;
    }
}

float Prompt(Vec2 position, int action, std::string_view label) {
    const Theme& look = Ui.Look;
    std::string name = Input::UsingGamepad() ? Input::ButtonName(action) : Input::KeyName(action);
    TextStyle style = Style(look.Text.Color);
    Vec2 size = Text::Measure(name, style);
    BitMotor::Rect key(position, size + Vec2(4, 4));
    UI::Rect(key, look.Button);
    UI::RectOutline(key, look.Border);
    Text::Draw(name, position + Vec2(2, 2), style);
    float width = key.Width;
    if (!label.empty()) {
        Text::Draw(label, {key.Right() + 3, position.Y + 2}, style);
        width += 3 + Text::Measure(label, style).X;
    }
    return width;
}

void BeginModal(BitMotor::Rect area, std::string_view title) {
    if (Ui.InModal) EndModal();
    UI::Rect({0, 0, Screen::Width(), Screen::Height()}, Ui.Look.ModalShade);
    Ui.InModal = true;
    Ui.ModalNow = true;
    Panel(area, title);
}

void BeginModal() {
    if (Ui.InModal) EndModal();
    Ui.InModal = true;
    Ui.ModalNow = true;
}

void EndModal() { Ui.InModal = false; }

bool Dropdown(BitMotor::Rect area, std::string_view label, int& selected, std::span<const std::string> options,
              bool enabled) {
    const Theme& look = Ui.Look;
    const int count = int(options.size());
    const int before = selected;
    if (count > 0) selected = Clamp(selected, 0, count - 1);
    BitMotor::Rect box = label.empty() ? area
                                       : BitMotor::Rect(area.X + std::floor(area.Width * 0.5f), area.Y,
                                                        area.Width - std::floor(area.Width * 0.5f), area.Height);
    bool focused = false, hovered = false;
    if (enabled && count > 0) {
        const int index = Register(true, focused);
        auto& drop = Ui.Drop;
        const bool open = drop.Open && drop.Index == index && drop.InModal == Ui.InModal;
        if (open) {
            drop.Seen = true;
            focused = true;
            drop.Row = std::floor(Max(area.Height - 2, CapHeight(Style(look.Text.Color)) + 4));
            drop.List = DropListArea(box, options.size(), drop.Row);
            drop.Items.assign(options.begin(), options.end());
            drop.Selected = selected;
            auto rowAt = [&](Vec2 point) {
                if (!drop.List.Contains(point)) return -1;
                return Clamp(int((point.Y - drop.List.Y - 1) / drop.Row), 0, count - 1);
            };
            if (Ui.MouseMoved)
                if (int row = rowAt(Ui.Mouse); row >= 0) drop.Hot = row;
            auto nav = [&](Nav n) { return Ui.Captured[std::size_t(n)]; };
            if (nav(Nav::Up)) drop.Hot = (drop.Hot + count - 1) % count;
            if (nav(Nav::Down)) drop.Hot = (drop.Hot + 1) % count;
            Ui.ShowFocus = Ui.ShowFocus || nav(Nav::Up) || nav(Nav::Down);
            if (drop.Click) {
                if (int row = rowAt(Ui.Mouse); row >= 0) selected = row;
                drop.Open = false;
                Fire();
            } else if (nav(Nav::Confirm)) {
                selected = drop.Hot;
                drop.Open = false;
                Fire();
            } else if (nav(Nav::Back)) {
                drop.Open = false;
            }
            drop.Selected = selected;
            hovered = box.Contains(Ui.Mouse);
        } else {
            hovered = Hovered(area);
            Hover(index, hovered);
            if (hovered) focused = Live() && index == Current().Focus;
            if (Clicked(area) || (focused && Ui.Confirm)) {
                drop.Open = true;
                drop.Seen = true;
                drop.InModal = Ui.InModal;
                drop.Index = index;
                drop.Hot = selected;
                drop.Selected = selected;
                drop.Row = std::floor(Max(area.Height - 2, CapHeight(Style(look.Text.Color)) + 4));
                drop.List = DropListArea(box, options.size(), drop.Row);
                drop.Items.assign(options.begin(), options.end());
                Current().Focus = index;
                Fire();
            } else if (focused && Ui.Adjust != 0) {
                selected = (selected + Ui.Adjust + count) % count;
                Fire();
            }
        }
    } else {
        Swallow(area);
    }
    const bool lit = hovered || (focused && Ui.ShowFocus);
    if (!label.empty()) {
        if (lit) UI::Rect(area, look.ButtonHover.WithAlpha(0.5f));
        Label({area.X, area.Y, area.Width * 0.5f, area.Height}, label, enabled ? look.Text.Color : look.TextDisabled,
              Align::Left);
    }
    BitMotor::Rect field = label.empty() ? box : box.Expand(-1);
    Box(field, look.ButtonImage, lit ? look.ButtonHover : look.Button,
        focused && Ui.ShowFocus ? look.Accent : look.Border);
    const Color text = enabled ? look.Text.Color : look.TextDisabled;
    if (count > 0)
        Label({field.X, field.Y, field.Width - 8, field.Height}, options[std::size_t(selected)], text, Align::Left);
    // A small arrow at the right end.
    const float arrowX = field.Right() - look.Padding - 5, arrowY = std::floor(field.Y + field.Height * 0.5f) - 1;
    for (int i = 0; i < 3; ++i) UI::Rect({arrowX + float(i), arrowY + float(i), float(5 - 2 * i), 1}, text);
    Remember(area, hovered, focused);
    return selected != before;
}

bool Dropdown(BitMotor::Rect area, std::string_view label, int& selected,
              std::initializer_list<std::string_view> options, bool enabled) {
    std::vector<std::string> items(options.begin(), options.end());
    return Dropdown(area, label, selected, std::span<const std::string>(items), enabled);
}

bool TextField(BitMotor::Rect area, std::string_view label, std::string& text, const TextFieldOptions& options) {
    const Theme& look = Ui.Look;
    const std::string before = text;
    BitMotor::Rect box = label.empty() ? area
                                       : BitMotor::Rect(area.X + std::floor(area.Width * 0.5f), area.Y,
                                                        area.Width - std::floor(area.Width * 0.5f), area.Height);
    const TextStyle style = Style(options.Enabled ? look.Text.Color : look.TextDisabled);
    const float left = box.X + look.Padding, room = Max(1.0f, box.Width - look.Padding * 2);
    std::u32string letters = Detail::Decode(text);
    auto& edit = Ui.Edit;
    bool focused = false, hovered = false, active = false;

    // The character nearest to an x on screen, given the first one shown.
    auto caretAt = [&](float x, std::size_t first) {
        std::size_t best = first;
        float bestDistance = 1e9f;
        for (std::size_t i = first; i <= letters.size(); ++i) {
            float at = left + Text::Measure(Slice(letters, first, i - first), style).X;
            if (i > first) at += 1; // Measure ends at the last pixel; the caret goes after it
            if (float distance = std::abs(at - x); distance < bestDistance) {
                bestDistance = distance;
                best = i;
            }
        }
        return int(best);
    };
    // The first character shown: the end of the text (and the caret) must fit in the box.
    auto firstShown = [&](int caret) {
        std::size_t first = 0;
        while (first < std::size_t(caret) &&
               Text::Measure(Slice(letters, first, std::size_t(caret) - first), style).X > room - 2)
            ++first;
        return first;
    };

    if (options.Enabled) {
        const int index = Register(false, focused);
        const bool editing = edit.Active && edit.Index == index && edit.InModal == Ui.InModal;
        if (editing) {
            edit.Seen = true;
            focused = true;
            hovered = box.Contains(Ui.Mouse);
            edit.Caret = Clamp(edit.Caret, 0, int(letters.size()));
            const bool ctrl = Detail::FrameDown(Key::LeftCtrl) || Detail::FrameDown(Key::RightCtrl);
            bool done = false, moved = false;
            auto insert = [&](std::u32string_view add) {
                for (char32_t c : add) {
                    if (c < 0x20 || c == 0x7F) continue; // control characters
                    if (options.MaxLength > 0 && int(letters.size()) >= options.MaxLength) break;
                    letters.insert(letters.begin() + edit.Caret, c);
                    ++edit.Caret;
                }
                moved = true;
            };
            for (Key key : Detail::FrameKeys()) {
                switch (key) {
                case Key::Backspace:
                    if (edit.Caret > 0) letters.erase(std::size_t(--edit.Caret), 1);
                    moved = true;
                    break;
                case Key::Delete:
                    if (edit.Caret < int(letters.size())) letters.erase(std::size_t(edit.Caret), 1);
                    moved = true;
                    break;
                case Key::Left:
                    edit.Caret = Max(0, edit.Caret - 1);
                    moved = true;
                    break;
                case Key::Right:
                    edit.Caret = Min(int(letters.size()), edit.Caret + 1);
                    moved = true;
                    break;
                case Key::Home:
                case Key::Up:
                    edit.Caret = 0;
                    moved = true;
                    break;
                case Key::End:
                case Key::Down:
                    edit.Caret = int(letters.size());
                    moved = true;
                    break;
                case Key::Enter:
                case Key::KeypadEnter:
                case Key::Escape:
                    done = true;
                    break;
                case Key::V:
                    if (ctrl) insert(Detail::Decode(Detail::ClipboardText()));
                    break;
                case Key::C:
                    if (ctrl) Detail::SetClipboardText(Detail::Encode(letters));
                    break;
                default:
                    break;
                }
            }
            if (!ctrl) insert(Detail::Decode(Detail::FrameText()));
            // The gamepad (it has no keyboard): Confirm or Back finish.
            if (Ui.Captured[std::size_t(Nav::Confirm)] && !Detail::FrameDown(Key::Space) &&
                !Detail::FrameDown(Key::Enter))
                done = true;
            if (Ui.Captured[std::size_t(Nav::Back)] && !Detail::FrameDown(Key::Backspace) &&
                !Detail::FrameDown(Key::Escape))
                done = true;
            if (Ui.Click) {
                if (box.Contains(Ui.Mouse)) {
                    edit.Caret = caretAt(Ui.Mouse.X, firstShown(edit.Caret));
                    moved = true;
                    if (!Ui.ClickTaken) Ui.ClickTaken = true;
                } else {
                    done = true; // the click is free for what is under it
                }
            }
            if (moved) edit.Since = Time::RealTotal();
            text = Detail::Encode(letters);
            if (done) StopEditing();
            active = !done;
        } else {
            hovered = Hovered(box);
            Hover(index, hovered);
            if (hovered) focused = Live() && index == Current().Focus;
            const bool click = Clicked(box);
            if (click || (focused && Ui.Confirm)) {
                edit.Active = true;
                edit.Seen = true;
                edit.InModal = Ui.InModal;
                edit.Index = index;
                edit.Caret = click ? caretAt(Ui.Mouse.X, 0) : int(letters.size());
                edit.Since = Time::RealTotal();
                Current().Focus = index;
                Detail::SetTextInput(true);
                active = true;
                Fire();
            }
        }
    } else {
        Swallow(box);
    }

    const bool lit = hovered || (focused && Ui.ShowFocus) || active;
    if (!label.empty()) {
        if (hovered || (focused && Ui.ShowFocus)) UI::Rect(area, look.ButtonHover.WithAlpha(0.5f));
        Label({area.X, area.Y, area.Width * 0.5f, area.Height}, label, style.Color, Align::Left);
    }
    BitMotor::Rect field = label.empty() ? box : box.Expand(-1);
    if (options.Frame) {
        UI::Rect(field, look.BarBack);
        UI::RectOutline(field, active || (focused && Ui.ShowFocus) ? look.Accent : lit ? look.Text.Color : look.Border);
    }

    const float y = std::floor(field.Y + field.Height * 0.5f - std::floor((CapHeight(style) - 1) * 0.5f));
    letters = Detail::Decode(text);
    if (letters.empty() && !active) {
        if (!options.Placeholder.empty()) {
            TextStyle hint = style;
            hint.Color = look.TextDisabled;
            Text::Draw(options.Placeholder, {left, y}, hint);
        }
    } else {
        const int caret = active ? Clamp(edit.Caret, 0, int(letters.size())) : int(letters.size());
        const std::size_t first = active ? firstShown(caret) : 0;
        // As many characters from `first` as fit.
        std::size_t shown = letters.size() - first;
        while (shown > 0 && Text::Measure(Slice(letters, first, shown), style).X > room) --shown;
        std::string visible = Slice(letters, first, shown);
        if (options.Password) visible = std::string(Characters(visible), '*');
        Text::Draw(visible, {left, y}, style);
        if (active && std::fmod(Time::RealTotal() - edit.Since, 1.0) < 0.5) {
            std::string head = Slice(letters, first, std::size_t(caret) - first);
            if (options.Password) head = std::string(Characters(head), '*');
            float x = left + (head.empty() ? 0.0f : Text::Measure(head, style).X + 1);
            UI::Rect({x, y - 1, 1, CapHeight(style) + 2}, look.Accent);
        }
    }
    Remember(area, hovered, focused);
    return text != before;
}

bool Typing() { return Ui.Edit.Active; }

bool Pressed(Nav nav) { return Ui.Fired[std::size_t(nav)]; }

void SetNavigation(Nav nav, std::initializer_list<Control> controls) {
    Ui.Navigation[std::size_t(nav)] = {std::vector<Control>(controls), -1};
}

void SetNavigation(Nav nav, int action) { Ui.Navigation[std::size_t(nav)] = {{}, action}; }

void SetKeyboardFocus(bool on) {
    Ui.KeyboardFocus = on;
    if (!on) Ui.ShowFocus = false;
}

int Focus() {
    Scope& scope = Active();
    return scope.LastCount > 0 ? scope.Focus : -1;
}

void SetFocus(int index) { Active().Focus = Max(index, 0); }

} // namespace UI

} // namespace BitMotor
