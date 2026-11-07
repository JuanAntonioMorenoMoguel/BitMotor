#pragma once
#include <functional>

union SDL_Event;
struct SDL_Window;
struct SDL_GPUCommandBuffer;
struct SDL_GPUTexture;

// The debug panel: Dear ImGui over the game, opened with F1 (or --debug). The game fills it in
// Game::DebugPanel and Scene::DebugPanel with ImGui calls.
namespace BitMotor::Detail {

bool StartDebugPanel(SDL_Window* window);
void StopDebugPanel();
// Every SDL event goes here first: true if the panel takes it (the game should not see it).
bool DebugPanelEvent(const SDL_Event& event);
// Once per frame after the game's Draw: builds the panel with `fill` (only while it is open).
void BuildDebugPanel(const std::function<void()>& fill);
// Draws it on the window's swapchain texture, over the game.
void RenderDebugPanel(SDL_GPUCommandBuffer* commands, SDL_GPUTexture* swapchain);
bool DebugPanelOpen();
void SetDebugPanelOpen(bool open);

} // namespace BitMotor::Detail
