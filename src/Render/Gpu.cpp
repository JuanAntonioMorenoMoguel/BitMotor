// Gpu: shaders, pipelines, buffers and targets for CustomRenderers, on SDL_GPU, and the Frame they draw with.
#include "Render/GpuInternal.h"

#include "BitMotor/Core/Files.h"
#include "BitMotor/Core/Input.h"
#include "BitMotor/Core/Log.h"
#include "BitMotor/Core/Screen.h"
#include "BitMotor/Core/Time.h"
#include "Render/ImageInternal.h"
#include "Render/RendererInternal.h"
#include "Render/Shaders/Compiled.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace BitMotor {

namespace Detail {

namespace {

// Every live object, so that they can let their native objects go when the GPU stops.
std::vector<PipelineData*> Pipelines;
std::vector<BufferData*> Buffers;
std::vector<TargetData*> Targets;

template <class T> void Forget(std::vector<T*>& list, T* item) {
    list.erase(std::remove(list.begin(), list.end(), item), list.end());
}

Gpu::Globals Current;
std::array<Vec4, 13> Values{};
double RealTime = 0;

CustomRenderer* Renderer = nullptr;
int LastWidth = 0, LastHeight = 0;

SDL_GPUBufferUsageFlags NativeUsage(BufferUsage usage) {
    switch (usage) {
    case BufferUsage::Vertex:
        return SDL_GPU_BUFFERUSAGE_VERTEX;
    case BufferUsage::Index:
        return SDL_GPU_BUFFERUSAGE_INDEX;
    default:
        return SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
    }
}

bool CreateNative(BufferData& buffer) {
    if (buffer.Native) return true;
    SDL_GPUDevice* device = GpuDevice();
    if (!device || buffer.Bytes == 0) return false;
    SDL_GPUBufferCreateInfo info{};
    info.usage = NativeUsage(buffer.Usage);
    info.size = Uint32(buffer.Bytes);
    buffer.Native = SDL_CreateGPUBuffer(device, &info);
    if (!buffer.Native) Log::Error("Cannot make a GPU buffer of {} bytes: {}", buffer.Bytes, SDL_GetError());
    return buffer.Native != nullptr;
}

void ReleaseNative(BufferData& buffer) {
    if (buffer.Native && GpuDevice()) SDL_ReleaseGPUBuffer(GpuDevice(), buffer.Native);
    buffer.Native = nullptr;
}

void ReleaseNative(TargetData& target) {
    if (!target.External && target.Native && GpuDevice()) SDL_ReleaseGPUTexture(GpuDevice(), target.Native);
    target.Native = nullptr;
    if (!target.External) target.Width = target.Height = 0;
}

SDL_GPUShader* MakeNative(const ShaderData& shader) {
    SDL_GPUShaderCreateInfo info{};
    if (shader.Builtin) {
        info.code = reinterpret_cast<const Uint8*>(Shaders::FullscreenVert);
        info.code_size = sizeof(Shaders::FullscreenVert);
    } else {
        info.code = shader.Code.data();
        info.code_size = shader.Code.size();
    }
    info.entrypoint = "main";
    info.format = SDL_GPU_SHADERFORMAT_SPIRV;
    info.stage = shader.Stage == ShaderStage::Vertex ? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT;
    info.num_uniform_buffers = Uint32(std::max(0, shader.Resources.Uniforms));
    info.num_samplers = Uint32(std::max(0, shader.Resources.Samplers));
    info.num_storage_buffers = Uint32(std::max(0, shader.Resources.StorageBuffers));
    SDL_GPUShader* native = SDL_CreateGPUShader(GpuDevice(), &info);
    if (!native) Log::Error("SDL_CreateGPUShader: {}", SDL_GetError());
    return native;
}

SDL_GPUColorTargetBlendState BlendState(Blend blend) {
    SDL_GPUColorTargetBlendState state{};
    if (blend == Blend::None) return state;
    state.enable_blend = true;
    state.color_blend_op = state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    switch (blend) {
    case Blend::Alpha:
        state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        break;
    case Blend::Premultiplied:
        state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        break;
    case Blend::Additive:
        state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
        state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        break;
    case Blend::Multiply:
        state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_DST_COLOR;
        state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        break;
    default:
        break;
    }
    return state;
}

SDL_GPUPrimitiveType NativeTopology(Topology topology) {
    switch (topology) {
    case Topology::TriangleStrip:
        return SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP;
    case Topology::Lines:
        return SDL_GPU_PRIMITIVETYPE_LINELIST;
    case Topology::LineStrip:
        return SDL_GPU_PRIMITIVETYPE_LINESTRIP;
    case Topology::Points:
        return SDL_GPU_PRIMITIVETYPE_POINTLIST;
    default:
        return SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    }
}

SDL_GPUVertexElementFormat NativeAttribute(VertexAttribute::Format format) {
    switch (format) {
    case VertexAttribute::Format::Float:
        return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT;
    case VertexAttribute::Format::Float3:
        return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    case VertexAttribute::Format::Float4:
        return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    case VertexAttribute::Format::UByte4Norm:
        return SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM;
    default:
        return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    }
}

// Waits for `commands` to finish on the GPU.
bool SubmitAndWait(SDL_GPUCommandBuffer* commands) {
    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
    if (!fence) {
        Log::Error("SDL_SubmitGPUCommandBufferAndAcquireFence: {}", SDL_GetError());
        return false;
    }
    SDL_WaitForGPUFences(GpuDevice(), true, &fence, 1);
    SDL_ReleaseGPUFence(GpuDevice(), fence);
    return true;
}

std::size_t PixelBytes(TargetFormat format) {
    switch (format) {
    case TargetFormat::Rgba16F:
        return 8;
    case TargetFormat::Rgba32F:
        return 16;
    default:
        return 4;
    }
}

} // namespace

SDL_GPUTextureFormat NativeFormat(TargetFormat format) {
    switch (format) {
    case TargetFormat::Rgba16F:
        return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    case TargetFormat::Rgba32F:
        return SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    case TargetFormat::R32F:
        return SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
    default:
        return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    }
}

PipelineData::PipelineData() { Pipelines.push_back(this); }
PipelineData::~PipelineData() {
    if (Native && GpuDevice()) SDL_ReleaseGPUGraphicsPipeline(GpuDevice(), Native);
    Forget(Pipelines, this);
}
BufferData::BufferData() { Buffers.push_back(this); }
BufferData::~BufferData() {
    ReleaseNative(*this);
    Forget(Buffers, this);
}
TargetData::TargetData() { Targets.push_back(this); }
TargetData::~TargetData() {
    ReleaseNative(*this);
    Forget(Targets, this);
}

bool PrepareTarget(TargetData& target) {
    if (target.External) return target.Native != nullptr;
    SDL_GPUDevice* device = GpuDevice();
    if (!device) return false;
    int width = target.FixedWidth, height = target.FixedHeight;
    if (target.Scale > 0) {
        const Vec2 screen = Screen::PixelSize();
        width = std::max(1, int(std::lround(screen.X * target.Scale)));
        height = std::max(1, int(std::lround(screen.Y * target.Scale)));
    }
    if (target.Native && target.Width == width && target.Height == height) return true;
    ReleaseNative(target);
    if (width <= 0 || height <= 0) return false;
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = NativeFormat(target.Format);
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = Uint32(width);
    info.height = Uint32(height);
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    target.Native = SDL_CreateGPUTexture(device, &info);
    if (!target.Native) {
        Log::Error("Cannot create a {}x{} render target: {}", width, height, SDL_GetError());
        return false;
    }
    target.Width = width;
    target.Height = height;
    return true;
}

void FlushUploads(SDL_GPUCommandBuffer* commands) {
    SDL_GPUDevice* device = GpuDevice();
    if (!device) return;
    std::size_t total = 0;
    for (BufferData* buffer : Buffers)
        for (const BufferData::Upload& upload : buffer->Pending) total += upload.Bytes.size();
    if (total == 0) return;
    SDL_GPUTransferBufferCreateInfo info{};
    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    info.size = Uint32(total);
    SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(device, &info);
    auto* mapped = transfer ? static_cast<std::uint8_t*>(SDL_MapGPUTransferBuffer(device, transfer, false)) : nullptr;
    if (!mapped) {
        Log::Error("GPU upload of {} bytes: {}", total, SDL_GetError());
        if (transfer) SDL_ReleaseGPUTransferBuffer(device, transfer);
        for (BufferData* buffer : Buffers) buffer->Pending.clear();
        return;
    }
    std::size_t at = 0;
    for (BufferData* buffer : Buffers)
        for (const BufferData::Upload& upload : buffer->Pending) {
            std::memcpy(mapped + at, upload.Bytes.data(), upload.Bytes.size());
            at += upload.Bytes.size();
        }
    SDL_UnmapGPUTransferBuffer(device, transfer);
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(commands);
    at = 0;
    for (BufferData* buffer : Buffers) {
        for (const BufferData::Upload& upload : buffer->Pending) {
            if (CreateNative(*buffer)) {
                SDL_GPUTransferBufferLocation source{transfer, Uint32(at)};
                SDL_GPUBufferRegion destination{buffer->Native, Uint32(upload.Offset), Uint32(upload.Bytes.size())};
                SDL_UploadToGPUBuffer(copy, &source, &destination, false);
            }
            at += upload.Bytes.size();
        }
        buffer->Pending.clear();
    }
    SDL_EndGPUCopyPass(copy);
    SDL_ReleaseGPUTransferBuffer(device, transfer); // freed once the GPU has used it
}

void UpdateGlobals(double realDt) {
    RealTime += realDt;
    const Vec2 screen = Screen::Size();
    const Vec2 mouse = Input::Mouse();
    Current.Time = float(Time::Total());
    Current.Delta = float(Time::Delta());
    Current.RealTime = float(RealTime);
    Current.Frame = float(Time::Frame());
    Current.Width = screen.X;
    Current.Height = screen.Y;
    Current.PixelWidth = screen.X > 0 ? 1.0f / screen.X : 0.0f;
    Current.PixelHeight = screen.Y > 0 ? 1.0f / screen.Y : 0.0f;
    Current.MouseX = mouse.X;
    Current.MouseY = mouse.Y;
    std::copy(Values.begin(), Values.end(), Current.Values);
}

void PushGlobals(SDL_GPUCommandBuffer* commands) {
    SDL_PushGPUVertexUniformData(commands, 0, &Current, sizeof(Current));
    SDL_PushGPUFragmentUniformData(commands, 0, &Current, sizeof(Current));
}

void ReleaseGpuObjects() {
    for (PipelineData* pipeline : Pipelines) {
        if (pipeline->Native && GpuDevice()) SDL_ReleaseGPUGraphicsPipeline(GpuDevice(), pipeline->Native);
        pipeline->Native = nullptr;
    }
    for (BufferData* buffer : Buffers) ReleaseNative(*buffer);
    for (TargetData* target : Targets) ReleaseNative(*target);
}

CustomRenderer* ActiveRenderer() { return Renderer; }

void UseCustomRenderer(CustomRenderer* renderer) {
    Renderer = renderer;
    InitCustomRenderer();
}

void InitCustomRenderer() {
    if (!Renderer) return;
    LastWidth = LastHeight = 0;
    Renderer->Init();
}

void ShutdownCustomRenderer() {
    if (!Renderer) return;
    if (GpuDevice()) SDL_WaitForGPUIdle(GpuDevice());
    Renderer->Shutdown();
    Renderer = nullptr;
}

void RenderCustom(SDL_GPUCommandBuffer* commands, TargetData& screen) {
    if (!Renderer) return;
    if (screen.Width != LastWidth || screen.Height != LastHeight) {
        LastWidth = screen.Width;
        LastHeight = screen.Height;
        Renderer->Resize(screen.Width, screen.Height);
    }
    Target screenTarget;
    Detail::TargetAccess::Wrap(screenTarget, screen);
    FlushUploads(commands);
    PushGlobals(commands);
    Frame frame(commands, &screenTarget);
    Renderer->Render(frame);
}

} // namespace Detail

