#pragma once
#include "BitMotor/Render/CustomRenderer.h"
#include "BitMotor/Render/Gpu.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

namespace BitMotor::Detail {

struct ShaderData {
    std::vector<std::uint8_t> Code;
    ShaderStage Stage = ShaderStage::Vertex;
    ShaderResources Resources;
    bool Builtin = false; // BitMotor's fullscreen vertex shader
    // Goes up each time the code is reloaded, so the pipelines that use it are remade.
    int Version = 0;
};

struct PipelineData {
    SDL_GPUGraphicsPipeline* Native = nullptr;
    PipelineInfo Info;
    int VertexVersion = 0, FragmentVersion = 0; // of the shaders it was made with
    bool UsesVertices = false;
    PipelineData();
    ~PipelineData();
};

struct ComputePipelineData {
    SDL_GPUComputePipeline* Native = nullptr;
    Shader Source;
    int Version = 0;
    ComputePipelineData();
    ~ComputePipelineData();
};

// Remakes a pipeline whose shaders were reloaded (Debug). Keeps the old one if the new code fails.
void RefreshPipeline(PipelineData& pipeline);
void RefreshPipeline(ComputePipelineData& pipeline);
// Shader::Load in Debug: watches the source of a .spv made by BitMotor_Shaders and recompiles it on save.
void WatchShaderSource(const std::shared_ptr<ShaderData>& shader, const std::filesystem::path& spv);

struct BufferData {
    SDL_GPUBuffer* Native = nullptr;
    BufferUsage Usage = BufferUsage::Storage;
    std::size_t Bytes = 0;
    // Uploads waiting for the next pass, in order.
    struct Upload {
        std::size_t Offset;
        std::vector<std::uint8_t> Bytes;
    };
    std::vector<Upload> Pending;
    BufferData();
    ~BufferData();
};

struct TargetData {
    SDL_GPUTexture* Native = nullptr;
    int Width = 0, Height = 0;
    TargetFormat Format = TargetFormat::Rgba8;
    BitMotor::Filter Look = Filter::Linear;
    // > 0: that many times the screen's size, following it. 0: fixed.
    float Scale = 0;
    int FixedWidth = 0, FixedHeight = 0;
    // The engine's screen target, which it owns.
    bool External = false;
    TargetData();
    ~TargetData();
};

struct TargetAccess {
    static void Wrap(Target& target, TargetData& data);
};

// Prepares targets that follow the screen, and creates the ones made before the GPU existed. false if the
// target cannot be used now.
bool PrepareTarget(TargetData& target);
// The uploads waiting in every buffer, in a copy pass (outside any render pass).
void FlushUploads(SDL_GPUCommandBuffer* commands);
// Computes this frame's Gpu::Globals.
void UpdateGlobals(double realDt);
// Pushes them into uniform block 0 of both stages.
void PushGlobals(SDL_GPUCommandBuffer* commands);
// The GPU stopped: every pipeline, buffer and target lets its native object go.
void ReleaseGpuObjects();

CustomRenderer* ActiveRenderer();
// Called by the game loop around the renderer's life: after the GPU starts (Init and the first Resize), and
// before the services stop (Shutdown).
void InitCustomRenderer();
void ShutdownCustomRenderer();
// The renderer's frame into the screen target (the engine then shows it). `screen` wraps that target.
void RenderCustom(SDL_GPUCommandBuffer* commands, TargetData& screen);

// From the 2D renderer (Renderer.cpp), for Frame::Draw2D and the samplers.
void DrawSprites2D(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* pass, bool hdr);
SDL_GPUSampler* SamplerFor(BitMotor::Filter filter);
SDL_GPUTextureFormat NativeFormat(TargetFormat format);

} // namespace BitMotor::Detail
