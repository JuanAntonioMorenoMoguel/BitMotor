#include "Printing.h"
#include "Testing.h"

using namespace BitMotor;

TEST(ColorFromHexAndBytes) {
    constexpr Color orange = Color::Hex(0xFF8000);
    static_assert(orange.R == 1.0f && orange.B == 0.0f && orange.A == 1.0f);
    CHECK_NEAR(orange.G, 128.0 / 255.0, 1e-6);
    CHECK_EQ(Color::Bytes(255, 128, 0), orange);
    CHECK_EQ(Color::Hex("#FF8000"), orange);
    CHECK_EQ(Color::Hex("ff8000"), orange);
    CHECK_EQ(Color::Hex("#FF800080"), Color::Bytes(255, 128, 0, 128));
    CHECK_EQ(orange.ToHex(), std::string("#FF8000FF"));
}

TEST(ColorInvalidTextIsMagentaAndWarns) {
    std::size_t before = Log::Problems();
    CHECK_EQ(Color::Hex("#12345"), Colors::Magenta);
    CHECK_EQ(Color::Hex("#GG0000"), Colors::Magenta);
    CHECK_EQ(Log::Problems(), before + 2);
}

TEST(ColorOperations) {
    CHECK_EQ(Colors::Red.WithAlpha(0.5f), Color(1, 0, 0, 0.5f));
    CHECK_EQ(Colors::Black.Lighter(0.5f), Color(0.5f, 0.5f, 0.5f));
    CHECK_EQ(Colors::White.Darker(0.25f), Color(0.75f, 0.75f, 0.75f));
    CHECK_EQ(Colors::Black.Lerp(Colors::White.WithAlpha(0), 0.5f), Color(0.5f, 0.5f, 0.5f, 0.5f));
    CHECK_EQ(Lerp(Colors::Black, Colors::White, 1.0f), Colors::White);
    CHECK_EQ(Colors::Transparent.A, 0.0f);
    CHECK_EQ(Colors::Phosphor.ToHex(), std::string("#33FF66FF"));
}