// ---------------------------------------------------------------- Shader

Shader Shader::Load(const std::filesystem::path& path, ShaderStage stage, ShaderResources resources) {
    const std::filesystem::path full = path.is_absolute() ? path : Files::AssetPath(path);
    auto bytes = Files::ReadBytes(full);
    if (!bytes) {
        Log::Error("Shader {}: cannot read the file", path.string());
        return {};
    }
    Shader shader = FromMemory(*bytes, stage, resources);
    if (!shader) Log::Error("Shader {}: not SPIR-V (compile it with BitMotor_Shaders)", path.string());
    return shader;
}

Shader Shader::FromMemory(std::span<const std::uint8_t> spirv, ShaderStage stage, ShaderResources resources) {
    // SPIR-V starts with its magic number, 0x07230203.
    if (spirv.size() < 20 || spirv.size() % 4 != 0 || spirv[0] != 0x03 || spirv[1] != 0x02 || spirv[2] != 0x23 ||
        spirv[3] != 0x07)
        return {};
    Shader shader;
    shader.Data = std::make_shared<Detail::ShaderData>();
    shader.Data->Code.assign(spirv.begin(), spirv.end());
    shader.Data->Stage = stage;
    shader.Data->Resources = resources;
    return shader;
}

Shader Shader::Fullscreen() {
    static const std::shared_ptr<Detail::ShaderData> builtin = [] {
        auto data = std::make_shared<Detail::ShaderData>();
        data->Builtin = true;
        data->Stage = ShaderStage::Vertex;
        return data;
    }();
    Shader shader;
    shader.Data = builtin;
    return shader;
}

