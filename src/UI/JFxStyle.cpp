// UI::JFxStyle: aesthetics inspired by JavaFX (Modena). Only the look: the widgets use UI's focus, clicks
// and modal windows (UiInternal.h), and every shape is made of the white pixel, like UI::Rect.
#include "BitMotor/UI/JFxStyle.h"

#include "BitMotor/Core/Log.h"
#include "BitMotor/Core/Screen.h"
#include "BitMotor/Core/Time.h"
#include "BitMotor/Math/Functions.h"
#include "Core/InputInternal.h"
#include "Text/FontInternal.h"
#include "UI/UiInternal.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace BitMotor::UI::JFxStyle {

namespace {

constexpr int NavUp = 0, NavDown = 1, NavLeft = 2, NavRight = 3, NavConfirm = 4, NavBack = 5;

Palette Current = Modena();
// Disabled widgets draw everything with Palette::DisabledOpacity.
float Opacity = 1;

struct Fading {
    explicit Fading(bool enabled) { Opacity = enabled ? 1.0f : Current.DisabledOpacity; }
    ~Fading() { Opacity = 1; }
    Fading(const Fading&) = delete;
    Fading& operator=(const Fading&) = delete;
};

Color Faded(Color color) { return color.WithAlpha(color.A * Opacity); }

// ---------------------------------------------------------------- shapes

// A rectangle on whole pixels whose top and bottom corners can have their own radius.
struct Shape {
    float X0 = 0, Y0 = 0, X1 = 0, Y1 = 0, Top = 0, Bottom = 0;
};

Shape MakeShape(BitMotor::Rect area, float top, float bottom) {
    Shape s{std::round(area.X), std::round(area.Y), std::round(area.Right()), std::round(area.Bottom()), 0, 0};
    const float most = Max(0.0f, Min(s.X1 - s.X0, s.Y1 - s.Y0) * 0.5f);
    s.Top = Clamp(top, 0.0f, most);
    s.Bottom = Clamp(bottom, 0.0f, most);
    return s;
}

bool Inside(const Shape& s, float x, float y) {
    if (x < s.X0 || x >= s.X1 || y < s.Y0 || y >= s.Y1) return false;
    const bool upper = y < (s.Y0 + s.Y1) * 0.5f;
    const float r = upper ? s.Top : s.Bottom;
    if (r <= 0) return true;
    const float cx = Clamp(x, s.X0 + r, s.X1 - r);
    const float cy = upper ? Max(y, s.Y0 + r) : Min(y, s.Y1 - r);
    const float dx = x - cx, dy = y - cy;
    return dx * dx + dy * dy <= r * r;
}

// How much of pixel (x, y) the shape covers: 4x4 samples.
float Coverage(const Shape& s, float x, float y) {
    int in = 0;
    for (int j = 0; j < 4; ++j)
        for (int i = 0; i < 4; ++i) in += Inside(s, x + (float(i) + 0.5f) * 0.25f, y + (float(j) + 0.5f) * 0.25f);
    return float(in) / 16.0f;
}

// Fills the shape (minus `hole`, when there is one) row by row, in a vertical gradient. Runs of pixels with
// the same coverage become one rectangle, and the rows away from the corners of a one-colour shape too.
void Fill(const Shape& s, Color top, Color bottom, const Shape* hole = nullptr) {
    if (s.X1 <= s.X0 || s.Y1 <= s.Y0) return;
    const float height = s.Y1 - s.Y0;
    // Columns and rows near the edges where coverage changes; the rest is the same in every row.
    float zone = std::ceil(Max(s.Top, s.Bottom));
    if (hole) zone = Max(zone, Max(hole->X0 - s.X0, s.X1 - hole->X1) + std::ceil(Max(hole->Top, hole->Bottom)));
    float zoneY = std::ceil(Max(s.Top, s.Bottom));
    if (hole) zoneY = Max(zoneY, Max(hole->Y0 - s.Y0, s.Y1 - hole->Y1) + std::ceil(Max(hole->Top, hole->Bottom)));
    const bool flat = top == bottom;
    auto cover = [&](float x, float y) { return Coverage(s, x, y) - (hole ? Coverage(*hole, x, y) : 0.0f); };

    for (float y = s.Y0; y < s.Y1;) {
        const Color color = flat ? top : top.Lerp(bottom, (y + 0.5f - s.Y0) / height);
        float rows = 1;
        if (flat && y >= s.Y0 + zoneY && y < s.Y1 - zoneY) rows = (s.Y1 - zoneY) - y; // the plain middle band
        // Runs of equal coverage along the row.
        float runStart = s.X0, runCover = -1;
        auto emit = [&](float end) {
            if (runCover > 0)
                Detail::FillSpan(runStart, end, y, rows, color.WithAlpha(color.A * runCover), Layer::Hud, 0);
        };
        auto add = [&](float x, float c) {
            if (c == runCover) return;
            emit(x);
            runStart = x;
            runCover = c;
        };
        const float leftEnd = Min(s.X0 + zone, s.X1), rightStart = Max(s.X1 - zone, leftEnd);
        for (float x = s.X0; x < leftEnd; ++x) add(x, cover(x, y));
        if (rightStart > leftEnd) add(leftEnd, cover(leftEnd, y));
        for (float x = rightStart; x < s.X1; ++x) add(x, cover(x, y));
        emit(s.X1);
        y += rows;
    }
}

void FillRounded(BitMotor::Rect area, float top, float bottom, Color from, Color to) {
    Fill(MakeShape(area, top, bottom), Faded(from), Faded(to));
}

void OutlineRounded(BitMotor::Rect area, float top, float bottom, Color color, float thickness) {
    const Shape outer = MakeShape(area, top, bottom);
    const Shape inner = MakeShape(area.Expand(-thickness), Max(0.0f, top - thickness), Max(0.0f, bottom - thickness));
    Fill(outer, Faded(color), Faded(color), &inner);
}

// Modena's control: the outer border, the inner highlight and the body, from one base colour.
void Body(BitMotor::Rect area, Color base, float top, float bottom) {
    FillRounded(area, top, bottom, Derive(base, -0.23f), Derive(base, -0.23f));
    FillRounded(area.Expand(-1), Max(0.0f, top - 1), Max(0.0f, bottom - 1), Derive(base, 0.6f), Derive(base, -0.05f));
    FillRounded(area.Expand(-2), Max(0.0f, top - 2), Max(0.0f, bottom - 2), Derive(base, 0.1f), Derive(base, -0.08f));
}

// ---------------------------------------------------------------- text

TextStyle Style(Color color) {
    TextStyle style = Current.Text;
    style.Color = Faded(color);
    style.Layer = Layer::Hud;
    style.Depth = 0;
    return style;
}

float CapHeight() { return Text::Measure("H", Style(Current.TextColor)).Y; }
float TextWidth(std::string_view text) { return text.empty() ? 0.0f : Text::Measure(text, Style(Current.TextColor)).X; }
// The height of a row in menus and lists.
float RowHeight() { return std::round(CapHeight() + Current.Padding * 1.75f); }

// The text cut with "..." so that it fits in `width`.
std::string Fit(std::string_view text, float width) {
    if (TextWidth(text) <= width) return std::string(text);
    std::u32string letters = Detail::Decode(text);
    while (!letters.empty()) {
        letters.pop_back();
        std::string cut = Detail::Encode(letters) + "...";
        if (TextWidth(cut) <= width) return cut;
    }
    return {};
}

// Text centred vertically in the area, `pad` pixels from its left or right edge.
void DrawText(BitMotor::Rect area, std::string_view text, Color color, Align align, float pad) {
    if (text.empty()) return;
    TextStyle style = Style(color);
    style.Align = align;
    const float y = std::floor(area.Y + area.Height * 0.5f - std::floor((CapHeight() - 1) * 0.5f));
    const float x = align == Align::Left    ? area.X + pad
                    : align == Align::Right ? area.Right() - pad
                                            : area.X + std::floor(area.Width * 0.5f);
    Text::Draw(text, {std::round(x), y}, style);
}

// ---------------------------------------------------------------- marks

void Check(BitMotor::Rect box, Color color) {
    const Vec2 unit[6] = {{0.20f, 0.52f}, {0.33f, 0.39f}, {0.44f, 0.51f},
                          {0.70f, 0.22f}, {0.83f, 0.35f}, {0.44f, 0.78f}};
    Vec2 points[6];
    for (int i = 0; i < 6; ++i) points[i] = {box.X + unit[i].X * box.Width, box.Y + unit[i].Y * box.Height};
    UI::Polygon(points, Faded(color));
}

// A small triangle pointing down, centred on `center`.
void Arrow(Vec2 center, float width, Color color) {
    const float half = std::round(width * 0.5f), height = std::round(width * 0.5f);
    const Vec2 a(std::round(center.X) - half, std::round(center.Y - height * 0.5f));
    UI::Triangle(a, {a.X + 2 * half, a.Y}, {a.X + half, a.Y + height}, Faded(color));
}

// ---------------------------------------------------------------- ids and dragging

std::uint64_t Hash(std::string_view kind, BitMotor::Rect area, std::string_view label = {}, int number = 0) {
    std::uint64_t h = 1469598103934665603ull;
    auto mix = [&](const void* data, std::size_t size) {
        const auto* bytes = static_cast<const unsigned char*>(data);
        for (std::size_t i = 0; i < size; ++i) h = (h ^ bytes[i]) * 1099511628211ull;
    };
    mix(kind.data(), kind.size());
    const float values[4] = {area.X, area.Y, area.Width, area.Height};
    mix(values, sizeof(values));
    mix(label.data(), label.size());
    mix(&number, sizeof(number));
    return h == 0 ? 1 : h;
}

// What the mouse is dragging (a slider thumb, a split bar, a scroll bar): 0 when nothing.
std::uint64_t DragId = 0;
float DragGrab = 0; // where the thumb was taken, from its start
// The list last clicked or moved with the keyboard: its selection is blue, the others' grey (in JavaFX,
// only the list with the focus shows the accent).
std::uint64_t ActiveList = 0;

void ReleaseDrag() {
    if (!Detail::UiMouseDown()) DragId = 0;
}

// ---------------------------------------------------------------- popups

struct Entry {
    enum Kind { Item, CheckItem, SubItem, Line } Type = Item;
    std::string Label, Shortcut;
    bool Enabled = true, Checked = false, Hot = false;
    float Y = 0, Height = 0; // from the top of the popup
    int Number = -1;         // its number among the rows that can be chosen (-1: a line)
};

// One popup box: the open menu, or the submenu beside it.
struct PopupLevel {
    Vec2 At;            // top-left corner of the popup
    float MinWidth = 0; // a combo box's list is as wide as the box
    float Width = 0;    // last frame's, for the mouse
    int Hot = -1;       // the item under the mouse or chosen with Up/Down
    int Items = 0, LastItems = 0;
    bool HasChecks = false;
    std::vector<Entry> Entries;
    float NextY = 0;
    float LabelWidth = 0, ShortcutWidth = 0;
};

struct MenuState : PopupLevel {
    bool Open = false;
    std::uint64_t Owner = 0;
    bool Chosen = false;
    // The menu bar being drawn and the bar menu that is open.
    bool InBar = false;
    BitMotor::Rect Bar;
    float BarX = 0;
    int BarIndex = 0, BarCount = 0;
    int OpenOrdinal = -1; // which bar menu is open (-1: the popup is not a bar menu)
    int SwitchTo = -1;    // Left/Right asked for this bar menu
    // The submenu (one level): the item whose submenu is open (-1: none), whether Up/Down/Confirm are for
    // it, whether it was given this frame, and its drawing, done right after the menu's.
    PopupLevel Sub;
    int SubOwner = -1;
    bool SubFocus = false, SubBuilt = false;
    bool HotIsSub = false; // last frame, the hot item opened a submenu (Right is for it, not the menu bar)
    bool NavUsed = false;  // Left or Escape closed the submenu: they do nothing else
    std::function<void()> SubDraw;
} Menus;

// The popup that MenuItem and the others add to now.
PopupLevel* Level = &Menus;

float PopupPad() { return std::round(Max(2.0f, Current.Padding * 0.5f)); }

void CloseSub() {
    Menus.SubOwner = -1;
    Menus.SubFocus = false;
}

// The UI closes the popup on a click outside, or when a frame does not keep it.
bool IsOpen(std::uint64_t id) {
    if (Menus.Open && !Detail::PopupOpen()) {
        Menus.Open = false;
        Menus.OpenOrdinal = -1;
        CloseSub();
    }
    return Menus.Open && Menus.Owner == id;
}

void OpenMenu(std::uint64_t id, Vec2 at, float minWidth, int hot, int ordinal = -1) {
    Menus.Open = true;
    Menus.Owner = id;
    Menus.At = {std::round(at.X), std::round(at.Y)};
    Menus.MinWidth = minWidth;
    Menus.Width = 0;
    Menus.Hot = hot;
    Menus.OpenOrdinal = ordinal;
    CloseSub();
    Detail::OpenPopup();
    Detail::FireClick();
}

void CloseMenu() {
    Menus.Open = false;
    Menus.OpenOrdinal = -1;
    CloseSub();
    Level = &Menus;
    Detail::ClosePopup();
}

void StartLevel(PopupLevel& level) {
    level.Entries.clear();
    level.Items = 0;
    level.HasChecks = false;
    level.NextY = PopupPad();
    level.LabelWidth = level.ShortcutWidth = 0;
}

void MoveHot(PopupLevel& level, const Detail::PopupInput& in) {
    const int count = level.LastItems;
    if (count <= 0) return;
    if (in.Nav[NavUp]) level.Hot = level.Hot < 0 ? count - 1 : (level.Hot + count - 1) % count;
    if (in.Nav[NavDown]) level.Hot = (level.Hot + 1) % count;
}

void BeginPopup() {
    Detail::KeepPopup();
    StartLevel(Menus);
    Level = &Menus;
    Menus.Chosen = false;
    Menus.SubBuilt = false;
    Menus.NavUsed = false;
    Menus.SubDraw = nullptr;
    const Detail::PopupInput& in = Detail::Popup();
    if (Menus.SubFocus && Menus.SubOwner >= 0) {
        // Up/Down are the submenu's; Left and Escape go back to the menu.
        if (in.Nav[NavLeft] || in.Nav[NavBack]) {
            CloseSub();
            Menus.NavUsed = true;
        }
        return;
    }
    MoveHot(Menus, in);
    // In a menu bar, Left and Right go to the menu beside it (Right opens a submenu instead, if there is one).
    const bool right = in.Nav[NavRight] && !Menus.HotIsSub;
    if (Menus.OpenOrdinal >= 0 && Menus.BarCount > 0 && (in.Nav[NavLeft] || right))
        Menus.SwitchTo = (Menus.OpenOrdinal + (in.Nav[NavLeft] ? -1 : 1) + Menus.BarCount) % Menus.BarCount;
}

// Whether the keyboard is on this level now.
bool KeyboardHere(const PopupLevel& level) { return (&level == &Menus.Sub) == Menus.SubFocus; }

bool PopupRow(Entry::Kind type, std::string_view label, std::string_view shortcut, bool enabled, bool checked) {
    PopupLevel& level = *Level;
    const float row = RowHeight();
    const int item = level.Items++;
    const BitMotor::Rect area(level.At.X, level.At.Y + level.NextY, level.Width, row);
    const Vec2 mouse = Detail::UiMouse();
    const Detail::PopupInput& in = Detail::Popup();
    if (Detail::UiMouseMoved() && area.Contains(mouse)) {
        level.Hot = enabled ? item : -1;
        if (&level == &Menus)
            CloseSub();
        else
            Menus.SubFocus = true;
    }
    bool chosen = false;
    if (in.Click && area.Contains(mouse)) {
        Detail::UsePopupClick();
        chosen = enabled;
    }
    if (in.Nav[NavConfirm] && level.Hot == item && enabled && KeyboardHere(level)) chosen = true;
    level.LabelWidth = Max(level.LabelWidth, TextWidth(label));
    level.ShortcutWidth = Max(level.ShortcutWidth, TextWidth(shortcut));
    level.HasChecks = level.HasChecks || type == Entry::CheckItem;
    level.Entries.push_back(
        {type, std::string(label), std::string(shortcut), enabled, checked, level.Hot == item, level.NextY, row, item});
    level.NextY += row;
    if (chosen) {
        Menus.Chosen = true;
        Detail::FireClick();
    }
    return chosen;
}

// The size of a popup from its rows, kept inside the screen. `beside`: a submenu's parent box, to go to its
// left when there is no room on the right.
BitMotor::Rect PlacePopup(PopupLevel& level, const BitMotor::Rect* beside = nullptr) {
    const float pad = Current.Padding, check = level.HasChecks ? std::round(CapHeight() + 6) : 0.0f;
    const float arrows =
        std::any_of(level.Entries.begin(), level.Entries.end(), [](const Entry& e) { return e.Type == Entry::SubItem; })
            ? pad * 2
            : 0.0f;
    const float width = std::round(
        Max(level.MinWidth, pad + check + level.LabelWidth +
                                (level.ShortcutWidth > 0 ? pad * 3 + level.ShortcutWidth : 0) + arrows + pad * 2));
    const float height = level.NextY + PopupPad();
    if (Screen::Width() > 0) {
        if (beside && level.At.X + width > Screen::Width()) level.At.X = beside->X - width + 1;
        level.At.X = Max(0.0f, Min(level.At.X, Screen::Width() - width));
    }
    if (Screen::Height() > 0) level.At.Y = Max(0.0f, Min(level.At.Y, Screen::Height() - height));
    level.Width = width;
    level.LastItems = level.Items;
    return {level.At, {width, height}};
}

// A small triangle pointing right, centred on `center`.
void ArrowRight(Vec2 center, float height, Color color) {
    const float half = std::round(height * 0.5f), width = std::round(height * 0.5f);
    const Vec2 a(std::round(center.X - width * 0.5f), std::round(center.Y) - half);
    UI::Triangle(a, {a.X + width, a.Y + half}, {a.X, a.Y + 2 * half}, Faded(color));
}

void DrawPopupBox(const Palette& p, BitMotor::Rect box, bool checks, const std::vector<Entry>& entries) {
    const Palette saved = Current;
    Current = p;
    const float check = checks ? std::round(CapHeight() + 6) : 0.0f;
    Shadow(box, 0, p.Shadow, p.ShadowSize);
    FillRounded(box, 0, 0, Derive(p.Base, -0.17f), Derive(p.Base, -0.30f));
    FillRounded(box.Expand(-1), 0, 0, p.InnerBackground, p.InnerBackground);
    for (const Entry& e : entries) {
        const BitMotor::Rect row(box.X + 1, box.Y + e.Y, box.Width - 2, e.Height);
        if (e.Type == Entry::Line) {
            UI::Rect({row.X + p.Padding * 0.5f, std::floor(row.Y + row.Height * 0.5f), row.Width - p.Padding, 1},
                     p.BoxBorder);
            continue;
        }
        const bool hot = e.Hot && e.Enabled;
        if (hot) FillRounded(row, 0, 0, p.Accent, p.Accent);
        Opacity = e.Enabled ? 1.0f : p.DisabledOpacity;
        const Color text = hot ? p.SelectedText : p.TextColor;
        if (e.Checked) {
            const float side = std::round(CapHeight() + 2);
            Check({row.X + p.Padding, std::round(row.Y + (row.Height - side) * 0.5f), side, side},
                  hot ? p.SelectedText : p.Mark);
        }
        DrawText({row.X + p.Padding + check, row.Y, row.Width, row.Height}, e.Label, text, Align::Left, 0);
        if (e.Type == Entry::SubItem)
            ArrowRight({row.Right() - p.Padding * 1.5f, row.Center().Y}, std::round(CapHeight() * 0.8f),
                       hot ? p.SelectedText : p.Mark);
        else
            DrawText(row, e.Shortcut, text, Align::Right, p.Padding * 2);
        Opacity = 1;
    }
    Current = saved;
}

void EndPopup() {
    const Palette p = Current;
    const BitMotor::Rect box = PlacePopup(Menus);
    const Detail::PopupInput& in = Detail::Popup();
    if ((in.Click || in.RightClick) && box.Contains(Detail::UiMouse())) Detail::UsePopupClick();
    if (!Menus.SubBuilt) CloseSub();
    Menus.HotIsSub = std::any_of(Menus.Entries.begin(), Menus.Entries.end(), [](const Entry& e) {
        return e.Type == Entry::SubItem && e.Number == Menus.Hot && e.Enabled;
    });
    if ((in.Nav[NavBack] && !Menus.NavUsed) || Menus.Chosen) CloseMenu();
    if (!Menus.Open) return;
    Detail::DrawLast([p, box, checks = Menus.HasChecks, entries = Menus.Entries, sub = Menus.SubDraw] {
        DrawPopupBox(p, box, checks, entries);
        if (sub) sub(); // the submenu over its menu
    });
}

// The end of a submenu (EndMenu inside BeginSubMenu).
void EndSubPopup() {
    PopupLevel& sub = Menus.Sub;
    const BitMotor::Rect parent(Menus.At, {Menus.Width, Menus.NextY});
    const BitMotor::Rect box = PlacePopup(sub, &parent);
    const Detail::PopupInput& in = Detail::Popup();
    if ((in.Click || in.RightClick) && box.Contains(Detail::UiMouse())) Detail::UsePopupClick();
    Level = &Menus;
    Menus.SubDraw = [p = Current, box, checks = sub.HasChecks, entries = sub.Entries] {
        DrawPopupBox(p, box, checks, entries);
    };
}

// ---------------------------------------------------------------- buttons

Color StateBase(Color base, const Detail::WidgetInput& in, bool open = false) {
    if (in.Down || open) return Derive(base, -0.06f);
    if (in.Hovered) return base == Current.Base ? Current.Hover : Derive(base, 0.4f);
    return base;
}

bool ButtonOf(BitMotor::Rect area, std::string_view label, bool enabled, Color base) {
    Fading fading(enabled);
    const Detail::WidgetInput in = Detail::Interact(area, enabled);
    ControlBody(area, StateBase(base, in), Current.Radius);
    if (in.ShowFocus) FocusRing(area, Current.Radius);
    DrawText(area, Fit(label, area.Width - 4), Current.TextColor, Align::Center, 0);
    Detail::RememberWidget(area, in.Hovered, in.Focused);
    if (in.Pressed) Detail::FireClick();
    return in.Pressed;
}

// The square or circle at the left of a CheckBox or RadioButton.
BitMotor::Rect MarkBox(BitMotor::Rect area) {
    const float side = std::round(CapHeight() + 6);
    return {std::round(area.X), std::round(area.Y + (area.Height - side) * 0.5f), side, side};
}

// The tabs and scroll bars of TabPane and ListView: Modena's thumb and tab colours.
void Tab(BitMotor::Rect area, bool selected, bool hovered) {
    const float r = Current.Radius;
    if (selected) {
        FillRounded(area, r, 0, Current.OuterBorder, Current.OuterBorder);
        FillRounded(area.Expand(-1), r - 1, 0, Derive(Current.Background, 0.6f), Current.Background);
        // Open at the bottom, into the content.
        BitMotor::Rect inside = area.Expand(-1);
        FillRounded({inside.X + 1, inside.Y + 1, inside.Width - 2, inside.Height + 1}, Max(0.0f, r - 2), 0,
                    Current.Background, Current.Background);
    } else {
        Body(area, hovered ? Current.Base : Derive(Current.Base, -0.06f), r, 0);
    }
}

} // namespace

