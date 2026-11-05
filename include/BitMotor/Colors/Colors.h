#pragma once
#include "BitMotor/Colors/Color.h"

// 40 ready-made colours. Most follow the usual web values; Phosphor (old green monitor) and Ice are ours.
//     Screen::Clear(Colors::Navy);
//     Color warning = Colors::Amber;
namespace BitMotor::Colors {

// Greys
inline constexpr Color White = Color::Hex(0xFFFFFF);
inline constexpr Color Black = Color::Hex(0x000000);
inline constexpr Color Gray = Color::Hex(0x808080);
inline constexpr Color LightGray = Color::Hex(0xC0C0C0);
inline constexpr Color DarkGray = Color::Hex(0x404040);
inline constexpr Color Slate = Color::Hex(0x708090);

// Reds and oranges
inline constexpr Color Red = Color::Hex(0xFF0000);
inline constexpr Color DarkRed = Color::Hex(0x8B0000);
inline constexpr Color Crimson = Color::Hex(0xDC143C);
inline constexpr Color Maroon = Color::Hex(0x800000);
inline constexpr Color Coral = Color::Hex(0xFF7F50);
inline constexpr Color Salmon = Color::Hex(0xFA8072);
inline constexpr Color Orange = Color::Hex(0xFFA500);
inline constexpr Color Amber = Color::Hex(0xFFBF00);

// Yellows and greens
inline constexpr Color Gold = Color::Hex(0xFFD700);
inline constexpr Color Yellow = Color::Hex(0xFFFF00);
inline constexpr Color Lime = Color::Hex(0x00FF00);
inline constexpr Color Green = Color::Hex(0x008000);
inline constexpr Color DarkGreen = Color::Hex(0x006400);
inline constexpr Color Olive = Color::Hex(0x808000);
inline constexpr Color Mint = Color::Hex(0x98FF98);
inline constexpr Color Phosphor = Color::Hex(0x33FF66);

// Cyans and blues
inline constexpr Color Teal = Color::Hex(0x008080);
inline constexpr Color Turquoise = Color::Hex(0x40E0D0);
inline constexpr Color Cyan = Color::Hex(0x00FFFF);
inline constexpr Color Ice = Color::Hex(0xC8F0FF);
inline constexpr Color SkyBlue = Color::Hex(0x87CEEB);
inline constexpr Color Blue = Color::Hex(0x0000FF);
inline constexpr Color Navy = Color::Hex(0x000080);

// Purples and pinks
inline constexpr Color Indigo = Color::Hex(0x4B0082);
inline constexpr Color Purple = Color::Hex(0x800080);
inline constexpr Color Violet = Color::Hex(0xEE82EE);
inline constexpr Color Lavender = Color::Hex(0xE6E6FA);
inline constexpr Color Magenta = Color::Hex(0xFF00FF);
inline constexpr Color Pink = Color::Hex(0xFFC0CB);
inline constexpr Color Rose = Color::Hex(0xFF007F);

// Browns
inline constexpr Color Brown = Color::Hex(0xA52A2A);
inline constexpr Color Tan = Color::Hex(0xD2B48C);
inline constexpr Color Beige = Color::Hex(0xF5F5DC);

// Fully transparent black.
inline constexpr Color Transparent = Color::Hex(0x000000, 0.0f);

} // namespace BitMotor::Colors