ShaderStage Shader::Stage() const { return Data ? Data->Stage : ShaderStage::Vertex; }

// ---------------------------------------------------------------- Pipeline

Pipeline Pipeline::Create(const PipelineInfo& info) {
    if (!info.Vertex || !info.Fragment || info.Vertex.Stage() != ShaderStage::Vertex ||
        info.Fragment.Stage() != ShaderStage::Fragment) {
        Log::Error("Pipeline::Create needs a vertex and a fragment shader");
        return {};
    }
    if (info.Targets.empty() || info.Targets.size() > 4) {
        Log::Error("Pipeline::Create: 1 to 4 targets, not {}", info.Targets.size());
        return {};
    }
    if (!Detail::GpuDevice()) {
        Log::Error("Pipeline::Create: there is no GPU yet (create pipelines in CustomRenderer::Init)");
        return {};
    }
    SDL_GPUShader* vertex = Detail::MakeNative(*info.Vertex.Internal());
    SDL_GPUShader* fragment = Detail::MakeNative(*info.Fragment.Internal());
    SDL_GPUColorTargetDescription targets[4]{};
    for (std::size_t i = 0; i < info.Targets.size(); ++i) {
        targets[i].format = Detail::NativeFormat(info.Targets[i]);
        targets[i].blend_state = Detail::BlendState(info.Blending);
    }
    std::vector<SDL_GPUVertexAttribute> attributes;
    for (const VertexAttribute& a : info.Attributes)
        attributes.push_back({Uint32(a.Location), 0, Detail::NativeAttribute(a.Type), Uint32(a.Offset)});
    SDL_GPUVertexBufferDescription vertexBuffer{};
    vertexBuffer.slot = 0;
    vertexBuffer.pitch = Uint32(std::max(0, info.VertexStride));
    vertexBuffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUGraphicsPipelineCreateInfo create{};
    create.vertex_shader = vertex;
    create.fragment_shader = fragment;
    create.primitive_type = Detail::NativeTopology(info.Topology);
    create.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    create.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    create.target_info.color_target_descriptions = targets;
    create.target_info.num_color_targets = Uint32(info.Targets.size());
    if (!attributes.empty()) {
        create.vertex_input_state.vertex_buffer_descriptions = &vertexBuffer;
        create.vertex_input_state.num_vertex_buffers = 1;
        create.vertex_input_state.vertex_attributes = attributes.data();
        create.vertex_input_state.num_vertex_attributes = Uint32(attributes.size());
    }
    SDL_GPUGraphicsPipeline* native =
        vertex && fragment ? SDL_CreateGPUGraphicsPipeline(Detail::GpuDevice(), &create) : nullptr;
    if (vertex) SDL_ReleaseGPUShader(Detail::GpuDevice(), vertex);
    if (fragment) SDL_ReleaseGPUShader(Detail::GpuDevice(), fragment);
    if (!native) {
        Log::Error("SDL_CreateGPUGraphicsPipeline: {}", SDL_GetError());
        return {};
    }
    Pipeline pipeline;
    pipeline.Data = std::make_shared<Detail::PipelineData>();
    pipeline.Data->Native = native;
    pipeline.Data->Targets = info.Targets;
    pipeline.Data->UsesVertices = !attributes.empty();
    return pipeline;
}

