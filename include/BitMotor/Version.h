#pragma once
#include <string_view>

namespace BitMotor {

// BitMotor's version, "Major.Minor.Patch" (the one in the root CMakeLists.txt).
std::string_view Version();

} // namespace BitMotor
