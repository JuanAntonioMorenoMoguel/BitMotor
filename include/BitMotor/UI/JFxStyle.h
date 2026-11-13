#pragma once
#include "BitMotor/UI/UI.h"

#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// UI::JFxStyle: aesthetics inspired by JavaFX (Modena).
// Only the look of a JavaFX desktop application: light grey controls with a soft vertical gradient, rounded
// corners, thin borders with a white inner highlight, soft shadows under menus and dialogs, a blue focus
// ring with a faint halo, blue selections and roomy spacing. It is immediate like the rest of UI: call the
// widgets every frame in Draw and each one draws itself and answers there. There is no scene graph, no CSS
// and no nodes; the colours are a struct. It shares UI's focus, clicks, modal windows, tooltips and OnClick,
// so JFxStyle widgets and UI widgets mix on one screen.
//     namespace Fx = UI::JFxStyle;
//     void Start() override { UI::SetTheme(Fx::Theme()); } // UI widgets and tooltips in the same colours
//     void Draw() override {
//         Fx::Background();
//         Fx::BeginMenuBar({0, 0, Screen::Width(), 24});
//         if (Fx::Menu(Tr("menu.file"))) {
//             if (Fx::MenuItem(Tr("menu.open"), "Ctrl+O")) Open();
//             Fx::MenuSeparator();
//             if (Fx::MenuItem(Tr("menu.quit"))) Quit();
//             Fx::EndMenu();
//         }
//         Fx::EndMenuBar();
//         Rect left, right;
//         Fx::SplitPane({0, 24, Screen::Width(), Screen::Height() - 24}, split, left, right);
//         if (Fx::Button({left.X + 8, left.Y + 8, 90, 26}, Tr("play"))) Play();
//         UI::Tooltip(Tr("play.tip"));
//         Fx::CheckBox({left.X + 8, left.Y + 42, 160, 22}, Tr("loop"), loop);
//     }
// Made for editors and tools: a logical resolution like 1280x720 and a TrueType font in Palette::Text
// (Font::Load) look closest to JavaFX, but it works with Font::Pixel at any size. Everything goes to the Hud
// layer, with its edges on whole pixels and its rounded corners smoothed.
namespace BitMotor::UI::JFxStyle {

// The colours, like Modena's looked-up colours (-fx-base, -fx-accent...). The defaults are Modena's own
// (what Modena() gives); Modena(base, accent) makes them from other colours. Change any of them afterwards.
struct Palette {
    // The font and size of every label (its Color is not used: see TextColor).
    TextStyle Text = {};
    // -fx-base: the controls (buttons, check boxes, menu bars) are shades of it.
    Color Base = Color::Hex(0xECECEC);
    // -fx-background: the window, panels, tab contents.
    Color Background = Color::Hex(0xF4F4F4);
    // -fx-control-inner-background: text fields, lists, the inside of check boxes.
    Color InnerBackground = Colors::White;
    // The base of a control under the mouse (-fx-hover-base) and pressed (-fx-pressed-base).
    Color Hover = Color::Hex(0xF8F8F8);
    Color Pressed = Color::Hex(0xDEDEDE);
    // Borders: of controls (-fx-outer-border), of text fields (-fx-text-box-border) and of boxes, panels and
    // dividers (-fx-box-border).
    Color OuterBorder = Color::Hex(0xB6B6B6);
    Color TextBoxBorder = Color::Hex(0xCFCFCF);
    Color BoxBorder = Color::Hex(0xC9C9C9);
    // Text on controls and backgrounds, on the accent (selections, menu items under the mouse) and the grey
    // text of an empty TextField.
    Color TextColor = Color::Hex(0x333333);
    Color SelectedText = Colors::White;
    Color PromptText = Color::Hex(0xA6A6A6);
    // The check of a CheckBox, the dot of a RadioButton, menu arrows.
    Color Mark = Color::Hex(0x575757);
    // -fx-accent: selections, menu items under the mouse, progress bars.
    Color Accent = Color::Hex(0x0096C9);
    // A selection in a list that does not have the focus.
    Color SelectionUnfocused = Color::Hex(0xD3D3D3);
    // -fx-focus-color: the ring of the focused widget, and -fx-faint-focus-color: the halo around it.
    Color Focus = Color::Hex(0x039ED3);
    Color FaintFocus = Color::Hex(0x039ED3, 0.13f);
    // -fx-default-button: the base of DefaultButton.
    Color DefaultButton = Color::Hex(0xABD8ED);
    // Under menus, combo box lists and dialogs; ShadowSize: how far it spreads, in pixels.
    Color Shadow = Colors::Black.WithAlpha(0.2f);
    float ShadowSize = 8;
    // Tooltips: dark, rounded, with light text.
    Color TooltipBack = Color::Hex(0x1E1E1E, 0.8f);
    Color TooltipText = Colors::White;
    // Behind a dialog, over everything else.
    Color ModalShade = Colors::Black.WithAlpha(0.3f);
    // Corners of controls (-fx-background-radius), in pixels.
    float Radius = 3;
    // Space between a control's edge and its text, left and right.
    float Padding = 8;
    // How visible a disabled widget is (JavaFX: -fx-opacity 0.4).
    float DisabledOpacity = 0.4f;
};

// Modena's colours for a base and an accent. Any base gives the same family of shades, like -fx-base in a
// JavaFX stylesheet; a dark base gives a dark look with light text.
//     Fx::SetPalette(Fx::Modena(Color::Hex(0x3C3F41), Color::Hex(0x4A88C7)));
Palette Modena(Color base = Color::Hex(0xECECEC), Color accent = Color::Hex(0x0096C9));
void SetPalette(const Palette& palette);
const Palette& GetPalette();
// A UI::Theme in the palette's colours, for UI's own widgets, tooltips and modal windows:
//     UI::SetTheme(Fx::Theme());
UI::Theme Theme();
// JavaFX's derive(color, amount): towards white (amount > 0) or black (amount < 0) in brightness, by
// amount in [-1, 1]. Modena makes its shades with it.
Color Derive(Color color, float amount);

// ---------------------------------------------------------------- shapes

// A rectangle with rounded corners (radius in pixels; 0: square), filled in one colour or in a vertical
// gradient from `top` to `bottom`. The edges stay on whole pixels and the corners are smoothed.
void RoundedRect(BitMotor::Rect area, float radius, Color color);
void RoundedRect(BitMotor::Rect area, float radius, Color top, Color bottom);
// A border `thickness` pixels wide just inside the area, with rounded corners.
void RoundedOutline(BitMotor::Rect area, float radius, Color color, float thickness = 1);
// A soft shadow around the area (and a little lower, as if lit from above), `size` pixels wide. Draw it
// before what casts it.
void Shadow(BitMotor::Rect area, float radius, Color color, float size);
// A control's body as Modena draws it, for widgets of your own: the outer border, a white inner highlight
// and a gradient, all made from `base` (Palette::Base, Hover, Pressed, DefaultButton or any other colour).
void ControlBody(BitMotor::Rect area, Color base, float radius);
// The focus ring with its faint halo, just outside the area.
void FocusRing(BitMotor::Rect area, float radius);

// ---------------------------------------------------------------- widgets

// The window's background colour over the area (the whole screen when it is empty).
void Background(BitMotor::Rect area = {});
// Text centred vertically in the area, from its left (Align::Left), centre or right.
void Label(BitMotor::Rect area, std::string_view text, Align align = Align::Left);
// A line between sections: horizontal in a wide area, vertical in a tall one (through its middle).
void Separator(BitMotor::Rect area);
// True on the frame it is clicked (or confirmed with the focus on it).
bool Button(BitMotor::Rect area, std::string_view label, bool enabled = true);
// The button of a dialog that Enter means: tinted with Palette::DefaultButton.
bool DefaultButton(BitMotor::Rect area, std::string_view label, bool enabled = true);
// A button that stays pressed while `selected` (mute, solo, tools). True on the frame it changes.
bool ToggleButton(BitMotor::Rect area, std::string_view label, bool& selected, bool enabled = true);
// A box with a check and its label on the right. True on the frame `value` changes.
bool CheckBox(BitMotor::Rect area, std::string_view label, bool& value, bool enabled = true);
// One choice of a group: chosen when `selected == value`; choosing it sets `selected`.
//     Fx::RadioButton(a, "Sine", wave, 0); Fx::RadioButton(b, "Square", wave, 1);
bool RadioButton(BitMotor::Rect area, std::string_view label, int& selected, int value, bool enabled = true);
// A track with a round thumb over the whole area. Dragging, or Left/Right by `step`, change `value` within
// [min, max]. True on every frame it changes.
bool Slider(BitMotor::Rect area, float& value, float min = 0, float max = 1, float step = 0.05f, bool enabled = true);
// `fraction` 0..1 of the track filled with the accent.
void ProgressBar(BitMotor::Rect area, float fraction);
// UI::TextField with Modena's look (it edits the same way; the label, if any, goes on the left half).
bool TextField(BitMotor::Rect area, std::string_view label, std::string& text, const TextFieldOptions& options = {});
// The chosen option, and a list of all of them under it on a click or Confirm (the mouse or Up/Down and
// Confirm choose, Escape or a click outside close it). Left/Right go through the options without opening
// it. True on the frame `selected` changes.
bool ComboBox(BitMotor::Rect area, int& selected, std::span<const std::string> options, bool enabled = true);
bool ComboBox(BitMotor::Rect area, int& selected, std::initializer_list<std::string_view> options, bool enabled = true);
// A list of rows with a scroll bar when they do not fit. A click or Left/Right choose `selected`; the mouse
// wheel moves `scroll` (the first row shown). True on the frame `selected` changes.
bool ListView(BitMotor::Rect area, int& selected, std::span<const std::string> items, int& scroll, bool enabled = true);

// A box with a title bar (a TitledPane that is always open). Returns the area inside, for its widgets.
BitMotor::Rect Panel(BitMotor::Rect area, std::string_view title = {});
// Tabs along the top of the area: a click or Confirm on one sets `selected`. Returns the area of the
// selected tab's content, to draw it there.
//     Rect page = Fx::TabPane(area, tab, {Tr("tab.pattern"), Tr("tab.instruments")});
//     if (tab == 0) DrawPattern(page);
BitMotor::Rect TabPane(BitMotor::Rect area, int& selected, std::span<const std::string> labels);
BitMotor::Rect TabPane(BitMotor::Rect area, int& selected, std::initializer_list<std::string_view> labels);

enum class Orientation {
    Horizontal, // side by side, divided by a vertical bar
    Vertical,   // one over the other
};
// Two areas divided by a bar the mouse drags; `ratio` (0..1) is where the bar is. True while it moves.
bool SplitPane(BitMotor::Rect area, float& ratio, BitMotor::Rect& first, BitMotor::Rect& second,
               Orientation orientation = Orientation::Horizontal);

// A dialog over everything: the rest of the screen is shaded and does not react (UI::BeginModal). Returns
// the area inside, under its title.
BitMotor::Rect BeginDialog(BitMotor::Rect area, std::string_view title = {});
void EndDialog();

// ---------------------------------------------------------------- for editors

// A number with two arrows at its right: a click on an arrow, the wheel over it, Left/Right, or dragging up
// and down change it by `step` within [min, max]. True on the frame it changes.
//     Fx::Spinner(area, bpm, 20, 400);   Fx::Spinner(area, volume, 0, 1, 0.05f, 2);
bool Spinner(BitMotor::Rect area, int& value, int min, int max, int step = 1, bool enabled = true);
// The same for a float, shown with `decimals` digits after the point.
bool Spinner(BitMotor::Rect area, float& value, float min, float max, float step, int decimals = 2,
             bool enabled = true);
// A round knob (pan, filters): dragging up/down, the wheel or Left/Right turn it by `step` within [min, max].
// The arc shows the value from `origin` (min by default; 0 for a pan knob from -1 to 1). True while it changes.
bool Knob(BitMotor::Rect area, float& value, float min = 0, float max = 1, float step = 0.01f, bool enabled = true);
bool Knob(BitMotor::Rect area, float& value, float min, float max, float step, float origin, bool enabled = true);

// A view on something larger than the area, with scroll bars when it does not fit. `content`: its size;
// `scroll`: how far it has moved, in pixels (kept within range). Returns where the content's top-left corner
// goes, and cuts what is drawn to the view (Screen::PushClip) until EndScrollArea. The wheel scrolls up and
// down, with Shift left and right; with Ctrl held it does nothing (that is for zooming).
//     Vec2 at = Fx::BeginScrollArea(area, {columns * cell, rows * cell}, scroll);
//     DrawCells(at);
//     Fx::EndScrollArea();
Vec2 BeginScrollArea(BitMotor::Rect area, Vec2 content, Vec2& scroll, bool enabled = true);
void EndScrollArea();
// The part of the open scroll area that shows the content (without its bars).
BitMotor::Rect ScrollView();

// A grid of cells for editors (steps, a piano roll, a tracker, pixels), in a scroll area. The grid draws its
// background and lines and answers the mouse; the caller draws what is in the cells between BeginGrid and
// EndGrid, with GridCell to know where each one is.
struct Grid {
    Vec2 Cell = {16, 16};
    int Columns = 16, Rows = 16;
    // A stronger line every MajorColumns columns and MajorRows rows (0: none), and a slightly darker
    // background every other BandColumns columns (beats in a step sequencer; 0: none).
    int MajorColumns = 0, MajorRows = 0, BandColumns = 0;
    // How far it has scrolled, in pixels (the grid keeps it in range).
    Vec2 Scroll = {0, 0};
};
struct GridPoint {
    int Column = -1, Row = -1;
    bool Valid() const { return Column >= 0 && Row >= 0; }
    bool operator==(const GridPoint&) const = default;
};
// What the mouse did on the grid this frame.
struct GridInput {
    GridPoint Hovered;      // the cell under the mouse (not Valid outside)
    GridPoint Pressed;      // the left button went down on this cell
    GridPoint RightPressed; // the right button went down on this cell
    GridPoint DragFrom;     // while the left button is held after pressing on the grid: where it went down
    bool Dragging = false;  // the left button is held after pressing on the grid (Hovered is where it is now)
    bool Released = false;  // the left button went up after pressing on the grid (Hovered is where)
};
GridInput BeginGrid(BitMotor::Rect area, Grid& grid, bool enabled = true);
// The area of a cell of the grid being drawn (it can be outside the view: the clip cuts it).
BitMotor::Rect GridCell(int column, int row);
// A selection over the cells from `a` to `b` (in any order), in the accent colour.
void GridSelection(GridPoint a, GridPoint b);
void EndGrid();

// Drag and drop with a text payload (an instrument's or a pattern's name...). A drag starts when the left
// button goes down on the source's area and the mouse moves 4 pixels; `label` follows the pointer. True
// while this source is being dragged.
//     Fx::DragSource(row, "pattern:" + name, name);
//     if (std::string what; Fx::DropTarget(playlist, what)) Add(what);
bool DragSource(BitMotor::Rect area, std::string_view payload, std::string_view label = {});
// True on the frame a drag is let go over the area, with what it carried in `payload`. While a drag is over
// it, the area is outlined in the accent colour.
bool DropTarget(BitMotor::Rect area, std::string& payload);
// What is being dragged now (empty: nothing).
std::string_view Dragged();

// A tree of items as rows (a model's nodes, folders). Items go in order with their depth: an item's children
// come right after it, one deeper. An arrow opens and closes the items with children; `open` keeps one flag
// per item (it grows to fit, closed items hide their children). A click or Left/Right choose `selected`
// (as in a ListView) and Confirm opens or closes it; the wheel moves `scroll` (the first row shown). Dragging
// a row onto another
// fills `move` with both (`To` -1: onto the empty space under the rows). True on the frame `selected` or
// `move` changes.
//     std::vector<Fx::TreeItem> items = {{"root", 0}, {"torso", 1}, {"head", 2}, {"arm", 2}};
//     Fx::TreeMove move;
//     Fx::TreeView(area, node, items, open, scroll, move);
//     if (move.From >= 0) Reparent(move.From, move.To);
struct TreeItem {
    std::string Label;
    int Depth = 0;
};
struct TreeMove {
    int From = -1, To = -1;
};
bool TreeView(BitMotor::Rect area, int& selected, std::span<const TreeItem> items, std::vector<bool>& open, int& scroll,
              TreeMove& move, bool enabled = true);

// A colour chooser in the area: a saturation and brightness square, a hue bar, an alpha bar, and `swatches`
// (a palette) in a row under them, with the colour and its hex code. Dragging on any of them, or a click on
// a swatch, changes `color`. True on every frame it changes.
bool ColorPicker(BitMotor::Rect area, Color& color, std::span<const Color> swatches = {}, bool enabled = true);

// ---------------------------------------------------------------- menus
// One menu is open at a time, drawn over everything with a shadow. Inside, the mouse or Up/Down and
// Confirm choose an item; Escape or a click outside close it. Menu and BeginContextMenu are true while
// their menu is open: then give it its items and call EndMenu.

// A bar along the top of the window, for Menu.
void BeginMenuBar(BitMotor::Rect area);
void EndMenuBar();
// A title in the menu bar that opens its menu on a click or Confirm. While one is open, pointing at another
// title opens that one, and Left/Right go to the next.
bool Menu(std::string_view label, bool enabled = true);
// The menu that opens where the area is right-clicked.
bool BeginContextMenu(BitMotor::Rect area);
// A row of the open menu, with its shortcut on the right (only shown: the game reads its keys). True on
// the frame it is chosen, which closes the menu.
bool MenuItem(std::string_view label, std::string_view shortcut = {}, bool enabled = true);
// The same with a check, switching `checked`.
bool CheckMenuItem(std::string_view label, bool& checked, std::string_view shortcut = {}, bool enabled = true);
void MenuSeparator();
// A row of the open menu that opens a submenu beside it (pointing at it, a click, Right or Confirm); Left or
// Escape go back. True while its submenu is open: then give it its items and call EndMenu. One level only:
// inside a submenu it is false.
//     if (Fx::BeginSubMenu(Tr("menu.export"))) {
//         if (Fx::MenuItem("WAV")) ExportWav();
//         Fx::EndMenu();
//     }
bool BeginSubMenu(std::string_view label, bool enabled = true);
// Ends the menu (or the submenu) that is being given its items.
void EndMenu();

} // namespace BitMotor::UI::JFxStyle
