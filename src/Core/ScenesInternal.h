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
// How much of the screen the transition covers now, 0..1 (0: none; a Fade's darkness, a Wipe's share).
float SceneFade();
// A Slide moving now: how far it has gone (0..1, eased) and towards which side (Transition::Side).
// The renderer draws the new frame and the held one side by side with it.
bool SceneSlide(float& progress, int& towards);
// The frame before a Slide was drawn: the renderer keeps a copy of it (the old scene) at the end of it.
bool SceneSnapshotWanted();

} // namespace BitMotor::Detail