// ---------------------------------------------------------------- palette

Color Derive(Color color, float amount) {
    // JavaFX's Color.deriveColor: the change is scaled by how bright the colour looks, then applied to the
    // brightness of its HSB form (brighter also takes saturation away).
    const float perceived =
        std::sqrt(color.R * color.R * 0.299f + color.G * color.G * 0.587f + color.B * color.B * 0.114f);
    float k = amount;
    if (amount > 0)
        k *= perceived > 0.85f  ? 1.6f
             : perceived > 0.6f ? 1.0f
             : perceived > 0.5f ? 0.9f
             : perceived > 0.4f ? 0.8f
             : perceived > 0.3f ? 0.7f
                                : 0.6f;
    else if (perceived < 0.2f)
        k *= 0.6f;
    k = Clamp(k, -1.0f, 1.0f);
    // RGB to HSB.
    const float high = Max(color.R, Max(color.G, color.B)), low = Min(color.R, Min(color.G, color.B));
    float hue = 0;
    if (high > low) {
        const float d = high - low;
        if (high == color.R)
            hue = std::fmod((color.G - color.B) / d + 6.0f, 6.0f);
        else if (high == color.G)
            hue = (color.B - color.R) / d + 2.0f;
        else
            hue = (color.R - color.G) / d + 4.0f;
    }
    float saturation = high > 0 ? (high - low) / high : 0.0f, brightness = high;
    if (k > 0) {
        saturation *= 1 - k;
        brightness += (1 - brightness) * k;
    } else {
        brightness *= k + 1;
    }
    saturation = Clamp(saturation, 0.0f, 1.0f);
    brightness = Clamp(brightness, 0.0f, 1.0f);
    // HSB to RGB.
    const float c = brightness * saturation, x = c * (1 - std::abs(std::fmod(hue, 2.0f) - 1)), m = brightness - c;
    // The six sectors of the hue circle: which channel is highest (c), second (x) and lowest (0).
    const int sector = int(hue) % 6;
    const float r = sector == 0 || sector == 5 ? c : sector == 1 || sector == 4 ? x : 0.0f;
    const float g = sector == 1 || sector == 2 ? c : sector == 0 || sector == 3 ? x : 0.0f;
    const float b = sector == 3 || sector == 4 ? c : sector == 2 || sector == 5 ? x : 0.0f;
    return {r + m, g + m, b + m, color.A};
}

