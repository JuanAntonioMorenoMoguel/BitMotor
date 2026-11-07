#pragma once

namespace BitMotor::Detail {

// The game loop drives the scene stack with these:
//     game.Start(); ApplySceneChanges();            // the first scene enters before the first Update
//     per Update: ...; game.Update(); UpdateScenes();   // updates the stack, then applies the changes
//     per frame:  DrawScenes(); game.Draw();
//     at the end: ShutdownScenes();                 // every scene exits, top first
void ApplySceneChanges();
void UpdateScenes();
void DrawScenes();
// The debug panels of every scene in the stack, bottom first.
void DebugPanelScenes();
void ShutdownScenes();
// How dark the fade is now, 0..1 (0: no fade).
float SceneFade();

} // namespace BitMotor::Detail
