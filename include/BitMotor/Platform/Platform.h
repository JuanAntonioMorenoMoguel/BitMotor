#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Achievements, statistics, leaderboards and DLC, all under Platform:: (games often have their own types
// called Stats or Achievements). For now they are kept on this computer (platform.json in Files::SavePath,
// written at the end of the frame that changed something); a store such as Steam will plug in behind the same
// calls, so games never need to change.
//     if (kills >= 100) Platform::Unlock("hunter");                       // true the first time
//     Platform::Progress("marathon", Platform::AddStat("runs"), 100);    // unlocks itself at 100
//     Platform::MaxStat("best_wave", wave);
//     int rank = Platform::Submit("score", score, playerName);           // 0 is the best, -1 if it did not enter
//     for (const LeaderboardEntry& e : Platform::Top("score")) ...
//     if (Platform::OwnsDlc("desert")) ...                               // locally: assets/dlc/desert exists
//     Subscription toast = Events::On<AchievementUnlocked>([](const AchievementUnlocked& a) { Show(a.Id); });
namespace BitMotor {

// Sent when an achievement is unlocked for the first time.
struct AchievementUnlocked {
    std::string Id;
};

struct LeaderboardEntry {
    std::int64_t Score = 0;
    std::string Name;
    std::string Date; // "YYYY-MM-DD"
};

// How a board ranks and how many entries it keeps. Boards that were never configured keep 10, highest first.
struct LeaderboardOptions {
    int Keep = 10;
    bool LowestFirst = false; // times: the fastest wins
};

namespace Platform {

// Where the data lives: "local" for now.
std::string_view Name();
// Writes now what changed (it also happens at the end of each frame and when the game closes).
void Save();

// ---- Achievements
// Unlocks it. True if it was not unlocked before.
bool Unlock(std::string_view id);
bool Unlocked(std::string_view id);
// Records how close it is (shown by the store) and unlocks it once `value` reaches `target`. True if this call
// unlocked it.
bool Progress(std::string_view id, std::int64_t value, std::int64_t target);
// The last value given to Progress (0 if none).
std::int64_t ProgressOf(std::string_view id);
// When it was unlocked, "YYYY-MM-DD" in local time; empty while it is locked.
std::string UnlockedOn(std::string_view id);
// The unlocked ones, in the order they were unlocked.
std::vector<std::string> Achievements();
// Locks it again (or every one with an empty id): for testing and debug menus.
void ResetAchievements(std::string_view id = {});

// ---- Statistics
std::int64_t Stat(std::string_view name, std::int64_t fallback = 0);
void SetStat(std::string_view name, std::int64_t value);
// Adds `amount` and returns the new value.
std::int64_t AddStat(std::string_view name, std::int64_t amount = 1);
// Keeps the highest value seen (best scores, longest runs). Returns the value kept.
std::int64_t MaxStat(std::string_view name, std::int64_t value);

// ---- Leaderboards
void ConfigureBoard(std::string_view board, LeaderboardOptions options);
// Adds a score. Returns its rank (0 is the best) or -1 if it did not make the board.
int Submit(std::string_view board, std::int64_t score, std::string_view name = {});
// The best `count` entries (all of them with 0), best first.
std::vector<LeaderboardEntry> Top(std::string_view board, int count = 0);
// The best score, or `fallback` if the board is empty.
std::int64_t Best(std::string_view board, std::int64_t fallback = 0);
void ClearBoard(std::string_view board);

// ---- DLC
// Whether the player has it. Locally: the folder dlc/<id> exists among the game's assets (Files::AssetPath).
bool OwnsDlc(std::string_view id);
// Every DLC the player has.
std::vector<std::string> Dlcs();

} // namespace Platform

} // namespace BitMotor