namespace {

float Brightness(Color c) { return std::sqrt(c.R * c.R * 0.299f + c.G * c.G * 0.587f + c.B * c.B * 0.114f); }

// JavaFX's ladder() for text: light text on dark colours, dark text on light ones.
Color TextOn(Color background) {
    const float b = Brightness(background);
    return b < 0.45f ? Colors::White : b < 0.6f ? Colors::Black : Color::Hex(0x333333);
}

} // namespace

Palette Modena(Color base, Color accent) {
    Palette p;
    const bool standard = accent == Color::Hex(0x0096C9);
    p.Base = base;
    p.Background = Derive(base, 0.264f);
    const float b = Brightness(base);
    // Modena's derive(-fx-base, 80%) is white on a light base; on a dark one it would be a light grey under
    // light text, so dark bases get a field a little darker than the background instead.
    p.InnerBackground = b < 0.5f ? Derive(base, -0.15f) : Derive(base, 0.8f);
    p.Hover = Derive(base, b < 0.2f ? 0.2f : b < 0.35f ? 0.3f : 0.4f);
    p.Pressed = Derive(base, -0.06f);
    p.OuterBorder = Derive(base, -0.23f);
    p.TextBoxBorder = Derive(p.Background, -0.15f);
    p.BoxBorder = Derive(base, -0.15f);
    p.TextColor = TextOn(base);
    p.SelectedText = Brightness(accent) < 0.6f ? Colors::White : Colors::Black;
    p.PromptText = Derive(p.InnerBackground, Brightness(p.InnerBackground) < 0.5f ? 0.45f : -0.35f);
    p.Mark = b < 0.3f ? Colors::White : Derive(base, -0.63f);
    p.Accent = accent;
    p.SelectionUnfocused = b < 0.5f ? Derive(base, 0.3f) : Color::Hex(0xD3D3D3);
    p.Focus = standard ? Color::Hex(0x039ED3) : accent;
    p.FaintFocus = p.Focus.WithAlpha(0x22 / 255.0f);
    p.DefaultButton = standard ? Color::Hex(0xABD8ED) : base.Lerp(accent, 0.4f);
    p.Shadow = Colors::Black.WithAlpha(b < 0.4f ? 0.45f : 0.2f);
    return p;
}

void SetPalette(const Palette& palette) { Current = palette; }
const Palette& GetPalette() { return Current; }

UI::Theme Theme() {
    const Palette& p = Current;
    UI::Theme t;
    t.Text = p.Text;
    t.Text.Color = p.TextColor;
    t.TextDisabled = p.TextColor.WithAlpha(p.DisabledOpacity);
    t.Panel = p.Background;
    t.Border = p.OuterBorder;
    t.Button = p.Base;
    t.ButtonHover = p.Hover;
    t.ButtonDown = p.Pressed;
    t.Accent = p.Focus;
    t.BarBack = Derive(p.Background, -0.1f);
    t.Tooltip = p.TooltipBack;
    t.TooltipBorder = Colors::Transparent;
    t.TooltipText = p.TooltipText;
    t.ModalShade = p.ModalShade;
    t.Padding = std::round(p.Padding * 0.75f);
    t.TextHover = p.TextColor;
    t.Faint = p.BoxBorder;
    t.TitleBack = p.Background;
    return t;
}

// ---------------------------------------------------------------- shapes

void RoundedRect(BitMotor::Rect area, float radius, Color color) { FillRounded(area, radius, radius, color, color); }

void RoundedRect(BitMotor::Rect area, float radius, Color top, Color bottom) {
    FillRounded(area, radius, radius, top, bottom);
}

void RoundedOutline(BitMotor::Rect area, float radius, Color color, float thickness) {
    if (thickness > 0) OutlineRounded(area, radius, radius, color, thickness);
}

void Shadow(BitMotor::Rect area, float radius, Color color, float size) {
    // Layers growing outwards, each a little transparent: dark near the area, fading away from it.
    const int layers = int(std::round(size));
    if (layers <= 0 || color.A <= 0) return;
    const BitMotor::Rect lowered(area.X, area.Y + std::round(size * 0.35f), area.Width, area.Height);
    const Color layer = color.WithAlpha(color.A * 1.6f / float(layers));
    for (int i = layers; i >= 1; --i) {
        const float grow = float(i);
        FillRounded(lowered.Expand(grow), radius + grow, radius + grow, layer, layer);
    }
}