// ---------------------------------------------------------------- Buffer

Buffer Buffer::Create(BufferUsage usage, std::size_t bytes) {
    Buffer buffer;
    buffer.Data = std::make_shared<Detail::BufferData>();
    buffer.Data->Usage = usage;
    buffer.Data->Bytes = std::max<std::size_t>(bytes, 16);
    return buffer;
}

void Buffer::Upload(const void* data, std::size_t bytes, std::size_t offset) {
    if (!Data || !data || bytes == 0) return;
    if (offset + bytes > Data->Bytes) {
        // Grows to the next power of two, losing what it had (and what was waiting before the offset).
        std::size_t size = std::max<std::size_t>(Data->Bytes, 16);
        while (size < offset + bytes) size *= 2;
        Detail::ReleaseNative(*Data);
        Data->Bytes = size;
    }
    const auto* begin = static_cast<const std::uint8_t*>(data);
    Data->Pending.push_back({offset, std::vector<std::uint8_t>(begin, begin + bytes)});
}

bool Buffer::Readback(void* out, std::size_t bytes, std::size_t offset) const {
    SDL_GPUDevice* device = Detail::GpuDevice();
    if (!Data || !device || !out || offset + bytes > Data->Bytes) return false;
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device);
    if (!commands) return false;
    Detail::FlushUploads(commands);
    if (!Data->Native) {
        SDL_SubmitGPUCommandBuffer(commands);
        return false;
    }
    SDL_GPUTransferBufferCreateInfo info{};
    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    info.size = Uint32(bytes);
    SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(device, &info);
    if (!transfer) {
        SDL_SubmitGPUCommandBuffer(commands);
        return false;
    }
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(commands);
    SDL_GPUBufferRegion source{Data->Native, Uint32(offset), Uint32(bytes)};
    SDL_GPUTransferBufferLocation destination{transfer, 0};
    SDL_DownloadFromGPUBuffer(copy, &source, &destination);
    SDL_EndGPUCopyPass(copy);
    bool ok = Detail::SubmitAndWait(commands);
    if (ok) {
        if (void* mapped = SDL_MapGPUTransferBuffer(device, transfer, false)) {
            std::memcpy(out, mapped, bytes);
            SDL_UnmapGPUTransferBuffer(device, transfer);
        } else {
            ok = false;
        }
    }
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    return ok;
}

