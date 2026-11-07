#pragma once
#include "BitMotor/Colors/Colors.h"
#include "BitMotor/Core/Input.h"
#include "BitMotor/Render/Texture.h"
#include "BitMotor/Text/Text.h"
#include "BitMotor/UI/Shapes.h"

#include <functional>
#include <initializer_list>
#include <string_view>

// Immediate interface: call the widgets every frame in Draw, and each one draws itself and answers there.
//     void Draw() override {
//         UI::Panel({60, 40, 200, 150}, Tr("options.title"));
//         if (UI::Button({70, 60, 180, 14}, Tr("options.back"))) Scenes::Pop();
//         if (UI::Slider({70, 80, 180, 14}, Tr("options.music"), settings.MusicVolume)) Settings::Apply();
//         UI::Tooltip(Tr("options.music.tip"));
//         UI::Toggle({70, 100, 180, 14}, Tr("options.fullscreen"), settings.Fullscreen);
//     }
// The mouse clicks and drags them; keyboard and gamepad move a focus through them in the order they are
// called (Up/Down, and Left/Right too on buttons) and Confirm presses the focused one; Left/Right change
// sliders and toggles. A click is taken by the first widget under it.
// Everything goes to the Hud layer, in logical pixels. The UI reads input per frame, so it also works when
// no Update runs (Time::SetScale(0) in a pause menu).
namespace BitMotor {

// Menu navigation. With repeat while held: Up, Down, Left and Right fire again after a moment.
enum class Nav { Up, Down, Left, Right, Confirm, Back };

namespace UI {

// How the widgets look. The default is flat colours and Font::Pixel, with no image needed; give regions of
// your atlas to draw panels and buttons as nine-slices (corners kept, edges and centre stretched).
struct Theme {
    TextStyle Text = {};
    Color TextDisabled = Colors::Gray;
    Color Panel = Color::Hex(0x10141C, 0.94f);
    Color Border = Color::Hex(0x5A6478);
    Color Button = Color::Hex(0x232A38);
    Color ButtonHover = Color::Hex(0x34405A);
    Color ButtonDown = Color::Hex(0x18202C);
    // The focused widget's border, the filled part of sliders and toggles.
    Color Accent = Colors::Gold;
    Color BarBack = Color::Hex(0x0A0D12);
    Color Tooltip = Color::Hex(0x05070A, 0.95f);
    // Behind a modal window, over everything else.
    Color ModalShade = Colors::Black.WithAlpha(0.55f);
    // Nine-slice images (empty: flat). Border: the size of their corners, in texture pixels.
    Region PanelImage = {};
    Region ButtonImage = {};
    float ImageBorder = 4;
    // Space between a widget's edge and its text.
    float Padding = 4;
    // Tooltips wrap at this width, a little away from the cursor, with their own border and line spacing
    // (0: the font's).
    float TooltipWidth = 160;
    Color TooltipBorder = Color::Hex(0x5A6478);
    Vec2 TooltipOffset = {8, 12};
    float TooltipLineHeight = 0;

