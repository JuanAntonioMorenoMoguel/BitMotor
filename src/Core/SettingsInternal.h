#pragma once

namespace BitMotor::Detail {

// The game loop, before opening the window: Config gives the defaults, then the standard settings are read
// from settings.json (quietly: the game's own fields are read later by Settings::Use).
void SetSettingsDefaults(bool fullscreen, bool vsync);
void PreloadSettings();
// Drops the current settings (tests, and the end of the game).
void ForgetSettings();

} // namespace BitMotor::Detail
