#pragma once
#include <filesystem>
#include <string>
#include <string_view>

namespace BitMotor::Detail {

// Who the game is, for SavePath: called by Run before Start.
void SetIdentity(const std::string& organization, const std::string& application);
// Replaces the save folder (tests).
void SetSavePath(const std::filesystem::path& folder);
// Checks the watched files (every half second of accumulated `dt`; `force` checks now). The loop calls it.
void UpdateWatches(double dt, bool force = false);

// UTF-8 text <-> path, the same on every platform (path::string() is not UTF-8 on Windows).
inline std::filesystem::path Utf8Path(std::string_view text) { return std::u8string(text.begin(), text.end()); }
inline std::string Utf8(const std::filesystem::path& path) {
    auto text = path.u8string();
    return std::string(text.begin(), text.end());
}

} // namespace BitMotor::Detail
