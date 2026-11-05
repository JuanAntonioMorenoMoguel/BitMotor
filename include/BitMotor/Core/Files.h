#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Reading and writing files, and where they live.
//     auto text = Files::Read(Files::AssetPath("levels/1.txt"));
//     if (!text) Log::Error("level 1 is missing");
//     Files::Write(Files::SavePath("notes.txt"), "hello");   // atomic: never a half-written file
//
//     Files::Watch(Files::AssetPath("shaders"));            // recursive, checked twice per second
//     Subscription reload = Events::On<FileChanged>([](const FileChanged& e) { Reload(e.Path); });
namespace BitMotor {

// Sent (at the end of a frame) when a watched file is modified or created.
struct FileChanged {
    std::filesystem::path Path;
};

namespace Files {

using Path = std::filesystem::path;

// Whole file, byte by byte (no newline conversion). nullopt if it cannot be read.
std::optional<std::string> Read(const Path& path);
std::optional<std::vector<std::uint8_t>> ReadBytes(const Path& path);

// Writes the whole file, creating the folders it needs. Atomic: the data goes to a temporary file that
// then replaces the old one, so a crash halfway keeps the previous version. false (and a log error) if
// it fails.
bool Write(const Path& path, std::string_view text);
bool WriteBytes(const Path& path, std::span<const std::uint8_t> bytes);

bool Exists(const Path& path);

// A game resource. During development (games prepared with BitMotor_Prepare) it is looked for first in
// the game's source folder, so edits are seen without building; otherwise next to the executable.
Path AssetPath(const Path& relative = {});

// The player's writable folder for saves and settings (per user and per game, from Config::Organization
// and Config::Title). The folder is created if needed.
Path SavePath(const Path& relative = {});

// Watches a file or a folder (recursively) and sends FileChanged when something changes.
void Watch(const Path& path);
void Unwatch(const Path& path);

} // namespace Files

} // namespace BitMotor
