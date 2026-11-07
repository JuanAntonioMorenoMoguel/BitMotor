#pragma once
#include <filesystem>
#include <optional>
#include <string>

namespace BitMotor::Detail {

// Settings::Apply: loads another language if Settings::Language changed.
void ApplyLanguageSetting(const std::string& requested);
// Where the lang/ files are (tests), and forgets the loaded language. nullopt: Files::AssetPath("lang").
void SetLanguageFolder(std::optional<std::filesystem::path> folder);

} // namespace BitMotor::Detail
