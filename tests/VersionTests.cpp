#include "Testing.h"

#include <BitMotor/BitMotor.h>

TEST(VersionMatchesCMake) { CHECK_EQ(BitMotor::Version(), std::string_view(BITMOTOR_EXPECTED_VERSION)); }
