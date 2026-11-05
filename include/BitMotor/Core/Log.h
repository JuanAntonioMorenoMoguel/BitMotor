#pragma once
#include <cstddef>
#include <format>
#include <string>
#include <utility>

// Messages to the console, with a counter of problems and a ring of recent messages.
//     Log::Info("Loaded {} enemies", count);
//     Log::Warn("Unknown field '{}'", name);
namespace BitMotor::Log {

enum class Level { Info, Warning, Error };

void Write(Level level, const std::string& message);

template <class... A> void Info(std::format_string<A...> format, A&&... args) {
    Write(Level::Info, std::format(format, std::forward<A>(args)...));
}
template <class... A> void Warn(std::format_string<A...> format, A&&... args) {
    Write(Level::Warning, std::format(format, std::forward<A>(args)...));
}
template <class... A> void Error(std::format_string<A...> format, A&&... args) {
    Write(Level::Error, std::format(format, std::forward<A>(args)...));
}

// Warnings + errors written since startup.
std::size_t Problems();

// Latest messages, to show them in a debug panel. Recent(0) is the newest; out of range returns "".
std::string Recent(std::size_t index);
std::size_t RecentCount();

} // namespace BitMotor::Log
