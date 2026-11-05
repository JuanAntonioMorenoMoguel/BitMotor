#pragma once

namespace BitMotor::Detail {

// Delivers the events queued with Events::Queue. The game loop calls it once per frame.
void FlushEvents();

} // namespace BitMotor::Detail
