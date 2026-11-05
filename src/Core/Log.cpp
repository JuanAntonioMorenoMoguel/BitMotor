#include "BitMotor/Core/Log.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <mutex>

namespace BitMotor::Log {

namespace {
std::mutex Mutex;
std::array<std::string, 64> Ring;
std::size_t Count = 0;
std::size_t ProblemCount = 0;
} // namespace

void Write(Level level, const std::string& message) {
    const char* prefix = level == Level::Info ? "[info] " : level == Level::Warning ? "[warn] " : "[error] ";
    std::lock_guard lock(Mutex);
    if (level != Level::Info) ++ProblemCount;
    std::FILE* out = level == Level::Error ? stderr : stdout;
    std::fprintf(out, "%s%s\n", prefix, message.c_str());
    std::fflush(out);
    Ring[Count % Ring.size()] = prefix + message;
    ++Count;
}

std::size_t Problems() {
    std::lock_guard lock(Mutex);
    return ProblemCount;
}

std::size_t RecentCount() {
    std::lock_guard lock(Mutex);
    return std::min(Count, Ring.size());
}

std::string Recent(std::size_t index) {
    std::lock_guard lock(Mutex);
    if (index >= std::min(Count, Ring.size())) return {};
    return Ring[(Count - 1 - index) % Ring.size()];
}

} // namespace BitMotor::Log
