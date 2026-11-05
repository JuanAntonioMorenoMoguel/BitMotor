#pragma once
#include <string>
#include <string_view>
#include <vector>

// The program's command-line options, without needing argc/argv.
// Both "--name value" and "--name=value" are accepted.
//     if (Args::Has("--debug")) ...
//     int level = Args::Get("--level", 1);
//     std::string lang = Args::Get("--lang", "en");
namespace BitMotor::Args {

bool Has(std::string_view name);

// The value after `name`, or `fallback` if the option is missing. A value that is not a valid number for
// the numeric versions logs a warning and returns the fallback.
std::string Get(std::string_view name, const std::string& fallback = "");
std::string Get(std::string_view name, const char* fallback);
int Get(std::string_view name, int fallback);
float Get(std::string_view name, float fallback);

// Every argument, in order, without the executable.
const std::vector<std::string>& All();

} // namespace BitMotor::Args
