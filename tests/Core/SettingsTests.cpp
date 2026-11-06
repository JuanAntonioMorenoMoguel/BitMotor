#include "Testing.h"

#include "Core/FilesInternal.h"
#include "Core/SettingsInternal.h"

#include <BitMotor/BitMotor.h>

#include <filesystem>
#include <string>

using namespace BitMotor;

namespace {

struct GameSettings : Settings {
    bool ShowTimer = false;
    int Difficulty = 1;
};

std::filesystem::path Fresh(const char* name) {
    std::filesystem::path folder = std::filesystem::temp_directory_path() / "BitMotorTests" / name;
    std::filesystem::remove_all(folder);
    Detail::SetSavePath(folder);
    Detail::ForgetSettings();
    Detail::SetSettingsDefaults(false, true);
    return folder;
}

// In the same namespace as the struct, like BITMOTOR_FIELDS.
BITMOTOR_FIELDS_EXTENDS(GameSettings, Settings, ShowTimer, Difficulty)

} // namespace

// A derived struct without its own list is not taken for its base (which would lose its fields).
struct Forgotten : Settings {
    int Lost = 0;
};
static_assert(Detail::HasFields<Settings>);
static_assert(Detail::HasFields<GameSettings>);
static_assert(!Detail::HasFields<Forgotten>);

TEST(FieldsExtendsWritesTheBaseFirst) {
    GameSettings settings;
    settings.MusicVolume = 0.5f;
    settings.ShowTimer = true;
    std::string json = Data::ToJson(settings);
    CHECK(json.find("\"MusicVolume\": 0.5") != std::string::npos);
    CHECK(json.find("\"ShowTimer\": true") != std::string::npos);
    CHECK(json.find("MasterVolume") < json.find("ShowTimer"));
    auto back = Data::FromJson<GameSettings>(json);
    CHECK(back.has_value());
    CHECK_EQ(back->MusicVolume, 0.5f);
    CHECK(back->ShowTimer);
}

TEST(SettingsDefaultsComeFromConfig) {
    Fresh("SettingsDefaults");
    Detail::SetSettingsDefaults(true, false);
    Detail::PreloadSettings(); // no file yet: defaults, no error
    CHECK(Settings::Get().Fullscreen);
    CHECK(!Settings::Get().VSync);
    CHECK_EQ(Settings::Get().MasterVolume, 1.0f);
    Detail::ForgetSettings();
}

TEST(SettingsUseReadsTheGamesOwnFields) {
    std::filesystem::path folder = Fresh("SettingsUse");
    GameSettings& mine = Settings::Use<GameSettings>();
    CHECK_EQ(&Settings::Get<GameSettings>(), &mine);
    CHECK_EQ(&Settings::Get(), static_cast<Settings*>(&mine)); // the standard view of the same object
    mine.Difficulty = 3;
    mine.SoundVolume = 0.25f;
    CHECK(Settings::Save());
    CHECK(Files::Exists(folder / "settings.json"));

    // Next launch: the engine reads the standard fields quietly, then the game asks for its type.
    Detail::ForgetSettings();
    std::size_t before = Log::Problems();
    Detail::PreloadSettings();
    CHECK_EQ(Settings::Get().SoundVolume, 0.25f);
    CHECK_EQ(Log::Problems(), before); // Difficulty and ShowTimer are not reported as unknown
    GameSettings& again = Settings::Use<GameSettings>();
    CHECK_EQ(again.Difficulty, 3);
    CHECK_EQ(again.SoundVolume, 0.25f);
    CHECK_EQ(Log::Problems(), before);
    Detail::ForgetSettings();
}

TEST(SettingsApplyCorrectsValues) {
    Fresh("SettingsApply");
    Settings& s = Settings::Get();
    s.MasterVolume = 3;
    s.Vibration = -1;
    s.TextScale = 0;
    s.Fullscreen = true;
    Settings::Apply();
    CHECK_EQ(s.MasterVolume, 1.0f);
    CHECK_EQ(s.Vibration, 0.0f);
    CHECK_EQ(s.TextScale, 0.5f);
    CHECK(s.Fullscreen); // kept even with no window to apply it to

    Window::SetFullscreen(false); // F11 or Alt+Enter: the settings follow
    CHECK(!s.Fullscreen);
    Window::SetVSync(false);
    CHECK(!s.VSync);
    Detail::ForgetSettings();
}

TEST(SettingsResetKeepsTheType) {
    Fresh("SettingsReset");
    Detail::SetSettingsDefaults(true, true);
    GameSettings& mine = Settings::Use<GameSettings>();
    mine.Difficulty = 5;
    mine.Fullscreen = false;
    mine.Language = "es";
    Settings::Reset();
    CHECK_EQ(mine.Difficulty, 1);
    CHECK(mine.Fullscreen); // the default from Config
    CHECK(mine.Language.empty());
    CHECK_EQ(&Settings::Get<GameSettings>(), &mine);
    Detail::ForgetSettings();
}
