#pragma once

namespace BitMotor::Detail {

// The loop drives game time with these. Per frame:
//     int updates = BeginFrame(realSeconds);
//     for (int i = 0; i < updates; ++i) { BeginUpdate(); game.Update(); }
//     BeginDraw(); game.Draw();
void ResetTime(int tickRate);
// Returns how many Updates this frame runs (at most MaxUpdatesPerFrame: a slow machine runs the game
// slower instead of freezing while it tries to catch up).
int BeginFrame(double realDt);
void BeginUpdate();
void BeginDraw();

inline constexpr int MaxUpdatesPerFrame = 8;

} // namespace BitMotor::Detail
