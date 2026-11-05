#include "Testing.h"

#include <BitMotor/BitMotor.h>

using namespace BitMotor;

TEST(LogCountsWarningsAndErrorsButNotInfo) {
    std::size_t before = Log::Problems();
    Log::Info("just information");
    CHECK_EQ(Log::Problems(), before);
    Log::Warn("a warning about {}", "something");
    Log::Error("an error with code {}", 42);
    CHECK_EQ(Log::Problems(), before + 2);
}

TEST(LogKeepsRecentMessagesNewestFirst) {
    Log::Info("first {}", 1);
    Log::Warn("second {}", 2);
    CHECK(Log::RecentCount() >= 2);
    CHECK_EQ(Log::Recent(0), std::string("[warn] second 2"));
    CHECK_EQ(Log::Recent(1), std::string("[info] first 1"));
    CHECK_EQ(Log::Recent(1000), std::string());
}

TEST(LogRingKeepsOnlyTheLatest64) {
    for (int i = 0; i < 100; ++i) Log::Info("message {}", i);
    CHECK_EQ(Log::RecentCount(), std::size_t(64));
    CHECK_EQ(Log::Recent(0), std::string("[info] message 99"));
    CHECK_EQ(Log::Recent(63), std::string("[info] message 36"));
}
