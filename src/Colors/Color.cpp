#include "BitMotor/Colors/Color.h"

#include "BitMotor/Core/Log.h"

#include <cmath>
#include <format>

namespace BitMotor {

namespace {
int HexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int Byte(float channel) {
    float clamped = channel < 0.0f ? 0.0f : channel > 1.0f ? 1.0f : channel;
    return int(std::lround(clamped * 255.0f));
}
} // namespace

Color Color::Hex(std::string_view text) {
    std::string_view digits = !text.empty() && text.front() == '#' ? text.substr(1) : text;
    if (digits.size() == 6 || digits.size() == 8) {
        int bytes[4] = {0, 0, 0, 255};
        bool ok = true;
        for (std::size_t i = 0; i < digits.size() / 2 && ok; ++i) {
            int high = HexDigit(digits[i * 2]), low = HexDigit(digits[i * 2 + 1]);
            ok = high >= 0 && low >= 0;
            bytes[i] = high * 16 + low;
        }
        if (ok) return Bytes(bytes[0], bytes[1], bytes[2], bytes[3]);
    }
    Log::Warn("'{}' is not a colour: use \"#RRGGBB\" or \"#RRGGBBAA\"", text);
    return {1, 0, 1, 1};
}

std::string Color::ToHex() const {
    return std::format("#{:02X}{:02X}{:02X}{:02X}", Byte(R), Byte(G), Byte(B), Byte(A));
}

} // namespace BitMotor
