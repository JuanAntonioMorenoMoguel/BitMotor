// The frame on the GPU: the game draws into the screen target (logical resolution), which is then scaled
// into the window with black bars, and optionally saved to a file.
#include "Render/RendererInternal.h"

#include "BitMotor/Colors/Colors.h"
#include "BitMotor/Core/Log.h"
#include "BitMotor/Core/Screen.h"
#include "Core/DisplayInternal.h"
#include "Render/ImageInternal.h"
#include "Render/Shaders/Compiled.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace BitMotor::Detail {

namespace {

constexpr SDL_GPUTextureFormat ScreenFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

SDL_GPUDevice* Device = nullptr;
SDL_Window* Window = nullptr;
SDL_GPUGraphicsPipeline* FillPipeline = nullptr;
SDL_GPUTexture* ScreenTarget = nullptr;
Uint32 ScreenWidth = 0, ScreenHeight = 0;

// This frame's requests.
Color ClearColor = Colors::Black;
std::vector<Color> Fills;
std::filesystem::path ScreenshotPath;

SDL_GPUShader* MakeShader(const std::uint32_t* code, std::size_t bytes, SDL_GPUShaderStage stage, Uint32 uniforms) {
    SDL_GPUShaderCreateInfo info{};
    info.code = reinterpret_cast<const Uint8*>(code);
    info.code_size = bytes;
    info.entrypoint = "main";
    info.format = SDL_GPU_SHADERFORMAT_SPIRV;
    info.stage = stage;
    info.num_uniform_buffers = uniforms;
    SDL_GPUShader* shader = SDL_CreateGPUShader(Device, &info);
    if (!shader) Log::Error("SDL_CreateGPUShader: {}", SDL_GetError());
    return shader;
}

// Straight alpha over what is already there; the target's alpha adds up the same way.
SDL_GPUColorTargetBlendState AlphaBlend() {
    SDL_GPUColorTargetBlendState blend{};
    blend.enable_blend = true;
    blend.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    blend.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    blend.color_blend_op = SDL_GPU_BLENDOP_ADD;
    blend.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    blend.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    blend.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    return blend;
}

bool CreatePipelines() {
    SDL_GPUShader* vertex =
        MakeShader(Shaders::FullscreenVert, sizeof(Shaders::FullscreenVert), SDL_GPU_SHADERSTAGE_VERTEX, 0);
    SDL_GPUShader* fragment = MakeShader(Shaders::FillFrag, sizeof(Shaders::FillFrag), SDL_GPU_SHADERSTAGE_FRAGMENT, 1);
    if (vertex && fragment) {
        SDL_GPUColorTargetDescription target{};
        target.format = ScreenFormat;
        target.blend_state = AlphaBlend();
        SDL_GPUGraphicsPipelineCreateInfo info{};
        info.vertex_shader = vertex;
        info.fragment_shader = fragment;
        info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        info.target_info.color_target_descriptions = &target;
        info.target_info.num_color_targets = 1;
        FillPipeline = SDL_CreateGPUGraphicsPipeline(Device, &info);
        if (!FillPipeline) Log::Error("SDL_CreateGPUGraphicsPipeline: {}", SDL_GetError());
    }
    if (vertex) SDL_ReleaseGPUShader(Device, vertex);
    if (fragment) SDL_ReleaseGPUShader(Device, fragment);
    return FillPipeline != nullptr;
}

// The screen target follows Screen::Size (the window's size when there is no logical resolution).
bool PrepareScreenTarget() {
    Vec2 size = Screen::Size();
    Uint32 width = Uint32(std::lround(size.X)), height = Uint32(std::lround(size.Y));
    if (width == 0 || height == 0) return false; // minimised
    if (ScreenTarget && width == ScreenWidth && height == ScreenHeight) return true;
    if (ScreenTarget) SDL_ReleaseGPUTexture(Device, ScreenTarget);
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = ScreenFormat;
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = width;
    info.height = height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    ScreenTarget = SDL_CreateGPUTexture(Device, &info);
    ScreenWidth = ScreenTarget ? width : 0;
    ScreenHeight = ScreenTarget ? height : 0;
    if (!ScreenTarget) Log::Error("Cannot create the screen target ({}x{}): {}", width, height, SDL_GetError());
    return ScreenTarget != nullptr;
}

void DrawScreen(SDL_GPUCommandBuffer* commands) {
    SDL_GPUColorTargetInfo target{};
    target.texture = ScreenTarget;
    target.clear_color = {ClearColor.R, ClearColor.G, ClearColor.B, ClearColor.A};
    target.load_op = SDL_GPU_LOADOP_CLEAR;
    target.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &target, 1, nullptr);
    if (!Fills.empty()) {
        SDL_BindGPUGraphicsPipeline(pass, FillPipeline);
        for (const Color& fill : Fills) {
            SDL_PushGPUFragmentUniformData(commands, 0, &fill, sizeof(Color));
            SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
        }
    }
    SDL_EndGPURenderPass(pass);
}

