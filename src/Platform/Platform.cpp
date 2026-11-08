// The local platform: everything in one JSON file in the save folder. A store backend (Steam) will receive the
// same calls from here and keep this file as its offline copy.
#include "BitMotor/Platform/Platform.h"

#include "BitMotor/Core/Data.h"
#include "BitMotor/Core/Events.h"
#include "BitMotor/Core/Files.h"
#include "BitMotor/Core/Log.h"
#include "Platform/PlatformInternal.h"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <map>
#include <optional>

namespace BitMotor {

BITMOTOR_FIELDS(LeaderboardEntry, Score, Name, Date)

namespace {

struct BoardData {
    int Keep = 10;
    bool LowestFirst = false;
    std::vector<LeaderboardEntry> Entries;
};
BITMOTOR_FIELDS(BoardData, Keep, LowestFirst, Entries)

struct PlatformData {
    std::vector<std::string> Achievements; // in unlock order
    std::map<std::string, std::string> UnlockedOn;
    std::map<std::string, std::int64_t> Progress;
    std::map<std::string, std::int64_t> Stats;
    std::map<std::string, BoardData> Leaderboards;
};
BITMOTOR_FIELDS(PlatformData, Achievements, UnlockedOn, Progress, Stats, Leaderboards)

constexpr const char* FileName = "platform.json";

std::optional<PlatformData> Loaded;
bool Dirty = false;

PlatformData& State() {
    if (!Loaded) Loaded = Data::Load<PlatformData>(FileName).value_or(PlatformData{});
    return *Loaded;
}

void Changed() { Dirty = true; }

std::string Today() {
    std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char text[16];
    std::strftime(text, sizeof(text), "%Y-%m-%d", &local);
    return text;
}

BoardData& Board(std::string_view name) { return State().Leaderboards[std::string(name)]; }

bool Better(const BoardData& board, std::int64_t a, std::int64_t b) { return board.LowestFirst ? a < b : a > b; }

} // namespace

namespace Platform {

// ---- Achievements

bool Unlock(std::string_view id) {
    if (id.empty() || Unlocked(id)) return false;
    PlatformData& data = State();
    data.Achievements.emplace_back(id);
    data.UnlockedOn[std::string(id)] = Today();
    Changed();
    Log::Info("Achievement unlocked: {}", id);
    Events::Emit(AchievementUnlocked{std::string(id)});
    return true;
}

bool Unlocked(std::string_view id) {
    const auto& list = State().Achievements;
    return std::find(list.begin(), list.end(), id) != list.end();
}

bool Progress(std::string_view id, std::int64_t value, std::int64_t target) {
    std::int64_t& stored = State().Progress[std::string(id)];
    if (stored != value) {
        stored = value;
        Changed();
    }
    return value >= target && Unlock(id);
}

std::int64_t ProgressOf(std::string_view id) {
    const auto& progress = State().Progress;
    auto found = progress.find(std::string(id));
    return found == progress.end() ? 0 : found->second;
}

std::string UnlockedOn(std::string_view id) {
    const auto& dates = State().UnlockedOn;
    auto found = dates.find(std::string(id));
    return found == dates.end() ? std::string() : found->second;
}

std::vector<std::string> Achievements() { return State().Achievements; }

void ResetAchievements(std::string_view id) {
    PlatformData& data = State();
    if (id.empty()) {
        data.Achievements.clear();
        data.UnlockedOn.clear();
        data.Progress.clear();
    } else {
        std::erase(data.Achievements, std::string(id));
        data.UnlockedOn.erase(std::string(id));
        data.Progress.erase(std::string(id));
    }
    Changed();
}

// ---- Statistics

std::int64_t Stat(std::string_view name, std::int64_t fallback) {
    const auto& stats = State().Stats;
    auto found = stats.find(std::string(name));
    return found == stats.end() ? fallback : found->second;
}

void SetStat(std::string_view name, std::int64_t value) {
    auto [it, added] = State().Stats.try_emplace(std::string(name), value);
    if (!added && it->second == value) return;
    it->second = value;
    Changed();
}

std::int64_t AddStat(std::string_view name, std::int64_t amount) {
    std::int64_t value = Stat(name) + amount;
    SetStat(name, value);
    return value;
}

std::int64_t MaxStat(std::string_view name, std::int64_t value) {
    const auto& stats = State().Stats;
    auto found = stats.find(std::string(name));
    if (found != stats.end() && found->second >= value) return found->second;
    SetStat(name, value);
    return value;
}

// ---- Leaderboards

void ConfigureBoard(std::string_view board, LeaderboardOptions options) {
    BoardData& data = Board(board);
    const int keep = std::max(1, options.Keep);
    if (data.Keep == keep && data.LowestFirst == options.LowestFirst) return;
    data.Keep = keep;
    data.LowestFirst = options.LowestFirst;
    std::stable_sort(
        data.Entries.begin(), data.Entries.end(),
        [&](const LeaderboardEntry& a, const LeaderboardEntry& b) { return Better(data, a.Score, b.Score); });
    if (int(data.Entries.size()) > data.Keep) data.Entries.resize(std::size_t(data.Keep));
    Changed();
}

int Submit(std::string_view board, std::int64_t score, std::string_view name) {
    BoardData& data = Board(board);
    // After the entries with the same score: the first to reach it keeps the place.
    auto at = std::find_if(data.Entries.begin(), data.Entries.end(),
                           [&](const LeaderboardEntry& e) { return Better(data, score, e.Score); });
    const int rank = int(at - data.Entries.begin());
    if (rank >= data.Keep) return -1;
    data.Entries.insert(at, LeaderboardEntry{score, std::string(name), Today()});
    if (int(data.Entries.size()) > data.Keep) data.Entries.resize(std::size_t(data.Keep));
    Changed();
    return rank;
}

std::vector<LeaderboardEntry> Top(std::string_view board, int count) {
    const auto& boards = State().Leaderboards;
    auto found = boards.find(std::string(board));
    if (found == boards.end()) return {};
    const auto& entries = found->second.Entries;
    std::size_t n = count > 0 ? std::min(entries.size(), std::size_t(count)) : entries.size();
    return {entries.begin(), entries.begin() + std::ptrdiff_t(n)};
}

std::int64_t Best(std::string_view board, std::int64_t fallback) {
    auto top = Top(board, 1);
    return top.empty() ? fallback : top.front().Score;
}

void ClearBoard(std::string_view board) {
    if (State().Leaderboards.erase(std::string(board))) Changed();
}

// ---- DLC

bool OwnsDlc(std::string_view id) {
    if (id.empty()) return false;
    std::error_code error;
    return std::filesystem::is_directory(Files::AssetPath(std::filesystem::path("dlc") / std::string(id)), error);
}

std::vector<std::string> Dlcs() {
    std::vector<std::string> owned;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(Files::AssetPath("dlc"), error))
        if (entry.is_directory(error)) owned.push_back(entry.path().filename().string());
    std::sort(owned.begin(), owned.end());
    return owned;
}

std::string_view Name() { return "local"; }

void Save() {
    if (!Dirty || !Loaded) return;
    if (Data::Save(FileName, *Loaded)) Dirty = false;
}

} // namespace Platform

namespace Detail {

void FlushPlatform() { Platform::Save(); }

void ResetPlatform() {
    Loaded.reset();
    Dirty = false;
}

} // namespace Detail

} // namespace BitMotor
