// The debug panel, with Dear ImGui on SDL3 and SDL_GPU.
#include "Render/DebugPanelInternal.h"

#include "BitMotor/Core/Log.h"
#include "Render/RendererInternal.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

namespace BitMotor::Detail {

namespace {

bool Started = false;
bool Open = false;
bool Built = false; // this frame has a panel to draw

bool IsMouse(Uint32 type) {
    return type == SDL_EVENT_MOUSE_MOTION || type == SDL_EVENT_MOUSE_BUTTON_DOWN || type == SDL_EVENT_MOUSE_BUTTON_UP ||
           type == SDL_EVENT_MOUSE_WHEEL;
}

bool IsKeyboard(Uint32 type) {
    return type == SDL_EVENT_KEY_DOWN || type == SDL_EVENT_KEY_UP || type == SDL_EVENT_TEXT_INPUT;
}

} // namespace

bool StartDebugPanel(SDL_Window* window) {
    if (Started) return true;
    SDL_GPUDevice* device = GpuDevice();
    if (!device || !window) return false;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // the windows start where the game puts them, every time
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForSDLGPU(window);
    ImGui_ImplSDLGPU3_InitInfo info;
    info.Device = device;
    info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(device, window);
    info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
    if (!ImGui_ImplSDLGPU3_Init(&info)) {
        Log::Warn("The debug panel cannot start");
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        return false;
    }
    Started = true;
    return true;
}

void StopDebugPanel() {
    if (!Started) return;
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    Started = false;
    Built = false;
}

bool DebugPanelEvent(const SDL_Event& event) {
    if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_F1 && !event.key.repeat)
        SetDebugPanelOpen(!Open);
    if (!Started || !Open) return false;
    ImGui_ImplSDL3_ProcessEvent(&event);
    const ImGuiIO& io = ImGui::GetIO();
    return (IsMouse(event.type) && io.WantCaptureMouse) || (IsKeyboard(event.type) && io.WantCaptureKeyboard);
}

void BuildDebugPanel(const std::function<void()>& fill) {
    Built = false;
    if (!Started || !Open) return;
    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    fill();
    ImGui::Render();
    Built = true;
}

void RenderDebugPanel(SDL_GPUCommandBuffer* commands, SDL_GPUTexture* swapchain) {
    if (!Built || !swapchain) return;
    Built = false;
    ImDrawData* data = ImGui::GetDrawData();
    if (!data || data->DisplaySize.x <= 0 || data->DisplaySize.y <= 0) return;
    ImGui_ImplSDLGPU3_PrepareDrawData(data, commands); // its uploads, before the pass
    SDL_GPUColorTargetInfo target{};
    target.texture = swapchain;
    target.load_op = SDL_GPU_LOADOP_LOAD;
    target.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &target, 1, nullptr);
    ImGui_ImplSDLGPU3_RenderDrawData(data, commands, pass);
    SDL_EndGPURenderPass(pass);
}

bool DebugPanelOpen() { return Open; }

void SetDebugPanelOpen(bool open) {
    Open = open;
    if (!Open) Built = false;
}

} // namespace BitMotor::Detail