// Scales the screen target into the window, black around it.
void Present(SDL_GPUCommandBuffer* commands) {
    SDL_GPUTexture* swapchain = nullptr;
    Uint32 width = 0, height = 0;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(commands, Window, &swapchain, &width, &height)) {
        Log::Error("SDL_WaitAndAcquireGPUSwapchainTexture: {}", SDL_GetError());
        return;
    }
    if (!swapchain) return; // minimised or not ready: nothing to show this frame
    Rect view = ViewportFor({float(width), float(height)});
    SDL_GPUBlitInfo blit{};
    blit.source.texture = ScreenTarget;
    blit.source.w = ScreenWidth;
    blit.source.h = ScreenHeight;
    blit.destination.texture = swapchain;
    blit.destination.x = Uint32(std::max(0L, std::lround(view.X)));
    blit.destination.y = Uint32(std::max(0L, std::lround(view.Y)));
    blit.destination.w = Uint32(std::max(1L, std::lround(view.Width)));
    blit.destination.h = Uint32(std::max(1L, std::lround(view.Height)));
    blit.load_op = SDL_GPU_LOADOP_CLEAR;
    blit.clear_color = {0, 0, 0, 1};
    blit.filter = SDL_GPU_FILTER_NEAREST;
    SDL_BlitGPUTexture(commands, &blit);
}

SDL_GPUTransferBuffer* StartDownload(SDL_GPUCommandBuffer* commands) {
    SDL_GPUTransferBufferCreateInfo info{};
    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    info.size = ScreenWidth * ScreenHeight * 4;
    SDL_GPUTransferBuffer* buffer = SDL_CreateGPUTransferBuffer(Device, &info);
    if (!buffer) {
        Log::Error("Screenshot: {}", SDL_GetError());
        return nullptr;
    }
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(commands);
    SDL_GPUTextureRegion source{};
    source.texture = ScreenTarget;
    source.w = ScreenWidth;
    source.h = ScreenHeight;
    source.d = 1;
    SDL_GPUTextureTransferInfo destination{};
    destination.transfer_buffer = buffer;
    destination.pixels_per_row = ScreenWidth;
    destination.rows_per_layer = ScreenHeight;
    SDL_DownloadFromGPUTexture(copy, &source, &destination);
    SDL_EndGPUCopyPass(copy);
    return buffer;
}

void FinishDownload(SDL_GPUTransferBuffer* buffer, const std::filesystem::path& path) {
    Image image;
    image.Width = int(ScreenWidth);
    image.Height = int(ScreenHeight);
    image.Pixels.resize(std::size_t(ScreenWidth) * ScreenHeight * 4);
    if (void* mapped = SDL_MapGPUTransferBuffer(Device, buffer, false)) {
        std::memcpy(image.Pixels.data(), mapped, image.Pixels.size());
        SDL_UnmapGPUTransferBuffer(Device, buffer);
        for (std::size_t i = 3; i < image.Pixels.size(); i += 4) image.Pixels[i] = 255; // the screen is opaque
        if (SaveImage(image, path)) Log::Info("Screenshot: {}", path.string());
    } else {
        Log::Error("Screenshot: {}", SDL_GetError());
    }
    SDL_ReleaseGPUTransferBuffer(Device, buffer);
}

