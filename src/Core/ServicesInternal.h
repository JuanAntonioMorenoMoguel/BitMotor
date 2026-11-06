#pragma once
#include <cstddef>

namespace BitMotor::Detail {

// The game loop calls these: UpdateServices once per Update, StopServices when the game ends.
void UpdateServices();
// Stops every service in reverse order and destroys them.
void StopServices();
std::size_t ServiceCount();

} // namespace BitMotor::Detail