std::size_t Buffer::Size() const { return Data ? Data->Bytes : 0; }
BufferUsage Buffer::Usage() const { return Data ? Data->Usage : BufferUsage::Storage; }

// ---------------------------------------------------------------- Target

Target Target::Create(int width, int height, TargetFormat format, Filter filter) {
    if (width <= 0 || height <= 0) {
        Log::Error("Target::Create: {}x{} is not a size", width, height);
        return {};
    }
    Target target;
    target.Data = std::make_shared<Detail::TargetData>();
    target.Data->FixedWidth = width;
    target.Data->FixedHeight = height;
    target.Data->Format = format;
    target.Data->Look = filter;
    Detail::PrepareTarget(*target.Data); // now if there is a GPU, otherwise when first used
    return target;
}

Target Target::Create(TargetFormat format, float scale, Filter filter) {
    Target target;
    target.Data = std::make_shared<Detail::TargetData>();
    target.Data->Scale = scale > 0 ? scale : 1.0f;
    target.Data->Format = format;
    target.Data->Look = filter;
    Detail::PrepareTarget(*target.Data);
    return target;
}

int Target::Width() const {
    if (!Data) return 0;
    if (Data->Width > 0) return Data->Width;
    if (Data->Scale > 0) return std::max(1, int(std::lround(Screen::PixelSize().X * Data->Scale)));
    return Data->FixedWidth;
}

