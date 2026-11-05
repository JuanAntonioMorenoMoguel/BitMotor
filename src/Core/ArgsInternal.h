#pragma once
#include <string>
#include <vector>

namespace BitMotor::Detail {

// Replaces the arguments read from the operating system (tests).
void SetArgs(std::vector<std::string> args);

} // namespace BitMotor::Detail
