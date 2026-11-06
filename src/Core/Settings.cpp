#include "BitMotor/Core/Settings.h"

#include "BitMotor/Core/Log.h"
#include "BitMotor/Core/Window.h"
#include "BitMotor/Math/Functions.h"
#include "Core/SettingsInternal.h"

#include <cstdlib>

namespace BitMotor {

namespace Detail {

namespace {

std::unique_ptr<Settings> Current;
SettingsHooks Hooks;
bool DefaultFullscreen = false;
bool DefaultVSync = true;

SettingsHooks BaseHooks() {
    return {[](const Settings& s) { return Data::Save(SettingsFile, s); },
            [](Settings& s) {
                s = Settings{};
                SettingsDefaults(s);
            }};
}

} // namespace

void UseSettings(std::unique_ptr<Settings> settings, SettingsHooks hooks) {
    Current = std::move(settings);
    Hooks = std::move(hooks);
    Settings::Apply();
}

Settings& CurrentSettings() {
    if (!Current) {
        Current = std::make_unique<Settings>();
        SettingsDefaults(*Current);
        Hooks = BaseHooks();
    }
    return *Current;
}

void SettingsDefaults(Settings& settings) {
    settings.Fullscreen = DefaultFullscreen;
    settings.VSync = DefaultVSync;
}

void WrongSettingsType(const char* asked) {
    Log::Error("Settings::Get<{}>: the settings in use are another type (Settings::Use<{}> in Game::Start)", asked,
               asked);
    std::abort();
}

void SetSettingsDefaults(bool fullscreen, bool vsync) {
    DefaultFullscreen = fullscreen;
    DefaultVSync = vsync;
}

void PreloadSettings() {
    auto settings = std::make_unique<Settings>();
    SettingsDefaults(*settings);
    ReadSettings(*settings, true);
    UseSettings(std::move(settings), BaseHooks());
}

void ForgetSettings() {
    Current.reset();
    Hooks = {};
}

} // namespace Detail

void Settings::Apply() {
    Settings& s = Detail::CurrentSettings();
    s.MasterVolume = Clamp(s.MasterVolume, 0.0f, 1.0f);
    s.MusicVolume = Clamp(s.MusicVolume, 0.0f, 1.0f);
    s.SoundVolume = Clamp(s.SoundVolume, 0.0f, 1.0f);
    s.Vibration = Clamp(s.Vibration, 0.0f, 1.0f);
    s.ScreenShake = Clamp(s.ScreenShake, 0.0f, 1.0f);
    s.TextScale = Clamp(s.TextScale, 0.5f, 3.0f);
    // Copies: these calls write Fullscreen and VSync back into the settings.
    const bool fullscreen = s.Fullscreen, vsync = s.VSync;
    if (Window::IsFullscreen() != fullscreen) Window::SetFullscreen(fullscreen);
    if (Window::VSync() != vsync) Window::SetVSync(vsync);
    s.Fullscreen = fullscreen;
    s.VSync = vsync;
}

bool Settings::Save() {
    Settings& s = Detail::CurrentSettings();
    return Detail::Hooks.Save ? Detail::Hooks.Save(s) : false;
}

void Settings::Reset() {
    Settings& s = Detail::CurrentSettings();
    if (Detail::Hooks.Reset) Detail::Hooks.Reset(s);
    Apply();
}

} // namespace BitMotor