int Target::Height() const {
    if (!Data) return 0;
    if (Data->Height > 0) return Data->Height;
    if (Data->Scale > 0) return std::max(1, int(std::lround(Screen::PixelSize().Y * Data->Scale)));
    return Data->FixedHeight;
}

TargetFormat Target::Format() const { return Data ? Data->Format : TargetFormat::Rgba8; }
Filter Target::GetFilter() const { return Data ? Data->Look : Filter::Linear; }

bool Target::Save(const std::filesystem::path& path) const {
    SDL_GPUDevice* device = Detail::GpuDevice();
    if (!Data || !device || !Data->Native) {
        Log::Error("Target::Save: the target is not on the GPU");
        return false;
    }
    if (Data->Format != TargetFormat::Rgba8) {
        Log::Error("Target::Save: only Rgba8 targets can be saved as images");
        return false;
    }
    const Uint32 width = Uint32(Data->Width), height = Uint32(Data->Height);
    SDL_GPUTransferBufferCreateInfo info{};
    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    info.size = width * height * Uint32(Detail::PixelBytes(Data->Format));
    SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(device, &info);
    SDL_GPUCommandBuffer* commands = transfer ? SDL_AcquireGPUCommandBuffer(device) : nullptr;
    if (!commands) {
        if (transfer) SDL_ReleaseGPUTransferBuffer(device, transfer);
        return false;
    }
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(commands);
    SDL_GPUTextureRegion source{};
    source.texture = Data->Native;
    source.w = width;
    source.h = height;
    source.d = 1;
    SDL_GPUTextureTransferInfo destination{};
    destination.transfer_buffer = transfer;
    destination.pixels_per_row = width;
    destination.rows_per_layer = height;
    SDL_DownloadFromGPUTexture(copy, &source, &destination);
    SDL_EndGPUCopyPass(copy);
    bool ok = Detail::SubmitAndWait(commands);
    Detail::Image image;
    if (ok) {
        image.Width = int(width);
        image.Height = int(height);
        image.Pixels.resize(std::size_t(width) * height * 4);
        if (void* mapped = SDL_MapGPUTransferBuffer(device, transfer, false)) {
            std::memcpy(image.Pixels.data(), mapped, image.Pixels.size());
            SDL_UnmapGPUTransferBuffer(device, transfer);
        } else {
            ok = false;
        }
    }
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    return ok && Detail::SaveImage(image, path);
}

