#pragma once
#include <filesystem>

namespace BitMotor::Detail {

// The end of a frame: writes platform.json if something changed.
void FlushPlatform();
// Forgets everything in memory (the file is read again on the next call): when the game stops, and in tests.
void ResetPlatform();

} // namespace BitMotor::Detail
