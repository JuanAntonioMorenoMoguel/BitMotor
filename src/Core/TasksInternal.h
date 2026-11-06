#pragma once

namespace BitMotor::Detail {

// The game loop calls these: UpdateTasks once per Update (after the Services), ClearTasks when the game
// ends.
void UpdateTasks();
void ClearTasks();

} // namespace BitMotor::Detail