namespace Detail {

void TargetAccess::Wrap(Target& target, TargetData& data) {
    // Shares the engine's screen target without owning it.
    target.Data = std::shared_ptr<TargetData>(&data, [](TargetData*) {});
}

} // namespace Detail

// ---------------------------------------------------------------- Gpu

namespace Gpu {

SDL_GPUDevice* Native() { return Detail::GpuDevice(); }

void SetGlobal(int index, Vec4 value) {
    if (index < 0 || index >= int(Detail::Values.size())) {
        Log::Warn("Gpu::SetGlobal: index {} out of 0..12", index);
        return;
    }
    Detail::Values[std::size_t(index)] = value;
    Detail::Current.Values[index] = value;
}

Vec4 Global(int index) {
    if (index < 0 || index >= int(Detail::Values.size())) return {};
    return Detail::Values[std::size_t(index)];
}

const Globals& CurrentGlobals() { return Detail::Current; }

} // namespace Gpu

// ---------------------------------------------------------------- Frame

Frame::~Frame() {
    if (Pass) {
        Log::Warn("Frame: a pass was not ended (Frame::End)");
        End();
    }
}

Target& Frame::Screen() { return *ScreenTarget; }

void Frame::Begin(Target& target, std::optional<Color> clear) { Begin({&target}, clear); }

void Frame::Begin(std::initializer_list<Target*> targets, std::optional<Color> clear) {
    if (Pass) {
        Log::Warn("Frame::Begin: the previous pass was not ended");
        End();
    }
    if (targets.size() == 0 || targets.size() > 4) {
        Log::Error("Frame::Begin: 1 to 4 targets, not {}", targets.size());
        return;
    }
    Detail::FlushUploads(Commands); // a copy pass cannot happen inside a render pass
    SDL_GPUColorTargetInfo infos[4]{};
    Uint32 count = 0;
    for (Target* target : targets) {
        if (!target || !*target || !Detail::PrepareTarget(*target->Internal())) {
            Log::Error("Frame::Begin: a target is empty or cannot be created");
            return;
        }
        SDL_GPUColorTargetInfo& info = infos[count++];
        info.texture = target->Internal()->Native;
        info.load_op = clear ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
        info.store_op = SDL_GPU_STOREOP_STORE;
        if (clear) info.clear_color = {clear->R, clear->G, clear->B, clear->A};
    }
    Pass = SDL_BeginGPURenderPass(Commands, infos, count, nullptr);
    if (!Pass) {
        Log::Error("SDL_BeginGPURenderPass: {}", SDL_GetError());
        return;
    }
    First = *targets.begin();
    Bound = nullptr;
}

void Frame::End() {
    if (!Pass) return;
    SDL_EndGPURenderPass(Pass);
    Pass = nullptr;
    First = nullptr;
    Bound = nullptr;
}

void Frame::Bind(const Pipeline& pipeline) {
    if (!Pass || !pipeline || !pipeline.Internal()->Native) return;
    SDL_BindGPUGraphicsPipeline(Pass, pipeline.Internal()->Native);
    Bound = pipeline.Internal().get();
}

void Frame::BindTexture(int slot, const Target& target, ShaderStage stage) {
    if (!Pass || !target || !Detail::PrepareTarget(*target.Internal())) return;
    SDL_GPUTextureSamplerBinding binding{target.Internal()->Native, Detail::SamplerFor(target.GetFilter())};
    if (stage == ShaderStage::Vertex)
        SDL_BindGPUVertexSamplers(Pass, Uint32(slot), &binding, 1);
    else
        SDL_BindGPUFragmentSamplers(Pass, Uint32(slot), &binding, 1);
}