bool ApplyVSync(bool vsync) {
    SDL_GPUPresentMode mode = SDL_GPU_PRESENTMODE_VSYNC;
    if (!vsync) {
        if (SDL_WindowSupportsGPUPresentMode(Device, Window, SDL_GPU_PRESENTMODE_IMMEDIATE))
            mode = SDL_GPU_PRESENTMODE_IMMEDIATE;
        else if (SDL_WindowSupportsGPUPresentMode(Device, Window, SDL_GPU_PRESENTMODE_MAILBOX))
            mode = SDL_GPU_PRESENTMODE_MAILBOX;
    }
    if (!SDL_SetGPUSwapchainParameters(Device, Window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, mode)) {
        Log::Warn("SDL_SetGPUSwapchainParameters: {}", SDL_GetError());
        return false;
    }
    return true;
}

} // namespace

bool StartRenderer(SDL_Window* window, bool vsync) {
#ifdef NDEBUG
    constexpr bool debug = false;
#else
    constexpr bool debug = true; // validation messages when the Vulkan SDK is installed
#endif
    Device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, debug, nullptr);
    if (!Device) {
        Log::Error("No usable GPU (BitMotor needs Vulkan): {}", SDL_GetError());
        return false;
    }
    if (!SDL_ClaimWindowForGPUDevice(Device, window)) {
        Log::Error("SDL_ClaimWindowForGPUDevice: {}", SDL_GetError());
        SDL_DestroyGPUDevice(Device);
        Device = nullptr;
        return false;
    }
    Window = window;
    Log::Info("GPU: {}", SDL_GetGPUDeviceDriver(Device));
    ApplyVSync(vsync);
    if (!CreatePipelines()) {
        StopRenderer();
        return false;
    }
    UploadTextures(); // the ones loaded before the window opened
    return true;
}

void StopRenderer() {
    if (!Device) return;
    SDL_WaitForGPUIdle(Device);
    ReleaseTextures();
    if (ScreenTarget) SDL_ReleaseGPUTexture(Device, ScreenTarget);
    if (FillPipeline) SDL_ReleaseGPUGraphicsPipeline(Device, FillPipeline);
    ScreenTarget = nullptr;
    FillPipeline = nullptr;
    ScreenWidth = ScreenHeight = 0;
    if (Window) SDL_ReleaseWindowFromGPUDevice(Device, Window);
    SDL_DestroyGPUDevice(Device);
    Device = nullptr;
    Window = nullptr;
    Fills.clear();
    ScreenshotPath.clear();
}

SDL_GPUDevice* GpuDevice() { return Device; }

void SetRendererVSync(bool vsync) {
    if (Device) ApplyVSync(vsync);
}

void BeginFrame2D() {
    ClearColor = Colors::Black;
    Fills.clear();
}

void EndFrame2D() {
    if (!Device) return;
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(Device);
    if (!commands) {
        Log::Error("SDL_AcquireGPUCommandBuffer: {}", SDL_GetError());
        return;
    }
    SDL_GPUTransferBuffer* download = nullptr;
    if (PrepareScreenTarget()) {
        DrawScreen(commands);
        if (!ScreenshotPath.empty()) download = StartDownload(commands);
        Present(commands);
    }
    if (!download) {
        SDL_SubmitGPUCommandBuffer(commands);
        return;
    }
    // A screenshot waits for this frame to finish on the GPU (only on the frames that take one).
    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
    if (fence) {
        SDL_WaitForGPUFences(Device, true, &fence, 1);
        SDL_ReleaseGPUFence(Device, fence);
        FinishDownload(download, ScreenshotPath);
    } else {
        Log::Error("Screenshot: {}", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(Device, download);
    }
    ScreenshotPath.clear();
}

void ClearScreen(Color color) {
    // Covers everything drawn before it, so what was queued is simply dropped.
    ClearColor = color;
    Fills.clear();
}

void FillScreen(Color color) {
    if (color.A > 0) Fills.push_back(color);
}

void RequestScreenshot(const std::filesystem::path& path) {
    if (!Device) {
        Log::Warn("Screen::Screenshot: there is no window yet");
        return;
    }
    ScreenshotPath = path;
}

} // namespace BitMotor::Detail