    // Widgets with a colour of their own (the Panel, Button and FlatButton that take a `tint`): the tint
    // colours their frame (the nine-slice image too) and their label; pointed at, both turn TextHover.
    // Disabled, the frame is Faint and the label TextDisabled.
    Color TextHover = Colors::White;
    Color Faint = Color::Hex(0x2A303C);
    // A tinted panel's title: on a tab filled with the tint, in TitleBack (TitleTab), or cut into the top
    // border over a strip of TitleBack.
    bool TitleTab = false;
    Color TitleBack = Color::Hex(0x05070A);
};

void SetTheme(const Theme& theme);
const Theme& GetTheme();

// ---------------------------------------------------------------- widgets

// A frame to put widgets on, with an optional title cut into its top border.
void Panel(BitMotor::Rect area, std::string_view title = {});
// True on the frame it is clicked (or confirmed with the focus on it).
bool Button(BitMotor::Rect area, std::string_view label, bool enabled = true);
// A box that switches `value`. True on the frame it changes.
bool Toggle(BitMotor::Rect area, std::string_view label, bool& value, bool enabled = true);
// The label on the left, the track on the right half. Dragging or Left/Right (by `step`) change `value`
// within [min, max]. True on every frame it changes.
bool Slider(BitMotor::Rect area, std::string_view label, float& value, float min = 0, float max = 1, float step = 0.05f,
            bool enabled = true);
// The same in a colour of their own (see Theme: the tint colours the frame and the label). `tooltip` is
// shown while it is pointed at, also when it is disabled (to say why).
void Panel(BitMotor::Rect area, std::string_view title, Color tint);
bool Button(BitMotor::Rect area, std::string_view label, Color tint, bool enabled = true,
            std::string_view tooltip = {});
// A flat button for lists and choices: an outline in the tint, filled faintly while pointed at.
bool FlatButton(BitMotor::Rect area, std::string_view label, Color tint, bool enabled = true,
                std::string_view tooltip = {});
// A filled bar (health, loading): `fraction` 0..1 of it in `color`.
void Bar(BitMotor::Rect area, float fraction, Color color);
void Bar(BitMotor::Rect area, float fraction, Color color, Color back);
// The same in blocks of `segment` pixels with a 1-pixel gap, like an LED meter.
void Segments(BitMotor::Rect area, float fraction, Color color, float segment = 3);
void Segments(BitMotor::Rect area, float fraction, Color color, Color back, float segment = 3);
// A region stretched over the area (icons, portraits).
void Image(BitMotor::Rect area, const Region& image, Color tint = Colors::White);
// A region drawn as a nine-slice over the area: its corners (`border` texture pixels) keep their size, the
// edges and the centre stretch. What panels and buttons use for their images.
void NineSlice(BitMotor::Rect area, const Region& image, float border, Color tint = Colors::White);
// Text shown next to the cursor (or the focused widget) while the widget drawn just before is pointed at.
void Tooltip(std::string_view text);
// Text shown next to the cursor this frame, for your own widgets:
//     if (UI::Hovered(gauge)) UI::ShowTooltip(std::format("{:.0f} degrees", temperature));
void ShowTooltip(std::string_view text);
// "[Space] Jump" / "[Pad South] Jump": the action's control for the device in use, boxed, and a label.
// Returns its width.
float Prompt(Vec2 position, int action, std::string_view label = {});
template <Input::Detail::ActionId A> float Prompt(Vec2 position, A action, std::string_view label = {}) {
    return Prompt(position, int(action), label);
}

// A window over everything: between BeginModal and EndModal are the only widgets that react (from the next
// frame on), the rest of the screen is shaded behind it, and the focus moves into it.
void BeginModal(BitMotor::Rect area, std::string_view title = {});
// The same for a window the game draws itself: nothing is drawn, only the input is kept for the widgets
// until EndModal.
void BeginModal();
void EndModal();

// ---------------------------------------------------------------- your own widgets

// The mouse is over the area (and nothing modal is in the way).
bool Hovered(BitMotor::Rect area);
// The area was clicked this frame; takes the click, so widgets under it do not get it.
bool Clicked(BitMotor::Rect area);
// The same with the right button.
bool RightClicked(BitMotor::Rect area);
// Called whenever a widget is pressed or changed (for an interface sound).
void OnClick(std::function<void()> callback);

// ---------------------------------------------------------------- navigation

// This frame, with repeat for the directions. Read in Draw (it is per frame, not per Update).
bool Pressed(Nav nav);
// Replaces the controls of a navigation input. The defaults: arrows, WASD and the D-pad or left stick;
// Enter, Space or Pad South confirm; Escape, Backspace or Pad East go back.
void SetNavigation(Nav nav, std::initializer_list<Control> controls);
// Uses the controls of one of the game's actions instead (so remapping them changes the menus too).
void SetNavigation(Nav nav, int action);
template <Input::Detail::ActionId A> void SetNavigation(Nav nav, A action) { SetNavigation(nav, int(action)); }
// Which widget has the focus: its number in calling order this frame (-1: none). SetFocus moves it.
int Focus();
void SetFocus(int index);

} // namespace UI

} // namespace BitMotor
