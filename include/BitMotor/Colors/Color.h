#pragma once
#include <cstdint>
#include <string>
#include <string_view>

// RGBA colour with floats in [0, 1] (what the GPU expects). Alpha is straight, not premultiplied.
//     Color hurt = Color::Hex(0xE53935);
//     Color ghost = Colors::White.WithAlpha(0.5f);
//     Color sky = Colors::SkyBlue.Lerp(Colors::Navy, nightAmount);
namespace BitMotor {

struct Color {
    float R = 0, G = 0, B = 0, A = 1;

    constexpr Color() = default;
    constexpr Color(float r, float g, float b, float a = 1.0f) : R(r), G(g), B(b), A(a) {}

    // 0xRRGGBB, as in image editors: Color::Hex(0xFF8800).
    static constexpr Color Hex(std::uint32_t rgb, float alpha = 1.0f) {
        return {float((rgb >> 16) & 0xFF) / 255.0f, float((rgb >> 8) & 0xFF) / 255.0f, float(rgb & 0xFF) / 255.0f,
                alpha};
    }
    // Text form, for data files: "#RRGGBB" or "#RRGGBBAA" ('#' optional, upper or lower case).
    // Invalid text logs a warning and returns magenta, so the mistake is visible on screen.
    static Color Hex(std::string_view text);
    // Bytes in 0..255.
    static constexpr Color Bytes(int r, int g, int b, int a = 255) {
        return {float(r) / 255.0f, float(g) / 255.0f, float(b) / 255.0f, float(a) / 255.0f};
    }

    constexpr Color WithAlpha(float alpha) const { return {R, G, B, alpha}; }
    // Mixes towards white (Lighter) or black (Darker) by `amount` in [0, 1]. Alpha does not change.
    constexpr Color Lighter(float amount = 0.25f) const {
        return {R + (1 - R) * amount, G + (1 - G) * amount, B + (1 - B) * amount, A};
    }
    constexpr Color Darker(float amount = 0.25f) const {
        return {R * (1 - amount), G * (1 - amount), B * (1 - amount), A};
    }
    // This colour when t = 0, `other` when t = 1 (all four channels).
    constexpr Color Lerp(Color other, float t) const {
        return {R + (other.R - R) * t, G + (other.G - G) * t, B + (other.B - B) * t, A + (other.A - A) * t};
    }

    // "#RRGGBBAA" with each channel clamped to [0, 1] and rounded.
    std::string ToHex() const;

    constexpr bool operator==(const Color&) const = default;
};

// Free form, so Tween<Color> works like any other value.
constexpr Color Lerp(Color a, Color b, float t) { return a.Lerp(b, t); }

} // namespace BitMotor