void ControlBody(BitMotor::Rect area, Color base, float radius) { Body(area, base, radius, radius); }

void FocusRing(BitMotor::Rect area, float radius) {
    OutlineRounded(area.Expand(2), radius + 2, radius + 2, Current.FaintFocus, 2);
    OutlineRounded(area, radius, radius, Current.Focus, 1);
}

// ---------------------------------------------------------------- widgets

void Background(BitMotor::Rect area) {
    if (area.Width <= 0 || area.Height <= 0) area = {0, 0, Screen::Width(), Screen::Height()};
    UI::Rect(area, Faded(Current.Background));
}

void Label(BitMotor::Rect area, std::string_view text, Align align) {
    DrawText(area, text, Current.TextColor, align, 0);
}

void Separator(BitMotor::Rect area) {
    if (area.Width >= area.Height)
        UI::Rect({area.X, std::floor(area.Y + area.Height * 0.5f), area.Width, 1}, Faded(Current.BoxBorder));
    else
        UI::Rect({std::floor(area.X + area.Width * 0.5f), area.Y, 1, area.Height}, Faded(Current.BoxBorder));
}

bool Button(BitMotor::Rect area, std::string_view label, bool enabled) {
    return ButtonOf(area, label, enabled, Current.Base);
}

bool DefaultButton(BitMotor::Rect area, std::string_view label, bool enabled) {
    return ButtonOf(area, label, enabled, Current.DefaultButton);
}

bool ToggleButton(BitMotor::Rect area, std::string_view label, bool& selected, bool enabled) {
    Fading fading(enabled);
    const Detail::WidgetInput in = Detail::Interact(area, enabled);
    if (in.Pressed) selected = !selected;
    const float r = Current.Radius;
    if (selected) {
        // Pressed in: a darker border and a body darker at the top.
        const Color base = in.Hovered ? Current.Base : Derive(Current.Base, -0.03f);
        FillRounded(area, r, r, Derive(Current.OuterBorder, -0.2f), Current.OuterBorder);
        FillRounded(area.Expand(-1), r - 1, r - 1, Derive(base, -0.22f), Derive(base, -0.11f));
    } else {
        ControlBody(area, StateBase(Current.Base, in), r);
    }
    if (in.ShowFocus) FocusRing(area, r);
    DrawText(area, Fit(label, area.Width - 4), Current.TextColor, Align::Center, 0);
    Detail::RememberWidget(area, in.Hovered, in.Focused);
    if (in.Pressed) Detail::FireClick();
    return in.Pressed;
}

bool CheckBox(BitMotor::Rect area, std::string_view label, bool& value, bool enabled) {
    Fading fading(enabled);
    const Detail::WidgetInput in = Detail::Interact(area, enabled, true);
    const bool changed = in.Pressed || in.Adjust != 0;
    if (changed) value = !value;
    const BitMotor::Rect box = MarkBox(area);
    ControlBody(box, StateBase(Current.Base, in), Current.Radius);
    if (in.ShowFocus) FocusRing(box, Current.Radius);
    if (value) Check(box, Current.Mark);
    DrawText({box.Right(), area.Y, area.Right() - box.Right(), area.Height}, label, Current.TextColor, Align::Left,
             std::round(Current.Padding * 0.5f));
    Detail::RememberWidget(area, in.Hovered, in.Focused);
    if (changed) Detail::FireClick();
    return changed;
}

bool RadioButton(BitMotor::Rect area, std::string_view label, int& selected, int value, bool enabled) {
    Fading fading(enabled);
    const Detail::WidgetInput in = Detail::Interact(area, enabled);
    const bool changed = in.Pressed && selected != value;
    if (in.Pressed) selected = value;
    const BitMotor::Rect box = MarkBox(area);
    const float round = box.Width * 0.5f;
    ControlBody(box, StateBase(Current.Base, in), round);
    if (in.ShowFocus) FocusRing(box, round);
    if (selected == value) {
        const float dot = std::round(box.Width * 0.36f);
        const BitMotor::Rect inner(std::round(box.Center().X - dot * 0.5f), std::round(box.Center().Y - dot * 0.5f),
                                   dot, dot);
        FillRounded(inner, dot * 0.5f, dot * 0.5f, Current.Mark, Current.Mark);
    }
    DrawText({box.Right(), area.Y, area.Right() - box.Right(), area.Height}, label, Current.TextColor, Align::Left,
             std::round(Current.Padding * 0.5f));
    Detail::RememberWidget(area, in.Hovered, in.Focused);
    if (in.Pressed) Detail::FireClick();
    return changed;
}

bool Slider(BitMotor::Rect area, float& value, float min, float max, float step, bool enabled) {
    Fading fading(enabled);
    ReleaseDrag();
    if (max < min) std::swap(min, max);
    const float before = value;
    value = Clamp(value, min, max);
    const std::uint64_t id = Hash("slider", area);
    const Detail::WidgetInput in = Detail::Interact(area, enabled, true);
    const float thumb = std::round(CapHeight() + 7);
    const float trackHeight = Max(4.0f, std::round(thumb * 0.3f));
    const BitMotor::Rect track(std::round(area.X + thumb * 0.5f), std::round(area.Center().Y - trackHeight * 0.5f),
                               std::round(area.Width - thumb), trackHeight);
    auto snap = [&](float v) {
        v = Clamp(v, min, max);
        return step > 0 ? Clamp(min + std::round((v - min) / step) * step, min, max) : v;
    };
    if (in.Pressed && in.Hovered) {
        DragId = id;
        Detail::FireClick();
    }
    if (enabled && DragId == id && Detail::UiMouseDown() && track.Width > 0)
        value = snap(min + Clamp((Detail::UiMouse().X - track.X) / track.Width, 0.0f, 1.0f) * (max - min));
    if (in.Adjust != 0) {
        value = snap(value + (step > 0 ? step : (max - min) * 0.05f) * float(in.Adjust));
        if (value != before) Detail::FireClick();
    }
    // The track: sunken, like a text field.
    const float r = trackHeight * 0.5f;
    FillRounded(track, r, r, Derive(Current.TextBoxBorder, -0.1f), Current.TextBoxBorder);
    FillRounded(track.Expand(-1), r - 1, r - 1, Derive(Current.InnerBackground, -0.09f), Current.InnerBackground);
    const float t = max > min ? (value - min) / (max - min) : 0.0f;
    const BitMotor::Rect knob(std::round(track.X + t * track.Width - thumb * 0.5f),
                              std::round(area.Center().Y - thumb * 0.5f), thumb, thumb);
    const bool dragging = DragId == id;
    ControlBody(knob, dragging ? Current.Pressed : in.Hovered ? Current.Hover : Current.Base, thumb * 0.5f);
    if (in.ShowFocus) FocusRing(knob, thumb * 0.5f);
    Detail::RememberWidget(area, in.Hovered, in.Focused);
    return value != before;
}

void ProgressBar(BitMotor::Rect area, float fraction) {
    const float r = Current.Radius;
    FillRounded(area, r, r, Derive(Current.TextBoxBorder, -0.1f), Current.TextBoxBorder);
    FillRounded(area.Expand(-1), r - 1, r - 1, Derive(Current.InnerBackground, -0.09f), Current.InnerBackground);
    const float t = Clamp(fraction, 0.0f, 1.0f);
    const BitMotor::Rect inside = area.Expand(-3);
    const float filled = std::round(inside.Width * t);
    if (filled > 0)
        FillRounded({inside.X, inside.Y, filled, inside.Height}, Max(0.0f, r - 1), Max(0.0f, r - 1),
                    Derive(Current.Accent, 0.05f), Derive(Current.Accent, -0.09f));
    Detail::RememberWidget(area, UI::Hovered(area), false);
}

bool TextField(BitMotor::Rect area, std::string_view label, std::string& text, const TextFieldOptions& options) {
    Fading fading(options.Enabled);
    const BitMotor::Rect box = label.empty() ? area
                                             : BitMotor::Rect(std::round(area.X + area.Width * 0.5f), area.Y,
                                                              area.Width - std::round(area.Width * 0.5f), area.Height);
    const bool editing = Detail::NextEditing();
    const bool focused = options.Enabled && (editing || (Detail::NextFocused() && Detail::ShowingFocus()));
    const float r = Current.Radius;
    FillRounded(box, r, r, focused ? Current.Focus : Current.TextBoxBorder,
                focused ? Current.Focus : Current.TextBoxBorder);
    FillRounded(box.Expand(-1), r - 1, r - 1, Derive(Current.InnerBackground, -0.04f), Current.InnerBackground);
    if (focused) OutlineRounded(box.Expand(2), r + 2, r + 2, Current.FaintFocus, 2);
    if (!label.empty()) DrawText(area, label, Current.TextColor, Align::Left, 0);
    // UI::TextField edits it and draws the text and the caret, in the palette's colours.
    const UI::Theme saved = UI::GetTheme();
    UI::Theme look = saved;
    look.Text = Style(Current.TextColor);
    look.TextDisabled = Faded(Current.PromptText);
    look.Accent = Current.TextColor; // the caret
    look.Padding = std::round(Current.Padding * 0.85f);
    UI::SetTheme(look);
    TextFieldOptions plain = options;
    plain.Frame = false;
    const bool changed = UI::TextField(box, {}, text, plain);
    UI::SetTheme(saved);
    return changed;
}

bool ComboBox(BitMotor::Rect area, int& selected, std::span<const std::string> options, bool enabled) {
    const int count = int(options.size());
    const int before = selected;
    if (count > 0) selected = Clamp(selected, 0, count - 1);
    enabled = enabled && count > 0;
    Fading fading(enabled);
    const std::uint64_t id = Hash("combo", area);
    const Detail::WidgetInput in = Detail::Interact(area, enabled, true);
    bool open = IsOpen(id);
    if (!open && in.Pressed) {
        OpenMenu(id, {area.X, area.Bottom() + 1}, area.Width, selected);
        open = true;
    } else if (!open && in.Adjust != 0) {
        selected = (selected + in.Adjust + count) % count;
        Detail::FireClick();
    }
    ControlBody(area, StateBase(Current.Base, in, open), Current.Radius);
    if (in.ShowFocus || open) FocusRing(area, Current.Radius);
    const float arrowWidth = std::round(CapHeight() * 0.9f + Current.Padding * 1.5f);
    if (count > 0)
        DrawText({area.X, area.Y, area.Width - arrowWidth, area.Height},
                 Fit(options[std::size_t(selected)], area.Width - arrowWidth - Current.Padding), Current.TextColor,
                 Align::Left, Current.Padding);
    Arrow({area.Right() - arrowWidth * 0.5f, area.Center().Y}, std::round(CapHeight() * 0.9f), Current.Mark);
    Detail::RememberWidget(area, in.Hovered, in.Focused);
    if (open) {
        BeginPopup();
        for (int i = 0; i < count; ++i)
            if (PopupRow(Entry::Item, options[std::size_t(i)], {}, true, false)) selected = i;
        EndPopup();
    }
    return selected != before;
}

