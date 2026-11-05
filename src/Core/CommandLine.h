#pragma once
#include <string>
#include <vector>

namespace BitMotor::Detail {

// The program's arguments (without the executable), read from the operating system so that Run()
// does not need argc/argv. Phase 1 builds the public Args module on top of this.
std::vector<std::string> CommandLine();

} // namespace BitMotor::Detail
