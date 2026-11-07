// The frame on the GPU, in passes, all at the logical resolution:
//   1. World sprites -> G-buffer (colour, normal + emission).
//   2. Scene (HDR): clear colour, Background, the World lit (or plain), Glow (added), Overlay.
//   3. Bloom (only when it is on): bright parts at half size, blurred across and down.
//   4. Screen target: the scene + bloom with the screen effects and the flash, then the Hud, then fades.
// The screen target is then scaled into the window with black bars, and optionally saved to a file.
#include "Render/RendererInternal.h"

#include "BitMotor/Colors/Colors.h"
#include "BitMotor/Core/Log.h"
#include "BitMotor/Core/Screen.h"
#include "BitMotor/Core/Time.h"
#include "BitMotor/Render/Light.h"
#include "Core/DisplayInternal.h"
#include "Render/DebugPanelInternal.h"
#include "Render/GpuInternal.h"
#include "Render/ImageInternal.h"
#include "Render/Shaders/Compiled.h"
#include "Render/SpritesInternal.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <initializer_list>

namespace BitMotor::Detail {

namespace {

constexpr SDL_GPUTextureFormat ScreenFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
constexpr SDL_GPUTextureFormat GBufferFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
constexpr SDL_GPUTextureFormat HdrFormat = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;

SDL_GPUDevice* Device = nullptr;
SDL_Window* Window = nullptr;

struct Pipelines {
    SDL_GPUGraphicsPipeline* Fill = nullptr;  // fades, on the screen target
    SDL_GPUGraphicsPipeline* Hud = nullptr;   // sprites on the screen target
    SDL_GPUGraphicsPipeline* World = nullptr; // sprites into the G-buffer
    SDL_GPUGraphicsPipeline* Scene = nullptr; // sprites on the HDR scene (Background, Overlay)
    SDL_GPUGraphicsPipeline* Glow = nullptr;  // additive sprites on the HDR scene
    SDL_GPUGraphicsPipeline* Light = nullptr; // the G-buffer, lit, over the background
    SDL_GPUGraphicsPipeline* BloomExtract = nullptr;
    SDL_GPUGraphicsPipeline* BloomBlur = nullptr;
    SDL_GPUGraphicsPipeline* Composite = nullptr;
    SDL_GPUGraphicsPipeline* HudGlow = nullptr; // additive sprites on an 8-bit target (Frame::Draw2D)

    std::array<SDL_GPUGraphicsPipeline*, 10> All() const {
        return {Fill, Hud, World, Scene, Glow, Light, BloomExtract, BloomBlur, Composite, HudGlow};
    }
} Pipeline;

SDL_GPUSampler* NearestSampler = nullptr;
SDL_GPUSampler* LinearSampler = nullptr;
Texture FlatNormals; // 1x1: facing the viewer, no emission

// A GPU buffer the shaders read, refilled each frame through its own upload buffer.
struct StorageBuffer {
    SDL_GPUBuffer* Buffer = nullptr;
    SDL_GPUTransferBuffer* Upload = nullptr;
    Uint32 Capacity = 0; // bytes

    bool Fill(SDL_GPUCopyPass* copy, const void* data, Uint32 bytes) {
        if (bytes == 0) return true;
        if (bytes > Capacity) {
            Uint32 capacity = std::max<Uint32>(64 * 1024, Capacity);
            while (capacity < bytes) capacity *= 2;
            Release();
            SDL_GPUBufferCreateInfo bufferInfo{};
            bufferInfo.usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
            bufferInfo.size = capacity;
            Buffer = SDL_CreateGPUBuffer(Device, &bufferInfo);
            SDL_GPUTransferBufferCreateInfo uploadInfo{};
            uploadInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
            uploadInfo.size = capacity;
            Upload = SDL_CreateGPUTransferBuffer(Device, &uploadInfo);
            Capacity = Buffer && Upload ? capacity : 0;
            if (!Capacity) {
                Log::Error("Cannot make a GPU buffer of {} bytes: {}", bytes, SDL_GetError());
                Release();
                return false;
            }
        }
        void* mapped = SDL_MapGPUTransferBuffer(Device, Upload, true); // cycle: last frame may still read it
        if (!mapped) return false;
        std::memcpy(mapped, data, bytes);
        SDL_UnmapGPUTransferBuffer(Device, Upload);
        SDL_GPUTransferBufferLocation source{Upload, 0};
        SDL_GPUBufferRegion destination{Buffer, 0, bytes};
        SDL_UploadToGPUBuffer(copy, &source, &destination, true);
        return true;
    }

