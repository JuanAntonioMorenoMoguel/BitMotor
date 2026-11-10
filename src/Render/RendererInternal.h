#pragma once
#include "BitMotor/Colors/Color.h"
#include "BitMotor/Math/Shapes.h"
#include "BitMotor/Render/Texture.h"

#include <cstdint>
#include <filesystem>
#include <vector>

struct SDL_Window;
struct SDL_GPUDevice;
struct SDL_GPUTexture;

// The GPU side of BitMotor, on SDL_GPU (Vulkan on Windows, Linux and the Steam Deck). Each frame the game
// draws into the screen target at the logical resolution, which is then scaled into the window.
namespace BitMotor::Detail {

// Creates the GPU device for the window (false and a log error if there is no usable GPU) and stops it.
// `linear`: Config::LinearColor.
bool StartRenderer(SDL_Window* window, bool vsync, bool linear = false);
void StopRenderer();
// nullptr while there is no renderer.
SDL_GPUDevice* GpuDevice();
void SetRendererVSync(bool vsync);
// Config::LinearColor, and a colour as the shaders want it then (sRGB to linear; unchanged otherwise).
bool LinearColor();
Color ToLinear(Color color);

// Each frame: BeginFrame2D before the game draws, EndFrame2D to render and present it.
void BeginFrame2D();
void EndFrame2D();

// What Screen asks for: they only record the request, which runs in EndFrame2D.
void ClearScreen(Color color);
// Covers the whole logical screen with `color`, blending by its alpha (scene fades), or an area of it
// (logical pixels: scene wipes).
void FillScreen(Color color);
void FillArea(const Rect& area, Color color);
void RequestScreenshot(const std::filesystem::path& path);

// The image behind a Texture. Its pixels wait in memory until there is a GPU device, then go up and are
// freed. GPU images still alive when the renderer stops are released then.
struct TextureData {
    int Width = 0, Height = 0;
    Filter Look = Filter::Nearest;
    std::vector<std::uint8_t> Pixels;
    SDL_GPUTexture* Gpu = nullptr;
    // Its normal map (Texture::SetNormals), for the World layer's lighting. Empty: flat.
    Texture Normals;
    // A RenderTexture's: the GPU image is made empty (cleared to transparent) and drawn into, never uploaded.
    bool Target = false;

    TextureData();
    ~TextureData();
    TextureData(const TextureData&) = delete;
    TextureData& operator=(const TextureData&) = delete;
};

// The texture on the GPU, uploading it first if needed. nullptr without a device.
SDL_GPUTexture* GpuTexture(TextureData& texture);
// The renderer calls these when the device starts (upload what was loaded before) and stops.
void UploadTextures();
void ReleaseTextures();

} // namespace BitMotor::Detail
