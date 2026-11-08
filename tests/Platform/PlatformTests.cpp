#include "Testing.h"

#include "Core/FilesInternal.h"
#include "Platform/PlatformInternal.h"

#include <BitMotor/BitMotor.h>

#include <filesystem>

using namespace BitMotor;

namespace {

std::filesystem::path FreshPlatform(const char* name) {
    std::filesystem::path folder = std::filesystem::temp_directory_path() / "BitMotorTests" / name;
    std::filesystem::remove_all(folder);
    Detail::SetSavePath(folder);
    Detail::ResetPlatform();
    return folder;
}

} // namespace

TEST(AchievementsUnlockOnceAndSendAnEvent) {
    FreshPlatform("PlatformUnlock");
    int events = 0;
    std::string last;
    Subscription s = Events::On<AchievementUnlocked>([&](const AchievementUnlocked& a) {
        ++events;
        last = a.Id;
    });
    CHECK(!Platform::Unlocked("first"));
    CHECK(Platform::Unlock("first"));
    CHECK(!Platform::Unlock("first"));
    CHECK(Platform::Unlocked("first"));
    CHECK_EQ(events, 1);
    CHECK_EQ(last, std::string("first"));
    CHECK_EQ(Platform::UnlockedOn("first").size(), std::size_t(10)); // YYYY-MM-DD
    CHECK(Platform::UnlockedOn("other").empty());
    CHECK(!Platform::Unlock(""));
}

TEST(AchievementsProgressUnlocksAtTheTarget) {
    FreshPlatform("PlatformProgress");
    CHECK(!Platform::Progress("runs", 3, 5));
    CHECK_EQ(Platform::ProgressOf("runs"), std::int64_t(3));
    CHECK(Platform::Progress("runs", 5, 5));
    CHECK(!Platform::Progress("runs", 6, 5)); // already unlocked
    CHECK(Platform::Unlock("b"));
    auto list = Platform::Achievements();
    CHECK_EQ(list.size(), std::size_t(2));
    CHECK_EQ(list[0], std::string("runs")); // unlock order
    Platform::ResetAchievements("runs");
    CHECK(!Platform::Unlocked("runs"));
    Platform::ResetAchievements();
    CHECK(Platform::Achievements().empty());
}

TEST(StatsAddSetAndMax) {
    FreshPlatform("PlatformStats");
    CHECK_EQ(Platform::Stat("kills"), std::int64_t(0));
    CHECK_EQ(Platform::Stat("kills", 7), std::int64_t(7));
    CHECK_EQ(Platform::AddStat("kills"), std::int64_t(1));
    CHECK_EQ(Platform::AddStat("kills", 10), std::int64_t(11));
    Platform::SetStat("kills", 2);
    CHECK_EQ(Platform::Stat("kills"), std::int64_t(2));
    CHECK_EQ(Platform::MaxStat("wave", 4), std::int64_t(4));
    CHECK_EQ(Platform::MaxStat("wave", 2), std::int64_t(4));
    CHECK_EQ(Platform::MaxStat("wave", 9), std::int64_t(9));
}

TEST(LeaderboardsRankAndKeepTheBest) {
    FreshPlatform("PlatformBoards");
    Platform::ConfigureBoard("score", {.Keep = 3});
    CHECK_EQ(Platform::Submit("score", 100, "a"), 0);
    CHECK_EQ(Platform::Submit("score", 300, "b"), 0);
    CHECK_EQ(Platform::Submit("score", 200, "c"), 1);
    CHECK_EQ(Platform::Submit("score", 200, "d"), 2); // after the earlier 200
    CHECK_EQ(Platform::Submit("score", 50, "e"), -1);
    auto top = Platform::Top("score");
    CHECK_EQ(top.size(), std::size_t(3));
    CHECK_EQ(top[0].Name, std::string("b"));
    CHECK_EQ(top[1].Name, std::string("c"));
    CHECK_EQ(top[2].Name, std::string("d"));
    CHECK_EQ(Platform::Top("score", 1).size(), std::size_t(1));
    CHECK_EQ(Platform::Best("score"), std::int64_t(300));
    CHECK_EQ(Platform::Best("none", -1), std::int64_t(-1));

    Platform::ConfigureBoard("time", {.Keep = 2, .LowestFirst = true});
    Platform::Submit("time", 90);
    CHECK_EQ(Platform::Submit("time", 60), 0);
    CHECK_EQ(Platform::Best("time"), std::int64_t(60));
    Platform::ClearBoard("time");
    CHECK(Platform::Top("time").empty());
}

TEST(PlatformSurvivesARestart) {
    std::filesystem::path folder = FreshPlatform("PlatformFile");
    Platform::Unlock("kept");
    Platform::SetStat("coins", 42);
    Platform::Submit("score", 10, "me");
    Platform::Save();
    CHECK(std::filesystem::exists(folder / "platform.json"));
    Detail::ResetPlatform(); // a new run of the game
    CHECK(Platform::Unlocked("kept"));
    CHECK_EQ(Platform::Stat("coins"), std::int64_t(42));
    auto top = Platform::Top("score");
    CHECK_EQ(top.size(), std::size_t(1));
    if (!top.empty()) CHECK_EQ(top[0].Name, std::string("me"));
    CHECK_EQ(std::string(Platform::Name()), std::string("local"));
}

TEST(DlcIsOwnedOnlyWhenItsFolderExists) {
    CHECK(!Platform::OwnsDlc(""));
    CHECK(!Platform::OwnsDlc("this_dlc_does_not_exist"));
}
