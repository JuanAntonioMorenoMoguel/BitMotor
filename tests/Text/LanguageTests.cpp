#include "Printing.h"
#include "Testing.h"

#include "BitMotor/Core/Files.h"
#include "Text/LanguageInternal.h"

#include <filesystem>

using namespace BitMotor;

namespace {

// A lang/ folder with English and Spanish, used for the language until the guard goes.
struct Languages {
    std::filesystem::path Folder = std::filesystem::temp_directory_path() / "BitMotorTests" / "lang";

    Languages() {
        std::filesystem::remove_all(Folder);
        Files::Write(Folder / "en.json", R"({"language": "English", "menu": {"play": "Play", "quit": "Quit"},
                                            "score": "Score: {0}", "pair": "{1} and {0}", "only.english": "Hi",
                                            "level": 3})");
        Files::Write(Folder / "es.json", R"({"language": "Español", "menu": {"play": "Jugar"},
                                            "score": "Puntos: {0}", "pair": "{1} y {0}"})");
        Files::Write(Folder / "broken.json", "{ nope");
        Detail::SetLanguageFolder(Folder);
        Settings::Get().Language.clear();
    }
    ~Languages() {
        Detail::SetLanguageFolder(std::nullopt);
        Settings::Get().Language.clear();
        std::filesystem::remove_all(Folder);
    }
};

} // namespace

TEST(LanguageTranslatesWithFallbacks) {
    Languages languages;
    Text::SetLanguage("es");
    CHECK_EQ(Text::Language(), "es");
    CHECK_EQ(Settings::Get().Language, "es");
    CHECK_EQ(Tr("menu.play"), "Jugar");
    CHECK_EQ(Tr("menu.quit"), "Quit");            // missing in Spanish: English
    CHECK_EQ(Tr("menu.nothing"), "menu.nothing"); // missing everywhere: the key
    CHECK_EQ(Tr("score", 12), "Puntos: 12");
    CHECK_EQ(Tr("pair", "a", std::string("b")), "b y a");
    CHECK_EQ(Tr("level"), "3");
    Text::SetLanguage("en");
    CHECK_EQ(Tr("menu.play"), "Play");
}

TEST(LanguageFindsFilesAndNames) {
    Languages languages;
    std::vector<std::string> codes = Text::Languages();
    CHECK_EQ(codes.size(), 3u);
    CHECK_EQ(codes[1], "en");
    CHECK_EQ(Text::LanguageName("es"), "Español");
    CHECK_EQ(Text::LanguageName("xx"), "xx");
    Text::SetLanguage("es-MX"); // falls back to its base language
    CHECK_EQ(Text::Language(), "es");
    Text::SetLanguage("xx"); // unknown: English (with a warning)
    CHECK_EQ(Text::Language(), "en");
    Text::SetLanguage("broken"); // unreadable: its keys come from English
    CHECK_EQ(Tr("menu.play"), "Play");
}

TEST(LanguageChangesAreAnnounced) {
    Languages languages;
    Text::SetLanguage("en");
    std::string heard;
    Subscription listen = Events::On<LanguageChanged>([&](const LanguageChanged& e) { heard = e.Language; });
    Text::SetLanguage("es");
    CHECK_EQ(heard, "es");
    heard.clear();
    Text::SetLanguage("es");
    CHECK_EQ(heard, ""); // same language: nothing
    Settings::Get().Language = "en";
    Settings::Apply(); // the settings switch it too
    CHECK_EQ(heard, "en");
    CHECK_EQ(Tr("menu.play"), "Play");
}

TEST(NumbersFollowTheLanguage) {
    Languages languages;
    Text::SetLanguage("en");
    CHECK_EQ(Text::Number(0), "0");
    CHECK_EQ(Text::Number(999), "999");
    CHECK_EQ(Text::Number(12500), "12,500");
    CHECK_EQ(Text::Number(-1234567), "-1,234,567");
    CHECK_EQ(Text::Number(1234.5, 2), "1,234.50");
    CHECK_EQ(Text::Number(-0.001), "0"); // no "-0"
    CHECK_EQ(Text::ShortNumber(950), "950");
    CHECK_EQ(Text::ShortNumber(1000), "1K");
    CHECK_EQ(Text::ShortNumber(12500), "12.5K");
    CHECK_EQ(Text::ShortNumber(999960), "1M"); // rounds up into the next unit
    CHECK_EQ(Text::ShortNumber(3400000), "3.4M");
    CHECK_EQ(Text::ShortNumber(-2500000000.0), "-2.5B");
    CHECK_EQ(Text::ShortNumber(250000), "250K");
    CHECK_EQ(Text::Percent(0.42), "42%");
    CHECK_EQ(Text::Percent(0.1234, 1), "12.3%");
    Text::SetLanguage("es");
    CHECK_EQ(Text::Number(12500), "12.500");
    CHECK_EQ(Text::Number(1234.5, 2), "1.234,50");
    CHECK_EQ(Text::ShortNumber(12500), "12,5K");
}

TEST(ClockFormatsSeconds) {
    CHECK_EQ(Text::Clock(0), "0:00");
    CHECK_EQ(Text::Clock(65), "1:05");
    CHECK_EQ(Text::Clock(65.9), "1:05");
    CHECK_EQ(Text::Clock(3725), "1:02:05");
    CHECK_EQ(Text::Clock(65.32, true), "1:05.32");
    CHECK_EQ(Text::Clock(59.999, true), "0:59.99");
    CHECK_EQ(Text::Clock(-5), "-0:05");
}