bool ComboBox(BitMotor::Rect area, int& selected, std::initializer_list<std::string_view> options, bool enabled) {
    std::vector<std::string> items(options.begin(), options.end());
    return ComboBox(area, selected, std::span<const std::string>(items), enabled);
}

bool ListView(BitMotor::Rect area, int& selected, std::span<const std::string> items, int& scroll, bool enabled) {
    Fading fading(enabled);
    ReleaseDrag();
    const int before = selected;
    const int count = int(items.size());
    const std::uint64_t id = Hash("list", area);
    const Detail::WidgetInput in = Detail::Interact(area, enabled, true);
    const float row = RowHeight();
    const BitMotor::Rect inside = area.Expand(-1);
    const int visible = Max(1, int(inside.Height / row));
    const int most = Max(0, count - visible);
    const bool bar = count > visible;
    const float barWidth = std::round(CapHeight() + 5);
    const BitMotor::Rect rows(inside.X, inside.Y, inside.Width - (bar ? barWidth : 0), inside.Height);
    const BitMotor::Rect track(rows.Right(), inside.Y, bar ? barWidth : 0, inside.Height);
    const float thumbHeight = bar ? Max(barWidth, std::round(track.Height * float(visible) / float(count))) : 0;
    const Vec2 mouse = Detail::UiMouse();

    if (enabled) {
        if (UI::Hovered(area)) scroll -= int(std::round(Detail::FrameWheel() * 3));
        if (in.Pressed && in.Hovered) {
            if (bar && track.Contains(mouse)) {
                DragId = id;
                const float thumbY =
                    track.Y + (track.Height - thumbHeight) * (most > 0 ? float(scroll) / float(most) : 0);
                DragGrab = mouse.Y >= thumbY && mouse.Y < thumbY + thumbHeight ? mouse.Y - thumbY : thumbHeight * 0.5f;
            } else if (int at = scroll + int((mouse.Y - rows.Y) / row); at >= 0 && at < count) {
                selected = at;
            }
            ActiveList = id;
        }
        if (DragId == id && Detail::UiMouseDown() && track.Height > thumbHeight)
            scroll = int(std::round((mouse.Y - DragGrab - track.Y) / (track.Height - thumbHeight) * float(most)));
        if (in.ShowFocus) ActiveList = id;
        if (in.Adjust != 0 && count > 0) {
            selected = Clamp(selected + in.Adjust, 0, count - 1);
            if (selected < scroll) scroll = selected;
            if (selected >= scroll + visible) scroll = selected - visible + 1;
        }
    }
    scroll = Clamp(scroll, 0, most);

    FillRounded(area, 0, 0, Current.BoxBorder, Current.BoxBorder);
    FillRounded(inside, 0, 0, Current.InnerBackground, Current.InnerBackground);
    for (int i = 0; i < visible && scroll + i < count; ++i) {
        const int item = scroll + i;
        const BitMotor::Rect cell(rows.X, rows.Y + float(i) * row, rows.Width, row);
        Color text = Current.TextColor;
        if (item == selected) {
            const bool focused = enabled && ActiveList == id;
            const Color back = focused ? Current.Accent : Current.SelectionUnfocused;
            FillRounded(cell, 0, 0, back, back);
            text = focused ? Current.SelectedText : Current.TextColor;
        } else if (item % 2 == 1) {
            const Color alt = Derive(Current.InnerBackground, -0.02f);
            FillRounded(cell, 0, 0, alt, alt);
        }
        DrawText(cell, Fit(items[std::size_t(item)], cell.Width - Current.Padding * 2), text, Align::Left,
                 Current.Padding);
    }
    if (bar) {
        FillRounded(track, 0, 0, Derive(Current.Base, 0.1f), Derive(Current.Base, 0.1f));
        UI::Rect({track.X, track.Y, 1, track.Height}, Faded(Current.BoxBorder));
        const float y =
            track.Y + std::round((track.Height - thumbHeight) * (most > 0 ? float(scroll) / float(most) : 0));
        const BitMotor::Rect thumb(track.X + 2, y + 1, track.Width - 3, thumbHeight - 2);
        ControlBody(thumb, DragId == id ? Current.Pressed : Current.Base, thumb.Width * 0.5f);
    }
    if (in.ShowFocus) FocusRing(area, 0);
    Detail::RememberWidget(area, in.Hovered, in.Focused);
    if (selected != before) Detail::FireClick();
    return selected != before;
}

BitMotor::Rect Panel(BitMotor::Rect area, std::string_view title) {
    const float r = Current.Radius;
    float top = area.Y;
    if (!title.empty()) {
        // A title bar like a button's body, square at the bottom.
        const float height = std::round(CapHeight() + Current.Padding * 1.5f);
        Body({area.X, area.Y, area.Width, height}, Current.Base, r, 0);
        DrawText({area.X, area.Y, area.Width, height}, Fit(title, area.Width - Current.Padding * 2), Current.TextColor,
                 Align::Left, Current.Padding);
        top = area.Y + height - 1;
    }
    const BitMotor::Rect box(area.X, top, area.Width, area.Bottom() - top);
    const float boxRadius = title.empty() ? r : 0;
    FillRounded(box, boxRadius, r, Current.BoxBorder, Current.BoxBorder);
    FillRounded(box.Expand(-1), Max(0.0f, boxRadius - 1), r - 1, Current.Background, Current.Background);
    Detail::RememberWidget(area, UI::Hovered(area), false);
    return box.Expand(-Current.Padding);
}

BitMotor::Rect TabPane(BitMotor::Rect area, int& selected, std::span<const std::string> labels) {
    const int count = int(labels.size());
    if (count > 0) selected = Clamp(selected, 0, count - 1);
    const float height = std::round(CapHeight() + Current.Padding * 1.75f) + 4;
    // The strip behind the tabs and the content box.
    const BitMotor::Rect strip(area.X, area.Y, area.Width, height);
    FillRounded(strip, 0, 0, Current.TextBoxBorder, Derive(Current.TextBoxBorder, 0.3f));
    UI::Rect({strip.X, strip.Y, strip.Width, 1}, Faded(Current.OuterBorder));
    const BitMotor::Rect content(area.X, area.Y + height - 1, area.Width, area.Height - height + 1);
    FillRounded(content, 0, 0, Current.OuterBorder, Current.OuterBorder);
    FillRounded(content.Expand(-1), 0, 0, Current.Background, Current.Background);
    float x = area.X + 4;
    int changedTo = -1;
    for (int i = 0; i < count; ++i) {
        const std::string& label = labels[std::size_t(i)];
        const BitMotor::Rect tab(x, area.Y + 4, std::round(TextWidth(label) + Current.Padding * 2.5f), height - 4);
        x += tab.Width + 1;
        const Detail::WidgetInput in = Detail::Interact(tab, true);
        if (in.Pressed && i != selected) changedTo = i;
        Tab(tab, i == selected, in.Hovered);
        DrawText(tab, label, Current.TextColor, Align::Center, 0);
        if (in.ShowFocus) FocusRing(tab.Expand(-1), Current.Radius);
        Detail::RememberWidget(tab, in.Hovered, in.Focused);
    }
    if (changedTo >= 0) {
        selected = changedTo;
        Detail::FireClick();
    }
    return content.Expand(-1);
}

BitMotor::Rect TabPane(BitMotor::Rect area, int& selected, std::initializer_list<std::string_view> labels) {
    std::vector<std::string> items(labels.begin(), labels.end());
    return TabPane(area, selected, std::span<const std::string>(items));
}

bool SplitPane(BitMotor::Rect area, float& ratio, BitMotor::Rect& first, BitMotor::Rect& second,
               Orientation orientation) {
    ReleaseDrag();
    const float before = ratio;
    const bool across = orientation == Orientation::Horizontal;
    const float length = across ? area.Width : area.Height;
    const float bar = std::round(Max(4.0f, Current.Padding * 0.75f));
    const float room = Max(0.0f, length - bar);
    const std::uint64_t id = Hash("split", area, {}, int(orientation));
    ratio = Clamp(ratio, 0.0f, 1.0f);
    float at = std::round(room * ratio);
    BitMotor::Rect divider = across ? BitMotor::Rect(area.X + at, area.Y, bar, area.Height)
                                    : BitMotor::Rect(area.X, area.Y + at, area.Width, bar);
    // A little wider than it looks, easier to take.
    if (UI::Clicked(divider.Expand(2))) {
        DragId = id;
        DragGrab = (across ? Detail::UiMouse().X - divider.X : Detail::UiMouse().Y - divider.Y);
    }
    const bool dragging = DragId == id && Detail::UiMouseDown();
    if (dragging && room > 0) {
        const Vec2 mouse = Detail::UiMouse();
        const float position = (across ? mouse.X - area.X : mouse.Y - area.Y) - DragGrab;
        ratio = Clamp(position / room, 0.0f, 1.0f);
        at = std::round(room * ratio);
        divider = across ? BitMotor::Rect(area.X + at, area.Y, bar, area.Height)
                         : BitMotor::Rect(area.X, area.Y + at, area.Width, bar);
    }
    first = across ? BitMotor::Rect(area.X, area.Y, at, area.Height) : BitMotor::Rect(area.X, area.Y, area.Width, at);
    second = across ? BitMotor::Rect(divider.Right(), area.Y, area.Right() - divider.Right(), area.Height)
                    : BitMotor::Rect(area.X, divider.Bottom(), area.Width, area.Bottom() - divider.Bottom());
    const bool lit = dragging || UI::Hovered(divider.Expand(2));
    UI::Rect(divider, Faded(lit ? Derive(Current.Background, -0.08f) : Current.Background));
    const Color line = Faded(Current.BoxBorder);
    if (across) {
        UI::Rect({divider.X, divider.Y, 1, divider.Height}, line);
        UI::Rect({divider.Right() - 1, divider.Y, 1, divider.Height}, line);
    } else {
        UI::Rect({divider.X, divider.Y, divider.Width, 1}, line);
        UI::Rect({divider.X, divider.Bottom() - 1, divider.Width, 1}, line);
    }
    return ratio != before;
}

