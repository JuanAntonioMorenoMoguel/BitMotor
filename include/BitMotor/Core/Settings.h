#pragma once
#include "BitMotor/Core/Data.h"
#include "BitMotor/Core/Fields.h"

#include <functional>
#include <memory>
#include <string>
#include <typeinfo>

// The player's options, saved in settings.json (Files::SavePath). BitMotor reads the standard ones before
// opening the window, so the game already starts in the right mode. A game adds its own by inheritance:
//     struct MySettings : Settings {
//         bool ShowTimer = false;
//     };
//     BITMOTOR_FIELDS_EXTENDS(MySettings, Settings, ShowTimer)
//
//     Settings::Use<MySettings>();              // in Game::Start: reads settings.json into a MySettings
//     auto& options = Settings::Get<MySettings>();
//     options.Fullscreen = true;
//     Settings::Apply();                        // the window changes now
//     Settings::Save();                         // only when the player confirms (an options menu)
//     Settings::Reset();                        // back to the defaults (not saved until Save)
//
// Without Use, Settings::Get() gives the standard ones. Window::SetFullscreen and SetVSync (Alt+Enter, F11)
// update Fullscreen and VSync too, without saving. Settings are for the main thread only.
namespace BitMotor {

struct Settings {
    // 0..1. Used by Audio (phase 5).
    float MasterVolume = 1;
    float MusicVolume = 1;
    float SoundVolume = 1;
    // Applied at once. Their defaults come from Config the first time the game runs.
    bool Fullscreen = false;
    bool VSync = true;
    // 0..1: multiplies Input::Rumble.
    float Vibration = 1;
    // 0..1: multiplies the camera shake (Camera2D, phase 3).
    float ScreenShake = 1;
    // false: softer white flashes (Sprite2D::Flash, phase 3), for players sensitive to them.
    bool Flashes = true;
    // Multiplies the size of the text (phase 4).
    float TextScale = 1;
    // "en", "es"... Empty: the system language (phase 4).
    std::string Language;

    Settings() = default;
    Settings(const Settings&) = default;
    Settings& operator=(const Settings&) = default;
    virtual ~Settings() = default;

    // Makes `T` (Settings or a type derived from it with BITMOTOR_FIELDS_EXTENDS) the game's settings:
    // reads settings.json into it (missing fields keep their defaults), applies them and returns them.
    template <class T = Settings> static T& Use();
    // The current settings as `T`. Asking for a type that was not given to Use is an error (abort).
    template <class T = Settings> static T& Get();
    // Puts the current values into effect: window mode, VSync; values out of range are corrected.
    static void Apply();
    // Writes settings.json. false if it fails (logged).
    static bool Save();
    // Back to the defaults and applies them. The file keeps the old values until Save.
    static void Reset();
};

BITMOTOR_FIELDS(Settings, MasterVolume, MusicVolume, SoundVolume, Fullscreen, VSync, Vibration, ScreenShake, Flashes,
                TextScale, Language)

namespace Detail {

inline constexpr const char* SettingsFile = "settings.json";

struct SettingsHooks {
    std::function<bool(const Settings&)> Save;
    std::function<void(Settings&)> Reset;
};
// Replaces the current settings (after reading them) and applies them.
void UseSettings(std::unique_ptr<Settings> settings, SettingsHooks hooks);
Settings& CurrentSettings();
// The defaults that come from Config (Fullscreen, VSync).
void SettingsDefaults(Settings& settings);
[[noreturn]] void WrongSettingsType(const char* asked);

// Reads settings.json over `settings`. `quiet`: fields this type does not know are not reported (the engine
// reads only the standard ones before the game says its own type).
template <class T> void ReadSettings(T& settings, bool quiet) {
    Document document = OpenFile(SaveFilePath(SettingsFile), false);
    if (!document.Ok()) return; // first run, or a broken file (logged): defaults
    if (quiet) (void)document.Keys();
    DataAccess::Read(document, settings);
}

} // namespace Detail

template <class T> T& Settings::Use() {
    static_assert(std::is_base_of_v<Settings, T>, "Settings::Use<T>: T must derive from BitMotor::Settings");
    auto settings = std::make_unique<T>();
    Detail::SettingsDefaults(*settings);
    Detail::ReadSettings(*settings, false);
    T& result = *settings;
    Detail::UseSettings(std::move(settings),
                        {[](const Settings& s) { return Data::Save(Detail::SettingsFile, static_cast<const T&>(s)); },
                         [](Settings& s) {
                             static_cast<T&>(s) = T{};
                             Detail::SettingsDefaults(s);
                         }});
    return result;
}

template <class T> T& Settings::Get() {
    if (T* settings = dynamic_cast<T*>(&Detail::CurrentSettings())) return *settings;
    Detail::WrongSettingsType(typeid(T).name());
}

} // namespace BitMotor