    void Release() {
        if (Buffer) SDL_ReleaseGPUBuffer(Device, Buffer);
        if (Upload) SDL_ReleaseGPUTransferBuffer(Device, Upload);
        Buffer = nullptr;
        Upload = nullptr;
        Capacity = 0;
    }
};

StorageBuffer SpriteBuffer, LightBuffer;
std::vector<SpriteInstance> Instances;
std::vector<SpriteBatch> Batches;
bool SpritesReady = false, LightsReady = false;

// The render targets, all at the screen's size except the bloom (half).
struct Target {
    SDL_GPUTexture* Texture = nullptr;
    Uint32 Width = 0, Height = 0;
};
Target ScreenTarget, AlbedoTarget, NormalTarget, SceneTarget, BloomA, BloomB;
Uint32 ScreenWidth = 0, ScreenHeight = 0;

// This frame's requests.
Color ClearColor = Colors::Black;
std::vector<Color> Fills;
std::filesystem::path ScreenshotPath;

// What the sprite vertex shader gets per batch (set 1, binding 0).
struct SpriteUniforms {
    float ScreenWidth, ScreenHeight, First, Unused0;
    float TextureWidth, TextureHeight, Unused1, Unused2;
};

SDL_GPUShader* MakeShader(const std::uint32_t* code, std::size_t bytes, SDL_GPUShaderStage stage, Uint32 uniforms,
                          Uint32 samplers = 0, Uint32 storageBuffers = 0) {
    SDL_GPUShaderCreateInfo info{};
    info.code = reinterpret_cast<const Uint8*>(code);
    info.code_size = bytes;
    info.entrypoint = "main";
    info.format = SDL_GPU_SHADERFORMAT_SPIRV;
    info.stage = stage;
    info.num_uniform_buffers = uniforms;
    info.num_samplers = samplers;
    info.num_storage_buffers = storageBuffers;
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

// The colour already multiplied by its alpha (G-buffer and the light pass).
SDL_GPUColorTargetBlendState PremultipliedBlend() {
    SDL_GPUColorTargetBlendState blend = AlphaBlend();
    blend.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    return blend;
}

// Adds light: the colour times its alpha on top of what is there, the target's alpha unchanged.
SDL_GPUColorTargetBlendState AdditiveBlend() {
    SDL_GPUColorTargetBlendState blend = AlphaBlend();
    blend.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    blend.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
    blend.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    return blend;
}

SDL_GPUColorTargetBlendState NoBlend() { return {}; }

SDL_GPUGraphicsPipeline* MakePipeline(SDL_GPUShader* vertex, SDL_GPUShader* fragment,
                                      std::initializer_list<SDL_GPUTextureFormat> formats,
                                      SDL_GPUColorTargetBlendState blend) {
    if (!vertex || !fragment) return nullptr;
    SDL_GPUColorTargetDescription targets[4]{};
    Uint32 count = 0;
    for (SDL_GPUTextureFormat format : formats) {
        targets[count].format = format;
        targets[count].blend_state = blend;
        ++count;
    }
    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = vertex;
    info.fragment_shader = fragment;
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    info.target_info.color_target_descriptions = targets;
    info.target_info.num_color_targets = count;
    SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(Device, &info);
    if (!pipeline) Log::Error("SDL_CreateGPUGraphicsPipeline: {}", SDL_GetError());
    return pipeline;
}

SDL_GPUSampler* MakeSampler(SDL_GPUFilter filter) {
    SDL_GPUSamplerCreateInfo info{};
    info.min_filter = filter;
    info.mag_filter = filter;
    info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    SDL_GPUSampler* sampler = SDL_CreateGPUSampler(Device, &info);
    if (!sampler) Log::Error("SDL_CreateGPUSampler: {}", SDL_GetError());
    return sampler;
}

#define BITMOTOR_SHADER(name, stage, ...)                                                                              \
    MakeShader(Shaders::name, sizeof(Shaders::name), SDL_GPU_SHADERSTAGE_##stage, __VA_ARGS__)

bool CreatePipelines() {
    SDL_GPUShader* fullscreen = BITMOTOR_SHADER(FullscreenVert, VERTEX, 0);
    SDL_GPUShader* fill = BITMOTOR_SHADER(FillFrag, FRAGMENT, 1);
    SDL_GPUShader* spriteVertex = BITMOTOR_SHADER(SpriteVert, VERTEX, 1, 0, 1);
    SDL_GPUShader* sprite = BITMOTOR_SHADER(SpriteFrag, FRAGMENT, 0, 1);
    SDL_GPUShader* world = BITMOTOR_SHADER(SpriteWorldFrag, FRAGMENT, 0, 2);
    SDL_GPUShader* light = BITMOTOR_SHADER(LightFrag, FRAGMENT, 1, 2, 1);
    SDL_GPUShader* extract = BITMOTOR_SHADER(BloomExtractFrag, FRAGMENT, 1, 1);
    SDL_GPUShader* blur = BITMOTOR_SHADER(BloomBlurFrag, FRAGMENT, 1, 1);
    SDL_GPUShader* composite = BITMOTOR_SHADER(CompositeFrag, FRAGMENT, 1, 2);

    Pipeline.Fill = MakePipeline(fullscreen, fill, {ScreenFormat}, AlphaBlend());
    Pipeline.Hud = MakePipeline(spriteVertex, sprite, {ScreenFormat}, AlphaBlend());
    Pipeline.World = MakePipeline(spriteVertex, world, {GBufferFormat, GBufferFormat}, PremultipliedBlend());
    Pipeline.Scene = MakePipeline(spriteVertex, sprite, {HdrFormat}, AlphaBlend());
    Pipeline.Glow = MakePipeline(spriteVertex, sprite, {HdrFormat}, AdditiveBlend());
    Pipeline.Light = MakePipeline(fullscreen, light, {HdrFormat}, PremultipliedBlend());
    Pipeline.BloomExtract = MakePipeline(fullscreen, extract, {HdrFormat}, NoBlend());
    Pipeline.BloomBlur = MakePipeline(fullscreen, blur, {HdrFormat}, NoBlend());
    Pipeline.Composite = MakePipeline(fullscreen, composite, {ScreenFormat}, NoBlend());
    Pipeline.HudGlow = MakePipeline(spriteVertex, sprite, {ScreenFormat}, AdditiveBlend());

    for (SDL_GPUShader* shader : {fullscreen, fill, spriteVertex, sprite, world, light, extract, blur, composite})
        if (shader) SDL_ReleaseGPUShader(Device, shader);
    NearestSampler = MakeSampler(SDL_GPU_FILTER_NEAREST);
    LinearSampler = MakeSampler(SDL_GPU_FILTER_LINEAR);
    const std::uint8_t flat[4] = {128, 128, 255, 0};
    FlatNormals = Texture::FromPixels(1, 1, flat);

    for (SDL_GPUGraphicsPipeline* pipeline : Pipeline.All())
        if (!pipeline) return false;
    return NearestSampler && LinearSampler && FlatNormals;
}

SDL_GPUSampler* SamplerFor(const Texture& texture) { return Detail::SamplerFor(texture.GetFilter()); }

// Recreates a target when the screen changes size.
bool Prepare(Target& target, SDL_GPUTextureFormat format, Uint32 width, Uint32 height) {
    if (target.Texture && target.Width == width && target.Height == height) return true;
    if (target.Texture) SDL_ReleaseGPUTexture(Device, target.Texture);
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = format;
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = width;
    info.height = height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    target.Texture = SDL_CreateGPUTexture(Device, &info);
    target.Width = target.Texture ? width : 0;
    target.Height = target.Texture ? height : 0;
    if (!target.Texture) Log::Error("Cannot create a {}x{} render target: {}", width, height, SDL_GetError());
    return target.Texture != nullptr;
}

void Release(Target& target) {
    if (target.Texture) SDL_ReleaseGPUTexture(Device, target.Texture);
    target = {};
}

// The targets follow Screen::Size (the window's size when there is no logical resolution); with a
// CustomRenderer, the screen follows Screen::PixelSize.
bool PrepareTargets() {
    Vec2 size = ActiveRenderer() ? Screen::PixelSize() : Screen::Size();
    Uint32 width = Uint32(std::lround(size.X)), height = Uint32(std::lround(size.Y));
    if (width == 0 || height == 0) return false; // minimised
    Uint32 halfWidth = std::max<Uint32>(1, width / 2), halfHeight = std::max<Uint32>(1, height / 2);
    if (ActiveRenderer()) { // only the screen: the renderer makes its own targets
        bool ok = Prepare(ScreenTarget, ScreenFormat, width, height);
        ScreenWidth = ok ? width : 0;
        ScreenHeight = ok ? height : 0;
        return ok;
    }
    bool ok = Prepare(ScreenTarget, ScreenFormat, width, height) &&
              Prepare(AlbedoTarget, GBufferFormat, width, height) &&
              Prepare(NormalTarget, GBufferFormat, width, height) && Prepare(SceneTarget, HdrFormat, width, height) &&
              Prepare(BloomA, HdrFormat, halfWidth, halfHeight) && Prepare(BloomB, HdrFormat, halfWidth, halfHeight);
    ScreenWidth = ok ? width : 0;
    ScreenHeight = ok ? height : 0;
    return ok;
}

// Copies this frame's sprites and lights to the GPU, before any pass draws.
void Upload(SDL_GPUCommandBuffer* commands) {
    BuildSprites(SpriteQueue(), Instances, Batches);
    const std::vector<LightInstance>& lights = LightQueue();
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(commands);
    SpritesReady = SpriteBuffer.Fill(copy, Instances.data(), Uint32(Instances.size() * sizeof(SpriteInstance)));
    LightsReady = LightBuffer.Fill(copy, lights.data(), Uint32(lights.size() * sizeof(LightInstance)));
    SDL_EndGPUCopyPass(copy);
    if (!SpritesReady) Instances.clear();
}

bool HasLayer(Layer layer) {
    return std::any_of(Batches.begin(), Batches.end(), [&](const SpriteBatch& b) { return b.Where == layer; });
}

// The batches of some layers, with `pipeline`. World batches also bind their normal maps.
void DrawBatches(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* pass, SDL_GPUGraphicsPipeline* pipeline,
                 std::initializer_list<Layer> layers, SDL_GPUGraphicsPipeline* glow = nullptr) {
    if (Instances.empty() || !SpriteBuffer.Buffer) return;
    SDL_BindGPUVertexStorageBuffers(pass, 0, &SpriteBuffer.Buffer, 1);
    SDL_GPUGraphicsPipeline* bound = nullptr;
    for (const SpriteBatch& batch : Batches) {
        if (std::find(layers.begin(), layers.end(), batch.Where) == layers.end()) continue;
        SDL_GPUTexture* texture = GpuTexture(*batch.Source.Internal());
        if (!texture) continue;
        SDL_GPUGraphicsPipeline* use = batch.Where == Layer::Glow && glow ? glow : pipeline;
        if (use != bound) {
            SDL_BindGPUGraphicsPipeline(pass, use);
            bound = use;
        }
        SDL_GPUTextureSamplerBinding bindings[2] = {{texture, SamplerFor(batch.Source)}, {}};
        Uint32 count = 1;
        if (batch.Where == Layer::World && use == Pipeline.World) {
            Texture normals = batch.Source.Normals() ? batch.Source.Normals() : FlatNormals;
            SDL_GPUTexture* normalTexture = GpuTexture(*normals.Internal());
            if (!normalTexture) normalTexture = GpuTexture(*FlatNormals.Internal());
            bindings[1] = {normalTexture, SamplerFor(normals)};
            count = 2;
        }
        SDL_BindGPUFragmentSamplers(pass, 0, bindings, count);
        const Vec2 logical = Screen::Size(); // sprites are placed in logical pixels, whatever the target's size
        SpriteUniforms uniforms{logical.X,
                                logical.Y,
                                float(batch.First),
                                0,
                                float(batch.Source.Width()),
                                float(batch.Source.Height()),
                                0,
                                0};
        SDL_PushGPUVertexUniformData(commands, 0, &uniforms, sizeof(uniforms));
        SDL_DrawGPUPrimitives(pass, 6, batch.Count, 0, 0);
    }
}

SDL_GPURenderPass* BeginPass(SDL_GPUCommandBuffer* commands, std::initializer_list<SDL_GPUTexture*> textures,
                             SDL_GPULoadOp load, Color clear = Colors::Transparent) {
    SDL_GPUColorTargetInfo targets[4]{};
    Uint32 count = 0;
    for (SDL_GPUTexture* texture : textures) {
        targets[count].texture = texture;
        targets[count].clear_color = {clear.R, clear.G, clear.B, clear.A};
        targets[count].load_op = load;
        targets[count].store_op = SDL_GPU_STOREOP_STORE;
        ++count;
    }
    return SDL_BeginGPURenderPass(commands, targets, count, nullptr);
}

void Fullscreen(SDL_GPURenderPass* pass, SDL_GPUGraphicsPipeline* pipeline,
                std::initializer_list<SDL_GPUTextureSamplerBinding> samplers) {
    SDL_BindGPUGraphicsPipeline(pass, pipeline);
    if (samplers.size()) SDL_BindGPUFragmentSamplers(pass, 0, samplers.begin(), Uint32(samplers.size()));
    SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
}

void DrawScene(SDL_GPUCommandBuffer* commands) {
    // 1. The World into the G-buffer.
    const bool world = HasLayer(Layer::World);
    if (world) {
        SDL_GPURenderPass* pass =
            BeginPass(commands, {AlbedoTarget.Texture, NormalTarget.Texture}, SDL_GPU_LOADOP_CLEAR);
        DrawBatches(commands, pass, Pipeline.World, {Layer::World});
        SDL_EndGPURenderPass(pass);
    }

    // 2. The scene: background, the lit world, glow and overlay.
    SDL_GPURenderPass* pass = BeginPass(commands, {SceneTarget.Texture}, SDL_GPU_LOADOP_CLEAR, ClearColor);
    DrawBatches(commands, pass, Pipeline.Scene, {Layer::Background});
    if (world) {
        const std::vector<LightInstance>& lights = LightQueue();
        float count = Lighting::Enabled() && LightsReady && LightBuffer.Buffer ? float(lights.size()) : 0.0f;
        Color ambient = Lighting::Ambient();
        float params[8] = {ambient.R,
                           ambient.G,
                           ambient.B,
                           count,
                           Lighting::Enabled() ? 1.0f : 0.0f,
                           Lighting::Stepped() ? 1.0f : 0.0f,
                           0,
                           0};
        SDL_BindGPUGraphicsPipeline(pass, Pipeline.Light);
        SDL_GPUTextureSamplerBinding gbuffer[2] = {{AlbedoTarget.Texture, NearestSampler},
                                                   {NormalTarget.Texture, NearestSampler}};
        SDL_BindGPUFragmentSamplers(pass, 0, gbuffer, 2);
        // A storage buffer must be bound even with no lights: any one will do, the count says 0.
        SDL_GPUBuffer* lightBuffer = LightBuffer.Buffer ? LightBuffer.Buffer : SpriteBuffer.Buffer;
        SDL_BindGPUFragmentStorageBuffers(pass, 0, &lightBuffer, 1);
        SDL_PushGPUFragmentUniformData(commands, 0, params, sizeof(params));
        SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
    }
    DrawBatches(commands, pass, Pipeline.Scene, {Layer::Glow, Layer::Overlay}, Pipeline.Glow);
    SDL_EndGPURenderPass(pass);

    // 3. Bloom.
    if (Lighting::Bloom() > 0) {
        float threshold[4] = {Lighting::BloomThreshold(), 0, 0, 0};
        pass = BeginPass(commands, {BloomA.Texture}, SDL_GPU_LOADOP_DONT_CARE);
        SDL_PushGPUFragmentUniformData(commands, 0, threshold, sizeof(threshold));
        Fullscreen(pass, Pipeline.BloomExtract, {{SceneTarget.Texture, LinearSampler}});
        SDL_EndGPURenderPass(pass);
        float across[4] = {1.0f / float(BloomA.Width), 0, 0, 0}, down[4] = {0, 1.0f / float(BloomA.Height), 0, 0};
        pass = BeginPass(commands, {BloomB.Texture}, SDL_GPU_LOADOP_DONT_CARE);
        SDL_PushGPUFragmentUniformData(commands, 0, across, sizeof(across));
        Fullscreen(pass, Pipeline.BloomBlur, {{BloomA.Texture, LinearSampler}});
        SDL_EndGPURenderPass(pass);
        pass = BeginPass(commands, {BloomA.Texture}, SDL_GPU_LOADOP_DONT_CARE);
        SDL_PushGPUFragmentUniformData(commands, 0, down, sizeof(down));
        Fullscreen(pass, Pipeline.BloomBlur, {{BloomB.Texture, LinearSampler}});
        SDL_EndGPURenderPass(pass);
    }

    // 4. The screen: composite with the effects and the flash, the Hud, the fades.
    const ScreenEffects& effects = Screen::Effects();
    Color flash = FlashColor();
    float composite[16] = {Lighting::Bloom(),
                           Lighting::Enabled() ? 1.0f : 0.0f,
                           float(Time::Total()),
                           0,
                           effects.Vignette,
                           effects.Grain,
                           effects.Aberration,
                           effects.Saturation,
                           flash.R,
                           flash.G,
                           flash.B,
                           FlashAmount(),
                           float(ScreenWidth),
                           float(ScreenHeight),
                           0,
                           0};
    pass = BeginPass(commands, {ScreenTarget.Texture}, SDL_GPU_LOADOP_DONT_CARE);
    SDL_PushGPUFragmentUniformData(commands, 0, composite, sizeof(composite));
    Fullscreen(pass, Pipeline.Composite, {{SceneTarget.Texture, NearestSampler}, {BloomA.Texture, LinearSampler}});
    DrawBatches(commands, pass, Pipeline.Hud, {Layer::Hud});
    if (!Fills.empty()) {
        SDL_BindGPUGraphicsPipeline(pass, Pipeline.Fill);
        for (const Color& fill : Fills) {
            SDL_PushGPUFragmentUniformData(commands, 0, &fill, sizeof(Color));
            SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
        }
    }
    SDL_EndGPURenderPass(pass);
}

// A CustomRenderer's frame into the screen target, then the scene fades on top.
void DrawCustom(SDL_GPUCommandBuffer* commands) {
    static TargetData screen;
    screen.External = true;
    screen.Native = ScreenTarget.Texture;
    screen.Width = int(ScreenWidth);
    screen.Height = int(ScreenHeight);
    screen.Format = TargetFormat::Rgba8;
    screen.Look = Filter::Nearest;
    UpdateGlobals(Time::RealDelta());
    RenderCustom(commands, screen);
    if (Fills.empty()) return;
    SDL_GPURenderPass* pass = BeginPass(commands, {ScreenTarget.Texture}, SDL_GPU_LOADOP_LOAD);
    SDL_BindGPUGraphicsPipeline(pass, Pipeline.Fill);
    for (const Color& fill : Fills) {
        SDL_PushGPUFragmentUniformData(commands, 0, &fill, sizeof(Color));
        SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
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
    blit.source.texture = ScreenTarget.Texture;
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
    RenderDebugPanel(commands, swapchain);
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
    source.texture = ScreenTarget.Texture;
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
    FlatNormals = {};
    ReleaseTextures();
    ReleaseGpuObjects();
    for (Target* target : {&ScreenTarget, &AlbedoTarget, &NormalTarget, &SceneTarget, &BloomA, &BloomB})
        Release(*target);
    for (SDL_GPUGraphicsPipeline* pipeline : Pipeline.All())
        if (pipeline) SDL_ReleaseGPUGraphicsPipeline(Device, pipeline);
    Pipeline = {};
    for (SDL_GPUSampler* sampler : {NearestSampler, LinearSampler})
        if (sampler) SDL_ReleaseGPUSampler(Device, sampler);
    NearestSampler = LinearSampler = nullptr;
    SpriteBuffer.Release();
    LightBuffer.Release();
    Instances.clear();
    Batches.clear();
    ResetSprites();
    ScreenWidth = ScreenHeight = 0;
    if (Window) SDL_ReleaseWindowFromGPUDevice(Device, Window);
    SDL_DestroyGPUDevice(Device);
    Device = nullptr;
    Window = nullptr;
    Fills.clear();
    ScreenshotPath.clear();
}

SDL_GPUDevice* GpuDevice() { return Device; }

SDL_GPUSampler* SamplerFor(Filter filter) { return filter == Filter::Linear ? LinearSampler : NearestSampler; }

void DrawSprites2D(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* pass, bool hdr) {
    if (hdr)
        DrawBatches(commands, pass, Pipeline.Scene,
                    {Layer::Background, Layer::World, Layer::Glow, Layer::Overlay, Layer::Hud}, Pipeline.Glow);
    else
        DrawBatches(commands, pass, Pipeline.Hud,
                    {Layer::Background, Layer::World, Layer::Glow, Layer::Overlay, Layer::Hud}, Pipeline.HudGlow);
}

void SetRendererVSync(bool vsync) {
    if (Device) ApplyVSync(vsync);
}

void BeginFrame2D() {
    ClearColor = Colors::Black;
    Fills.clear();
    ResetSprites();
}

void EndFrame2D() {
    AdvanceFlash(Time::RealDelta());
    if (!Device) return;
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(Device);
    if (!commands) {
        Log::Error("SDL_AcquireGPUCommandBuffer: {}", SDL_GetError());
        return;
    }
    SDL_GPUTransferBuffer* download = nullptr;
    if (PrepareTargets()) {
        Upload(commands);
        if (ActiveRenderer())
            DrawCustom(commands);
        else
            DrawScene(commands);
        if (!ScreenshotPath.empty()) download = StartDownload(commands);
        Present(commands);
    }
    SpriteQueue().clear(); // the instances are on the GPU now; drop the texture references
    Batches.clear();
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
    SpriteQueue().clear();
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