BitMotor::Rect BeginDialog(BitMotor::Rect area, std::string_view title) {
    UI::Rect({0, 0, Screen::Width(), Screen::Height()}, Current.ModalShade);
    Shadow(area, 0, Current.Shadow, Current.ShadowSize * 1.5f);
    FillRounded(area, 0, 0, Current.OuterBorder, Current.OuterBorder);
    FillRounded(area.Expand(-1), 0, 0, Current.Background, Current.Background);
    float top = area.Y + 1;
    if (!title.empty()) {
        const float height = std::round(CapHeight() + Current.Padding * 2.5f);
        const BitMotor::Rect header(area.X + 1, area.Y + 1, area.Width - 2, height);
        FillRounded(header, 0, 0, Current.Background, Derive(Current.Background, 0.3f));
        UI::Rect({header.X, header.Bottom() - 1, header.Width, 1}, Current.BoxBorder);
        DrawText(header, title, Current.TextColor, Align::Left, Current.Padding * 1.5f);
        top = header.Bottom();
    }
    UI::BeginModal();
    return BitMotor::Rect(area.X + 1, top, area.Width - 2, area.Bottom() - 1 - top).Expand(-Current.Padding);
}

void EndDialog() { UI::EndModal(); }

// ---------------------------------------------------------------- for editors

namespace {

// Where a drag started (Spinner, Knob): the mouse's y and the value then.
float DragValue = 0;

// A small triangle pointing up, centred on `center`.
void ArrowUp(Vec2 center, float width, Color color) {
    const float half = std::round(width * 0.5f), height = std::round(width * 0.5f);
    const Vec2 a(std::round(center.X) - half, std::round(center.Y + height * 0.5f));
    UI::Triangle(a, {a.X + half, a.Y - height}, {a.X + 2 * half, a.Y}, Faded(color));
}

// Turns of the wheel this frame over the area (0 when it is not hovered or the UI does not take the mouse).
float WheelOver(BitMotor::Rect area) { return UI::Hovered(area) ? Detail::FrameWheel() : 0.0f; }

bool CtrlHeld() { return Detail::FrameDown(Key::LeftCtrl) || Detail::FrameDown(Key::RightCtrl); }
bool ShiftHeld() { return Detail::FrameDown(Key::LeftShift) || Detail::FrameDown(Key::RightShift); }

template <class T> bool SpinnerOf(BitMotor::Rect area, T& value, T min, T max, T step, int decimals, bool enabled) {
    Fading fading(enabled);
    ReleaseDrag();
    if (max < min) std::swap(min, max);
    const T before = value;
    value = Clamp(value, min, max);
    const std::uint64_t id = Hash("spinner", area);
    const Detail::WidgetInput in = Detail::Interact(area, enabled, true);
    const float arrows = std::round(CapHeight() + Current.Padding);
    const BitMotor::Rect buttons(area.Right() - arrows, area.Y, arrows, area.Height);
    const BitMotor::Rect up(buttons.X, buttons.Y, buttons.Width, std::floor(buttons.Height * 0.5f));
    const BitMotor::Rect down(buttons.X, up.Bottom(), buttons.Width, buttons.Height - up.Height);
    const Vec2 mouse = Detail::UiMouse();
    auto change = [&](double steps) {
        value = Clamp(T(double(value) + double(step) * steps), min, max);
        if constexpr (std::is_floating_point_v<T>)
            if (step > 0) value = Clamp(T(min + std::round((value - min) / step) * step), min, max);
    };
    if (enabled) {
        if (in.Pressed && in.Hovered) {
            if (up.Contains(mouse)) {
                change(1);
            } else if (down.Contains(mouse)) {
                change(-1);
            } else {
                DragId = id;
                DragGrab = mouse.Y;
                DragValue = float(value);
            }
        }
        if (DragId == id && Detail::UiMouseDown()) {
            value = T(DragValue);
            change(std::trunc((DragGrab - mouse.Y) / 4)); // a step every 4 pixels
        }
        if (const float wheel = WheelOver(area); wheel != 0) change(std::round(wheel));
        if (in.Adjust != 0) change(in.Adjust);
    }
    // A text field with the arrows on its right.
    FillRounded(area, Current.Radius, Current.Radius, Derive(Current.TextBoxBorder, -0.1f), Current.TextBoxBorder);
    FillRounded(area.Expand(-1), Current.Radius - 1, Current.Radius - 1, Derive(Current.InnerBackground, -0.09f),
                Current.InnerBackground);
    std::string text;
    if constexpr (std::is_floating_point_v<T>)
        text = std::format("{:.{}f}", double(value), Max(0, decimals));
    else
        text = std::to_string(value);
    DrawText({area.X, area.Y, area.Width - arrows, area.Height}, text, Current.TextColor, Align::Right,
             Current.Padding);
    const bool overUp = in.Hovered && up.Contains(mouse), overDown = in.Hovered && down.Contains(mouse);
    Body(up.Expand(-1), overUp ? (in.Down ? Current.Pressed : Current.Hover) : Current.Base, Current.Radius, 0);
    Body(down.Expand(-1), overDown ? (in.Down ? Current.Pressed : Current.Hover) : Current.Base, 0, Current.Radius);
    const float mark = std::round(CapHeight() * 0.6f);
    ArrowUp(up.Center(), mark, Current.Mark);
    Arrow(down.Center(), mark, Current.Mark);
    if (in.ShowFocus) FocusRing(area, Current.Radius);
    Detail::RememberWidget(area, in.Hovered, in.Focused);
    if (value != before) Detail::FireClick();
    return value != before;
}

} // namespace

bool Spinner(BitMotor::Rect area, int& value, int min, int max, int step, bool enabled) {
    return SpinnerOf(area, value, min, max, Max(1, step), 0, enabled);
}

bool Spinner(BitMotor::Rect area, float& value, float min, float max, float step, int decimals, bool enabled) {
    return SpinnerOf(area, value, min, max, step > 0 ? step : (max - min) * 0.01f, decimals, enabled);
}

bool Knob(BitMotor::Rect area, float& value, float min, float max, float step, bool enabled) {
    return Knob(area, value, min, max, step, min, enabled);
}

bool Knob(BitMotor::Rect area, float& value, float min, float max, float step, float origin, bool enabled) {
    Fading fading(enabled);
    ReleaseDrag();
    if (max < min) std::swap(min, max);
    const float before = value;
    value = Clamp(value, min, max);
    const std::uint64_t id = Hash("knob", area);
    const Detail::WidgetInput in = Detail::Interact(area, enabled, true);
    const Vec2 mouse = Detail::UiMouse();
    auto snap = [&](float v) {
        v = Clamp(v, min, max);
        return step > 0 ? Clamp(min + std::round((v - min) / step) * step, min, max) : v;
    };
    if (enabled) {
        if (in.Pressed && in.Hovered) {
            DragId = id;
            DragGrab = mouse.Y;
            DragValue = value;
            Detail::FireClick();
        }
        // 150 pixels up or down for the whole range.
        if (DragId == id && Detail::UiMouseDown()) value = snap(DragValue + (DragGrab - mouse.Y) / 150 * (max - min));
        const float unit = step > 0 ? step : (max - min) * 0.01f;
        if (const float wheel = WheelOver(area); wheel != 0) value = snap(value + unit * std::round(wheel));
        if (in.Adjust != 0) value = snap(value + unit * float(in.Adjust));
    }
    const float size = std::floor(Min(area.Width, area.Height));
    const Vec2 center(std::round(area.Center().X), std::round(area.Center().Y));
    const float ring = size * 0.5f - 1, body = std::round(size * 0.34f);
    // From the bottom left (t = 0) round the top to the bottom right (t = 1), clockwise on the screen.
    auto at = [&](float t, float radius) {
        const float angle = 3.14159265f * (0.75f + 1.5f * t);
        return center + Vec2(std::cos(angle), std::sin(angle)) * radius;
    };
    auto arc = [&](float from, float to, Color color) {
        if (to < from) std::swap(from, to);
        const int pieces = Max(1, int(std::ceil((to - from) * 24)));
        for (int i = 0; i < pieces; ++i)
            UI::Line(at(from + (to - from) * float(i) / float(pieces), ring - 1),
                     at(from + (to - from) * float(i + 1) / float(pieces), ring - 1), Faded(color), 2);
    };
    const float range = max > min ? max - min : 1.0f;
    const float t = (value - min) / range, t0 = Clamp((origin - min) / range, 0.0f, 1.0f);
    arc(0, 1, Current.BoxBorder);
    arc(t0, t, Current.Accent);
    const BitMotor::Rect knob(center.X - body, center.Y - body, body * 2, body * 2);
    ControlBody(knob, DragId == id ? Current.Pressed : in.Hovered ? Current.Hover : Current.Base, body);
    UI::Line(at(t, body * 0.25f), at(t, body - 2), Faded(Current.Mark), 2);
    if (in.ShowFocus) FocusRing(knob, body);
    Detail::RememberWidget(area, in.Hovered, in.Focused);
    if (value != before) Detail::FireClick();
    return value != before;
}

