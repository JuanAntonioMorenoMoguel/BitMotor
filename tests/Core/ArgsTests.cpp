#include "Testing.h"

#include "Core/ArgsInternal.h"

#include <BitMotor/BitMotor.h>

using namespace BitMotor;

TEST(ArgsHasAndGet) {
    Detail::SetArgs({"--debug", "--level", "3", "--lang=es", "--speed", "1.5", "--offset", "-4", "--last"});
    CHECK(Args::Has("--debug"));
    CHECK(Args::Has("--lang"));
    CHECK(Args::Has("--last"));
    CHECK(!Args::Has("--missing"));
    CHECK(!Args::Has("--lan")); // a prefix is not the option
    CHECK_EQ(Args::Get("--level", 1), 3);
    CHECK_EQ(Args::Get("--lang", "en"), std::string("es"));
    CHECK_NEAR(Args::Get("--speed", 1.0f), 1.5, 1e-6);
    CHECK_EQ(Args::Get("--offset", 0), -4);
    CHECK_EQ(Args::Get("--missing", 7), 7);
    CHECK_EQ(Args::Get("--debug", "x"), std::string("")); // followed by another option: no value
    CHECK_EQ(Args::Get("--last", "x"), std::string(""));
    CHECK_EQ(Args::All().size(), std::size_t(9));
    Detail::SetArgs({});
}

TEST(ArgsBadNumbersWarnAndUseTheFallback) {
    Detail::SetArgs({"--level", "three", "--speed", "1.5x"});
    std::size_t before = Log::Problems();
    CHECK_EQ(Args::Get("--level", 1), 1);
    CHECK_NEAR(Args::Get("--speed", 2.0f), 2.0, 1e-6);
    CHECK_EQ(Log::Problems(), before + 2);
    Detail::SetArgs({});
}