void Frame::BindTexture(int slot, const Texture& texture, ShaderStage stage) {
    if (!Pass || !texture) return;
    SDL_GPUTexture* native = Detail::GpuTexture(*texture.Internal());
    if (!native) return;
    SDL_GPUTextureSamplerBinding binding{native, Detail::SamplerFor(texture.GetFilter())};
    if (stage == ShaderStage::Vertex)
        SDL_BindGPUVertexSamplers(Pass, Uint32(slot), &binding, 1);
    else
        SDL_BindGPUFragmentSamplers(Pass, Uint32(slot), &binding, 1);
}

void Frame::BindStorage(ShaderStage stage, int slot, const Buffer& buffer) {
    if (!Pass || !buffer || !Detail::CreateNative(*buffer.Internal())) return;
    SDL_GPUBuffer* native = buffer.Internal()->Native;
    if (stage == ShaderStage::Vertex)
        SDL_BindGPUVertexStorageBuffers(Pass, Uint32(slot), &native, 1);
    else
        SDL_BindGPUFragmentStorageBuffers(Pass, Uint32(slot), &native, 1);
}

void Frame::BindVertices(const Buffer& buffer, std::size_t offset) {
    if (!Pass || !buffer || !Detail::CreateNative(*buffer.Internal())) return;
    SDL_GPUBufferBinding binding{buffer.Internal()->Native, Uint32(offset)};
    SDL_BindGPUVertexBuffers(Pass, 0, &binding, 1);
}

void Frame::BindIndices(const Buffer& buffer, std::size_t offset) {
    if (!Pass || !buffer || !Detail::CreateNative(*buffer.Internal())) return;
    SDL_GPUBufferBinding binding{buffer.Internal()->Native, Uint32(offset)};
    SDL_BindGPUIndexBuffer(Pass, &binding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
}

void Frame::Push(ShaderStage stage, int slot, const void* data, std::size_t bytes) {
    if (slot < 1 || slot > 3) {
        Log::Warn("Frame::Push: uniform block {} (the game's are 1..3; 0 is Gpu::Globals)", slot);
        return;
    }
    if (!data || bytes == 0 || bytes > 256) {
        Log::Warn("Frame::Push: {} bytes (1 to 256)", bytes);
        return;
    }
    if (stage == ShaderStage::Vertex)
        SDL_PushGPUVertexUniformData(Commands, Uint32(slot), data, Uint32(bytes));
    else
        SDL_PushGPUFragmentUniformData(Commands, Uint32(slot), data, Uint32(bytes));
}

void Frame::Draw(int vertices, int instances, int firstVertex, int firstInstance) {
    if (!Pass || !Bound || vertices <= 0 || instances <= 0) return;
    SDL_DrawGPUPrimitives(Pass, Uint32(vertices), Uint32(instances), Uint32(firstVertex), Uint32(firstInstance));
}

void Frame::DrawIndexed(int indices, int instances, int firstIndex, int vertexOffset, int firstInstance) {
    if (!Pass || !Bound || indices <= 0 || instances <= 0) return;
    SDL_DrawGPUIndexedPrimitives(Pass, Uint32(indices), Uint32(instances), Uint32(firstIndex), Sint32(vertexOffset),
                                 Uint32(firstInstance));
}

void Frame::Fullscreen(const Pipeline& pipeline) {
    Bind(pipeline);
    Draw(3);
}

void Frame::Draw2D() {
    if (!Pass || !First) {
        Log::Warn("Frame::Draw2D: needs a pass (Frame::Begin)");
        return;
    }
    const TargetFormat format = First->Format();
    if (format != TargetFormat::Rgba8 && format != TargetFormat::Rgba16F) {
        Log::Warn("Frame::Draw2D: the target must be Rgba8 or Rgba16F");
        return;
    }
    Detail::DrawSprites2D(Commands, Pass, format == TargetFormat::Rgba16F);
    Bound = nullptr;
    Detail::PushGlobals(Commands); // the sprites used uniform block 0
}

} // namespace BitMotor