namespace {

struct ScrollState {
    BitMotor::Rect Area, View, VTrack, HTrack;
    Vec2 Content;
    Vec2* Scroll = nullptr;
    std::uint64_t VId = 0, HId = 0;
};
std::vector<ScrollState> Scrolls;

float BarSize() { return std::round(CapHeight() + 5); }

// The thumb of a scroll bar along `track` (vertical when it is taller than wide).
BitMotor::Rect Thumb(BitMotor::Rect track, float view, float content, float scroll) {
    const bool vertical = track.Height > track.Width;
    const float length = vertical ? track.Height : track.Width;
    const float thumb = Max(BarSize(), std::round(length * Min(1.0f, view / Max(1.0f, content))));
    const float most = Max(0.0f, content - view);
    const float at = std::round((length - thumb) * (most > 0 ? scroll / most : 0));
    return vertical ? BitMotor::Rect(track.X, track.Y + at, track.Width, thumb)
                    : BitMotor::Rect(track.X + at, track.Y, thumb, track.Height);
}

} // namespace

Vec2 BeginScrollArea(BitMotor::Rect area, Vec2 content, Vec2& scroll, bool enabled) {
    ReleaseDrag();
    ScrollState state;
    state.Area = area;
    state.Content = content;
    state.Scroll = &scroll;
    state.VId = Hash("scroll-v", area);
    state.HId = Hash("scroll-h", area);
    const float bar = BarSize();
    const BitMotor::Rect inside = area.Expand(-1);
    bool vertical = content.Y > inside.Height;
    const bool horizontal = content.X > inside.Width - (vertical ? bar : 0);
    if (horizontal && !vertical) vertical = content.Y > inside.Height - bar;
    state.View = {inside.X, inside.Y, inside.Width - (vertical ? bar : 0), inside.Height - (horizontal ? bar : 0)};
    state.VTrack = vertical ? BitMotor::Rect(state.View.Right(), inside.Y, bar, state.View.Height) : BitMotor::Rect();
    state.HTrack = horizontal ? BitMotor::Rect(inside.X, state.View.Bottom(), state.View.Width, bar) : BitMotor::Rect();
    const Vec2 most(Max(0.0f, content.X - state.View.Width), Max(0.0f, content.Y - state.View.Height));
    const Vec2 mouse = Detail::UiMouse();

    if (enabled) {
        if (const float wheel = CtrlHeld() ? 0.0f : WheelOver(area); wheel != 0) {
            const float distance = wheel * RowHeight() * 3;
            if (ShiftHeld() || (!vertical && horizontal))
                scroll.X -= distance;
            else
                scroll.Y -= distance;
        }
        auto grab = [&](const BitMotor::Rect& track, std::uint64_t id, bool isVertical) {
            if (track.Width <= 0 || !UI::Clicked(track)) return;
            DragId = id;
            const BitMotor::Rect thumb = isVertical ? Thumb(track, state.View.Height, content.Y, scroll.Y)
                                                    : Thumb(track, state.View.Width, content.X, scroll.X);
            const float m = isVertical ? mouse.Y : mouse.X, start = isVertical ? thumb.Y : thumb.X;
            const float length = isVertical ? thumb.Height : thumb.Width;
            DragGrab = m >= start && m < start + length ? m - start : length * 0.5f;
        };
        grab(state.VTrack, state.VId, true);
        grab(state.HTrack, state.HId, false);
        if (Detail::UiMouseDown()) {
            if (DragId == state.VId && vertical) {
                const float thumb = Thumb(state.VTrack, state.View.Height, content.Y, scroll.Y).Height;
                if (state.VTrack.Height > thumb)
                    scroll.Y = (mouse.Y - DragGrab - state.VTrack.Y) / (state.VTrack.Height - thumb) * most.Y;
            }
            if (DragId == state.HId && horizontal) {
                const float thumb = Thumb(state.HTrack, state.View.Width, content.X, scroll.X).Width;
                if (state.HTrack.Width > thumb)
                    scroll.X = (mouse.X - DragGrab - state.HTrack.X) / (state.HTrack.Width - thumb) * most.X;
            }
        }
    }
    scroll = {std::round(Clamp(scroll.X, 0.0f, most.X)), std::round(Clamp(scroll.Y, 0.0f, most.Y))};

    FillRounded(area, 0, 0, Current.BoxBorder, Current.BoxBorder);
    FillRounded(inside, 0, 0, Current.InnerBackground, Current.InnerBackground);
    Scrolls.push_back(state);
    Screen::PushClip(state.View);
    return state.View.Position() - scroll;
}

void EndScrollArea() {
    if (Scrolls.empty()) {
        Log::Warn("Fx::EndScrollArea without a BeginScrollArea");
        return;
    }
    Screen::PopClip();
    const ScrollState state = Scrolls.back();
    Scrolls.pop_back();
    const Vec2 scroll = *state.Scroll;
    const Color track = Derive(Current.Base, 0.1f);
    if (state.VTrack.Width > 0) {
        FillRounded(state.VTrack, 0, 0, track, track);
        UI::Rect({state.VTrack.X, state.VTrack.Y, 1, state.VTrack.Height}, Faded(Current.BoxBorder));
        const BitMotor::Rect thumb = Thumb(state.VTrack, state.View.Height, state.Content.Y, scroll.Y);
        const BitMotor::Rect body(thumb.X + 2, thumb.Y + 1, thumb.Width - 3, thumb.Height - 2);
        ControlBody(body, DragId == state.VId ? Current.Pressed : Current.Base, body.Width * 0.5f);
    }
    if (state.HTrack.Width > 0) {
        FillRounded(state.HTrack, 0, 0, track, track);
        UI::Rect({state.HTrack.X, state.HTrack.Y, state.HTrack.Width, 1}, Faded(Current.BoxBorder));
        const BitMotor::Rect thumb = Thumb(state.HTrack, state.View.Width, state.Content.X, scroll.X);
        const BitMotor::Rect body(thumb.X + 1, thumb.Y + 2, thumb.Width - 2, thumb.Height - 3);
        ControlBody(body, DragId == state.HId ? Current.Pressed : Current.Base, body.Height * 0.5f);
    }
    if (state.VTrack.Width > 0 && state.HTrack.Width > 0)
        FillRounded({state.VTrack.X, state.HTrack.Y, state.VTrack.Width, state.HTrack.Height}, 0, 0, track, track);
}

BitMotor::Rect ScrollView() { return Scrolls.empty() ? BitMotor::Rect() : Scrolls.back().View; }

namespace {

struct GridState {
    Vec2 Origin;
    Vec2 Cell;
};
std::vector<GridState> Grids;
// The grid the left button went down on (0: none), and the cell.
std::uint64_t GridDrag = 0;
GridPoint GridFrom;

} // namespace

GridInput BeginGrid(BitMotor::Rect area, Grid& grid, bool enabled) {
    grid.Columns = Max(0, grid.Columns);
    grid.Rows = Max(0, grid.Rows);
    const Vec2 cell(Max(1.0f, grid.Cell.X), Max(1.0f, grid.Cell.Y));
    const Vec2 content(float(grid.Columns) * cell.X, float(grid.Rows) * cell.Y);
    const Vec2 origin = BeginScrollArea(area, content, grid.Scroll, enabled);
    const BitMotor::Rect view = ScrollView();
    const std::uint64_t id = Hash("grid", area);
    Grids.push_back({origin, cell});

    // The visible columns and rows.
    const int firstColumn = Max(0, int((view.X - origin.X) / cell.X));
    const int lastColumn = Min(grid.Columns, int(std::ceil((view.Right() - origin.X) / cell.X)));
    const int firstRow = Max(0, int((view.Y - origin.Y) / cell.Y));
    const int lastRow = Min(grid.Rows, int(std::ceil((view.Bottom() - origin.Y) / cell.Y)));
    const float width = Min(view.Width, content.X - (view.X - origin.X));
    const float height = Min(view.Height, content.Y - (view.Y - origin.Y));
    if (grid.BandColumns > 0) {
        const Color band = Derive(Current.InnerBackground, -0.04f);
        for (int c = firstColumn - firstColumn % grid.BandColumns; c < lastColumn; c += grid.BandColumns)
            if ((c / grid.BandColumns) % 2 == 1)
                UI::Rect({std::round(origin.X + float(c) * cell.X), view.Y, float(grid.BandColumns) * cell.X, height},
                         Faded(band));
    }
    const Color minor = Derive(Current.InnerBackground, -0.08f), major = Derive(Current.BoxBorder, -0.12f);
    for (int c = Max(1, firstColumn); c <= lastColumn && c < grid.Columns; ++c) {
        const bool strong = grid.MajorColumns > 0 && c % grid.MajorColumns == 0;
        UI::Rect({std::round(origin.X + float(c) * cell.X), view.Y, 1, height}, Faded(strong ? major : minor));
    }
    for (int r = Max(1, firstRow); r <= lastRow && r < grid.Rows; ++r) {
        const bool strong = grid.MajorRows > 0 && r % grid.MajorRows == 0;
        UI::Rect({view.X, std::round(origin.Y + float(r) * cell.Y), width, 1}, Faded(strong ? major : minor));
    }

    GridInput in;
    const Vec2 mouse = Detail::UiMouse();
    auto cellAt = [&](Vec2 p, bool clamp) {
        GridPoint point{int(std::floor((p.X - origin.X) / cell.X)), int(std::floor((p.Y - origin.Y) / cell.Y))};
        if (clamp) {
            point.Column = Clamp(point.Column, 0, grid.Columns - 1);
            point.Row = Clamp(point.Row, 0, grid.Rows - 1);
        } else if (point.Column < 0 || point.Row < 0 || point.Column >= grid.Columns || point.Row >= grid.Rows) {
            return GridPoint{};
        }
        return point;
    };
    if (!enabled || grid.Columns == 0 || grid.Rows == 0) return in;
    const bool over = UI::Hovered(view);
    if (over) in.Hovered = cellAt(mouse, false);
    if (over && in.Hovered.Valid() && UI::Clicked(view)) {
        in.Pressed = in.Hovered;
        GridDrag = id;
        GridFrom = in.Hovered;
    }
    if (over && in.Hovered.Valid() && UI::RightClicked(view)) in.RightPressed = in.Hovered;
    if (GridDrag == id) {
        in.Hovered = cellAt(mouse, true); // dragging out of the view keeps to the edge
        in.DragFrom = GridFrom;
        if (Detail::UiMouseDown()) {
            in.Dragging = true;
        } else {
            in.Released = true;
            GridDrag = 0;
        }
    }
    return in;
}

BitMotor::Rect GridCell(int column, int row) {
    if (Grids.empty()) return {};
    const GridState& g = Grids.back();
    return {std::round(g.Origin.X + float(column) * g.Cell.X), std::round(g.Origin.Y + float(row) * g.Cell.Y),
            std::round(g.Cell.X), std::round(g.Cell.Y)};
}

void GridSelection(GridPoint a, GridPoint b) {
    if (Grids.empty() || !a.Valid() || !b.Valid()) return;
    const BitMotor::Rect first = GridCell(Min(a.Column, b.Column), Min(a.Row, b.Row));
    const BitMotor::Rect last = GridCell(Max(a.Column, b.Column), Max(a.Row, b.Row));
    const BitMotor::Rect box(first.X, first.Y, last.Right() - first.X, last.Bottom() - first.Y);
    UI::Rect(box, Faded(Current.Accent.WithAlpha(0.22f)));
    RoundedOutline(box, 0, Current.Accent, 1);
}

void EndGrid() {
    if (Grids.empty()) {
        Log::Warn("Fx::EndGrid without a BeginGrid");
        return;
    }
    Grids.pop_back();
    EndScrollArea();
}

namespace {

struct DragState {
    std::string Payload, Label;
    std::uint64_t Source = 0;
    Vec2 From;
    bool Pending = false, Active = false;
    bool Dropped = false; // let go this frame: the targets can take it
    std::uint64_t Seen = 0;
} Drag;

void ResetDrag(std::uint64_t seen) {
    Drag = DragState{};
    Drag.Seen = seen;
}

// Once a frame: starts the drag after 4 pixels, lets it go when the button is up, and draws its label.
void UpdateDrag() {
    const std::uint64_t frame = Time::Frame();
    if (Drag.Seen == frame) return;
    Drag.Seen = frame;
    if (Drag.Dropped) ResetDrag(frame); // nobody took it last frame
    if (!Drag.Pending && !Drag.Active) return;
    if (!Detail::UiMouseDown()) {
        if (Drag.Active)
            Drag.Dropped = true;
        else
            ResetDrag(frame);
        return;
    }
    if (!Drag.Active && LengthSquared(Detail::UiMouse() - Drag.From) >= 16) Drag.Active = true;
    if (!Drag.Active || Drag.Label.empty()) return;
    Detail::DrawLast([p = Current, label = Drag.Label, at = Detail::UiMouse()] {
        const Palette saved = Current;
        Current = p;
        const float width = std::round(TextWidth(label) + p.Padding * 2), height = RowHeight();
        const BitMotor::Rect box(std::round(at.X + 12), std::round(at.Y + 8), width, height);
        Shadow(box, p.Radius, p.Shadow, p.ShadowSize * 0.5f);
        FillRounded(box, p.Radius, p.Radius, p.Accent, p.Accent);
        DrawText(box, label, p.SelectedText, Align::Center, 0);
        Current = saved;
    });
}

} // namespace

bool DragSource(BitMotor::Rect area, std::string_view payload, std::string_view label) {
    UpdateDrag();
    const std::uint64_t id = Hash("drag", area, payload);
    if (!Drag.Pending && !Drag.Active && Detail::FramePressed(MouseButton::Left) && UI::Hovered(area)) {
        Drag.Pending = true;
        Drag.Source = id;
        Drag.Payload = std::string(payload);
        Drag.Label = std::string(label);
        Drag.From = Detail::UiMouse();
    }
    return Drag.Active && !Drag.Dropped && Drag.Source == id;
}

bool DropTarget(BitMotor::Rect area, std::string& payload) {
    UpdateDrag();
    if (!Drag.Active) return false;
    const Vec2 mouse = Detail::UiMouse();
    const std::optional<BitMotor::Rect> clip = Screen::Clip();
    const bool over = area.Contains(mouse) && (!clip || clip->Contains(mouse)) && !Detail::PopupOpen();
    if (!over) return false;
    if (Drag.Dropped) {
        payload = Drag.Payload;
        ResetDrag(Drag.Seen);
        return true;
    }
    RoundedOutline(area, Current.Radius, Current.Accent, 2);
    return false;
}

std::string_view Dragged() {
    UpdateDrag();
    return Drag.Active && !Drag.Dropped ? std::string_view(Drag.Payload) : std::string_view();
}

// ---------------------------------------------------------------- menus

void BeginMenuBar(BitMotor::Rect area) {
    FillRounded(area, 0, 0, Derive(Current.Base, 0.75f), Current.Base);
    UI::Rect({area.X, area.Bottom() - 1, area.Width, 1}, Faded(Current.OuterBorder));
    Menus.InBar = true;
    Menus.Bar = area;
    Menus.BarX = area.X + std::round(Current.Padding * 0.5f);
    Menus.BarIndex = 0;
}

void EndMenuBar() {
    Menus.BarCount = Menus.BarIndex;
    Menus.InBar = false;
}

bool Menu(std::string_view label, bool enabled) {
    Fading fading(enabled);
    const int ordinal = Menus.BarIndex++;
    const BitMotor::Rect header(Menus.BarX, Menus.Bar.Y, std::round(TextWidth(label) + Current.Padding * 2),
                                Menus.Bar.Height - 1);
    Menus.BarX += header.Width;
    const std::uint64_t id = Hash("menu", Menus.Bar, label, ordinal);
    bool open = IsOpen(id);
    const bool barMenuOpen = Menus.Open && Menus.OpenOrdinal >= 0;
    const Detail::WidgetInput in = Detail::Interact(header, enabled);
    const Vec2 mouse = Detail::UiMouse();
    const Vec2 below(header.X, header.Bottom() + 1);
    if (enabled && Menus.SwitchTo == ordinal) {
        Menus.SwitchTo = -1;
        if (!open) OpenMenu(id, below, 0, 0, ordinal);
    } else if (enabled && barMenuOpen && Detail::Popup().Click && header.Contains(mouse)) {
        Detail::UsePopupClick();
        if (open)
            CloseMenu();
        else
            OpenMenu(id, below, 0, -1, ordinal);
    } else if (enabled && barMenuOpen && !open && Detail::UiMouseMoved() && header.Contains(mouse)) {
        OpenMenu(id, below, 0, -1, ordinal); // pointing at another title while a menu is open
    } else if (in.Pressed && !open) {
        OpenMenu(id, below, 0, in.Hovered ? -1 : 0, ordinal);
    }
    open = IsOpen(id);
    const bool lit = open || (!barMenuOpen && (in.Hovered || in.ShowFocus));
    if (lit) FillRounded(header, 0, 0, Current.Accent, Current.Accent);
    DrawText(header, label, lit ? Current.SelectedText : Current.TextColor, Align::Center, 0);
    Detail::RememberWidget(header, in.Hovered, in.Focused);
    if (open) BeginPopup();
    return open;
}

bool BeginContextMenu(BitMotor::Rect area) {
    const std::uint64_t id = Hash("context", area);
    bool open = IsOpen(id);
    const Vec2 mouse = Detail::UiMouse();
    if (open) {
        if (Detail::Popup().RightClick && area.Contains(mouse)) {
            Detail::UsePopupClick();
            Menus.At = mouse; // right-clicked again: the menu moves there
            Menus.Hot = -1;
            CloseSub();
        }
    } else if (UI::RightClicked(area)) {
        OpenMenu(id, mouse, 0, -1);
        open = true;
    }
    if (open) BeginPopup();
    return open;
}

bool MenuItem(std::string_view label, std::string_view shortcut, bool enabled) {
    return PopupRow(Entry::Item, label, shortcut, enabled, false);
}

bool CheckMenuItem(std::string_view label, bool& checked, std::string_view shortcut, bool enabled) {
    const bool chosen = PopupRow(Entry::CheckItem, label, shortcut, enabled, checked);
    if (chosen) {
        checked = !checked;
        Level->Entries.back().Checked = checked;
    }
    return chosen;
}

void MenuSeparator() {
    const float height = std::round(Max(5.0f, Current.Padding * 0.75f));
    Level->Entries.push_back({Entry::Line, {}, {}, false, false, false, Level->NextY, height, -1});
    Level->NextY += height;
}

bool BeginSubMenu(std::string_view label, bool enabled) {
    if (Level != &Menus || !Menus.Open) return false; // one level of submenus
    const float row = RowHeight();
    const int item = Menus.Items++;
    const float y = Menus.NextY;
    const BitMotor::Rect area(Menus.At.X, Menus.At.Y + y, Menus.Width, row);
    const Vec2 mouse = Detail::UiMouse();
    const Detail::PopupInput& in = Detail::Popup();
    if (Detail::UiMouseMoved() && area.Contains(mouse)) {
        Menus.Hot = enabled ? item : -1;
        if (enabled && Menus.SubOwner != item) {
            Menus.SubOwner = item;
            Menus.SubFocus = false;
            Menus.Sub.Hot = -1;
        }
    }
    if (in.Click && area.Contains(mouse)) {
        Detail::UsePopupClick();
        if (enabled) Menus.SubOwner = item;
    }
    if (enabled && !Menus.SubFocus && Menus.Hot == item && (in.Nav[NavRight] || in.Nav[NavConfirm])) {
        Menus.SubOwner = item;
        Menus.SubFocus = true;
        Menus.Sub.Hot = 0;
    }
    Menus.LabelWidth = Max(Menus.LabelWidth, TextWidth(label));
    Menus.Entries.push_back({Entry::SubItem,
                             std::string(label),
                             {},
                             enabled,
                             false,
                             Menus.Hot == item || Menus.SubOwner == item,
                             y,
                             row,
                             item});
    Menus.NextY += row;
    if (Menus.SubOwner != item) return false;

    PopupLevel& sub = Menus.Sub;
    StartLevel(sub);
    sub.At = {std::round(Menus.At.X + Menus.Width - 1), std::round(Menus.At.Y + y - PopupPad())};
    if (Menus.SubFocus) MoveHot(sub, in);
    Menus.SubBuilt = true;
    Level = &sub;
    return true;
}

void EndMenu() {
    if (Level == &Menus.Sub)
        EndSubPopup();
    else
        EndPopup();
}

} // namespace BitMotor::UI::JFxStyle
