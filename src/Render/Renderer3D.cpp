// 3D on the GPU: the queued models into the HDR scene, after the Background layer and before the World one,
// with a depth buffer of their own. One pipeline: vertices with position, normal, uv and colour; a texture
// (white when the material has none); ambient + sun + up to 8 point lights per model.
#include "BitMotor/Core/Log.h"
#include "BitMotor/Math/Functions.h"
#include "BitMotor/Render/Light.h"
#include "Render/GpuInternal.h"
#include "Render/ModelInternal.h"
#include "Render/RendererInternal.h"
#include "Render/Shaders/Compiled.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstddef>
#include <cstring>

namespace BitMotor::Detail {

namespace {

constexpr SDL_GPUTextureFormat SceneFormat = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;

std::vector<MeshData*> Meshes; // every live mesh, to release their buffers when the device stops
SDL_GPUGraphicsPipeline* Pipeline = nullptr;
SDL_GPUTexture* Depth = nullptr;
SDL_GPUTextureFormat DepthFormat = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
unsigned DepthWidth = 0, DepthHeight = 0;
Texture White;

struct ObjectUniforms {
    Mat4 ViewProjection;
    Mat4 World;
    Mat4 Normal;
    Color Tint;
    float Options[4];
};

struct LightUniforms {
    float Sun[4];
    float SunColor[4];
    float Ambient[4];
    float Options[4];
    float Position[8][4];
    float Colour[8][4];
};

SDL_GPUShader* MakeShader(const std::uint32_t* code, std::size_t bytes, SDL_GPUShaderStage stage, Uint32 uniforms,
                          Uint32 samplers) {
    SDL_GPUShaderCreateInfo info{};
    info.code = reinterpret_cast<const Uint8*>(code);
    info.code_size = bytes;
    info.entrypoint = "main";
    info.format = SDL_GPU_SHADERFORMAT_SPIRV;
    info.stage = stage;
    info.num_uniform_buffers = uniforms;
    info.num_samplers = samplers;
    SDL_GPUShader* shader = SDL_CreateGPUShader(GpuDevice(), &info);
    if (!shader) Log::Error("SDL_CreateGPUShader: {}", SDL_GetError());
    return shader;
}

void ReleaseBuffers(MeshData& mesh) {
    if (SDL_GPUDevice* device = GpuDevice()) {
        if (mesh.VertexBuffer) SDL_ReleaseGPUBuffer(device, mesh.VertexBuffer);
        if (mesh.IndexBuffer) SDL_ReleaseGPUBuffer(device, mesh.IndexBuffer);
    }
    mesh.VertexBuffer = mesh.IndexBuffer = nullptr;
}

bool Upload(SDL_GPUCopyPass* copy, MeshData& mesh) {
    SDL_GPUDevice* device = GpuDevice();
    const Uint32 vertexBytes = Uint32(mesh.Vertices.size() * sizeof(Vertex3D));
    const Uint32 indexBytes = Uint32(mesh.Indices.size() * sizeof(std::uint32_t));
    if (vertexBytes == 0 || indexBytes == 0) return false;
    SDL_GPUBufferCreateInfo info{};
    info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    info.size = vertexBytes;
    mesh.VertexBuffer = SDL_CreateGPUBuffer(device, &info);
    info.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    info.size = indexBytes;
    mesh.IndexBuffer = SDL_CreateGPUBuffer(device, &info);
    SDL_GPUTransferBufferCreateInfo transferInfo{};
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferInfo.size = vertexBytes + indexBytes;
    SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(device, &transferInfo);
    auto* mapped = transfer ? static_cast<std::uint8_t*>(SDL_MapGPUTransferBuffer(device, transfer, false)) : nullptr;
    if (!mesh.VertexBuffer || !mesh.IndexBuffer || !mapped) {
        Log::Error("Cannot put a mesh on the GPU: {}", SDL_GetError());
        if (transfer) SDL_ReleaseGPUTransferBuffer(device, transfer);
        ReleaseBuffers(mesh);
        return false;
    }
    std::memcpy(mapped, mesh.Vertices.data(), vertexBytes);
    std::memcpy(mapped + vertexBytes, mesh.Indices.data(), indexBytes);
    SDL_UnmapGPUTransferBuffer(device, transfer);
    SDL_GPUTransferBufferLocation source{transfer, 0};
    SDL_GPUBufferRegion destination{mesh.VertexBuffer, 0, vertexBytes};
    SDL_UploadToGPUBuffer(copy, &source, &destination, false);
    source.offset = vertexBytes;
    destination = {mesh.IndexBuffer, 0, indexBytes};
    SDL_UploadToGPUBuffer(copy, &source, &destination, false);
    SDL_ReleaseGPUTransferBuffer(device, transfer); // freed once the GPU has used it
    return true;
}

bool PrepareDepth(unsigned width, unsigned height) {
    if (Depth && DepthWidth == width && DepthHeight == height) return true;
    if (Depth) SDL_ReleaseGPUTexture(GpuDevice(), Depth);
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = DepthFormat;
    info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    info.width = width;
    info.height = height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    Depth = SDL_CreateGPUTexture(GpuDevice(), &info);
    DepthWidth = Depth ? width : 0;
    DepthHeight = Depth ? height : 0;
    if (!Depth) Log::Error("Cannot create the 3D depth buffer: {}", SDL_GetError());
    return Depth != nullptr;
}

// The 3x3 part of the inverse transpose, for normals under non-uniform scale.
Mat4 NormalMatrix(const Mat4& world) {
    Mat4 m = world;
    m.M[12] = m.M[13] = m.M[14] = 0;
    const Mat4 inverse = m.Inverse();
    Mat4 t;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) t.At(r, c) = inverse.At(c, r);
    return t;
}

void Copy(float out[4], Color c) { out[0] = c.R, out[1] = c.G, out[2] = c.B, out[3] = c.A; }

} // namespace

MeshData::MeshData() { Meshes.push_back(this); }
MeshData::~MeshData() {
    ReleaseBuffers(*this);
    std::erase(Meshes, this);
}

bool StartModels() {
    SDL_GPUDevice* device = GpuDevice();
    DepthFormat = SDL_GPUTextureSupportsFormat(device, SDL_GPU_TEXTUREFORMAT_D32_FLOAT, SDL_GPU_TEXTURETYPE_2D,
                                               SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)
                      ? SDL_GPU_TEXTUREFORMAT_D32_FLOAT
                      : SDL_GPU_TEXTUREFORMAT_D24_UNORM;
    SDL_GPUShader* vertex =
        MakeShader(Shaders::ModelVert, sizeof(Shaders::ModelVert), SDL_GPU_SHADERSTAGE_VERTEX, 1, 0);
    SDL_GPUShader* fragment =
        MakeShader(Shaders::ModelFrag, sizeof(Shaders::ModelFrag), SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1);
    SDL_GPUColorTargetDescription target{};
    target.format = SceneFormat;
    target.blend_state.enable_blend = true;
    target.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    target.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    target.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    target.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    target.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    target.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    const SDL_GPUVertexAttribute attributes[4] = {
        {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, Uint32(offsetof(Vertex3D, Position))},
        {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, Uint32(offsetof(Vertex3D, Normal))},
        {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, Uint32(offsetof(Vertex3D, Uv))},
        {3, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, Uint32(offsetof(Vertex3D, Tint))}};
    SDL_GPUVertexBufferDescription buffer{0, sizeof(Vertex3D), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0};
    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = vertex;
    info.fragment_shader = fragment;
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.vertex_input_state.vertex_buffer_descriptions = &buffer;
    info.vertex_input_state.num_vertex_buffers = 1;
    info.vertex_input_state.vertex_attributes = attributes;
    info.vertex_input_state.num_vertex_attributes = 4;
    info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
    info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    info.depth_stencil_state.enable_depth_test = true;
    info.depth_stencil_state.enable_depth_write = true;
    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    info.target_info.color_target_descriptions = &target;
    info.target_info.num_color_targets = 1;
    info.target_info.has_depth_stencil_target = true;
    info.target_info.depth_stencil_format = DepthFormat;
    Pipeline = vertex && fragment ? SDL_CreateGPUGraphicsPipeline(device, &info) : nullptr;
    if (vertex) SDL_ReleaseGPUShader(device, vertex);
    if (fragment) SDL_ReleaseGPUShader(device, fragment);
    if (!Pipeline) Log::Error("The 3D pipeline: {}", SDL_GetError());
    const std::uint8_t white[4] = {255, 255, 255, 255};
    White = Texture::FromPixels(1, 1, white);
    return Pipeline != nullptr;
}

void StopModels() {
    SDL_GPUDevice* device = GpuDevice();
    ReleaseMeshes();
    if (Pipeline && device) SDL_ReleaseGPUGraphicsPipeline(device, Pipeline);
    if (Depth && device) SDL_ReleaseGPUTexture(device, Depth);
    Pipeline = nullptr;
    Depth = nullptr;
    DepthWidth = DepthHeight = 0;
    White = {};
    ResetModels();
}

void ReleaseMeshes() {
    for (MeshData* mesh : Meshes) ReleaseBuffers(*mesh);
}

void UploadModels(SDL_GPUCopyPass* copy) {
    for (ModelDraw& draw : ModelQueue())
        if (draw.Geometry && !draw.Geometry->VertexBuffer) Upload(copy, *draw.Geometry);
}

void DrawModels(SDL_GPUCommandBuffer* commands, SDL_GPUTexture* scene, unsigned width, unsigned height) {
    std::vector<ModelDraw>& queue = ModelQueue();
    if (queue.empty() || !Pipeline || !PrepareDepth(width, height)) return;
    const Camera3D& camera = Screen::CurrentCamera3D();
    const Mat4 viewProjection = camera.ViewProjection();
    const bool linear = LinearColor();

    // Opaque first, then the translucent ones from far to near.
    auto translucent = [](const ModelDraw& d) { return d.Tint.A * d.Look.Tint.A < 0.999f; };
    auto distance = [&](const ModelDraw& d) {
        return LengthSquared(Vec3(d.World.M[12], d.World.M[13], d.World.M[14]) - camera.Position);
    };
    std::stable_sort(queue.begin(), queue.end(), [&](const ModelDraw& a, const ModelDraw& b) {
        if (translucent(a) != translucent(b)) return !translucent(a);
        return translucent(a) && distance(a) > distance(b);
    });

    SDL_GPUColorTargetInfo color{};
    color.texture = scene;
    color.load_op = SDL_GPU_LOADOP_LOAD;
    color.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPUDepthStencilTargetInfo depth{};
    depth.texture = Depth;
    depth.clear_depth = 1;
    depth.load_op = SDL_GPU_LOADOP_CLEAR;
    depth.store_op = SDL_GPU_STOREOP_DONT_CARE;
    depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &color, 1, &depth);
    if (!pass) return;
    SDL_BindGPUGraphicsPipeline(pass, Pipeline);

    LightUniforms light{};
    const Vec3 sun = Lighting3D::SunDirection();
    light.Sun[0] = sun.X, light.Sun[1] = sun.Y, light.Sun[2] = sun.Z;
    Copy(light.SunColor, ToLinear(Lighting3D::SunColor()));
    Copy(light.Ambient, ToLinear(Lighting3D::Ambient()));
    light.Ambient[3] = Lighting::Stepped() ? 1.0f : 0.0f;
    std::vector<const PointLight3D*> lights;
    for (const PointLight3D& l : PointLightQueue()) lights.push_back(&l);

    for (const ModelDraw& draw : queue) {
        MeshData& mesh = *draw.Geometry;
        if (!mesh.VertexBuffer || !mesh.IndexBuffer) continue;
        ObjectUniforms object{};
        object.ViewProjection = viewProjection;
        object.World = draw.World;
        object.Normal = NormalMatrix(draw.World);
        object.Tint = {draw.Tint.R * draw.Look.Tint.R, draw.Tint.G * draw.Look.Tint.G, draw.Tint.B * draw.Look.Tint.B,
                       draw.Tint.A * draw.Look.Tint.A};
        object.Options[0] = linear ? 1.0f : 0.0f;
        SDL_PushGPUVertexUniformData(commands, 0, &object, sizeof(object));

        // The 8 lights that reach this model most.
        const Vec3 centre = draw.World.TransformPoint((mesh.Min + mesh.Max) * 0.5f);
        auto reach = [&](const PointLight3D* l) { return Distance(l->Position, centre) - l->Radius; };
        const std::size_t count = std::min<std::size_t>(8, lights.size());
        std::partial_sort(lights.begin(), lights.begin() + std::ptrdiff_t(count), lights.end(),
                          [&](const PointLight3D* a, const PointLight3D* b) { return reach(a) < reach(b); });
        for (std::size_t i = 0; i < count; ++i) {
            const PointLight3D& l = *lights[i];
            light.Position[i][0] = l.Position.X, light.Position[i][1] = l.Position.Y;
            light.Position[i][2] = l.Position.Z, light.Position[i][3] = l.Radius;
            const Color c = ToLinear(l.Color);
            light.Colour[i][0] = c.R * l.Intensity, light.Colour[i][1] = c.G * l.Intensity;
            light.Colour[i][2] = c.B * l.Intensity, light.Colour[i][3] = 0;
        }
        light.Options[0] = draw.Look.Emission;
        light.Options[1] = draw.Look.Unlit ? 1.0f : 0.0f;
        light.Options[2] = linear ? 1.0f : 0.0f;
        light.Options[3] = float(count);
        SDL_PushGPUFragmentUniformData(commands, 0, &light, sizeof(light));

        const Texture& image = draw.Look.Image ? draw.Look.Image : White;
        SDL_GPUTexture* texture = GpuTexture(*image.Internal());
        if (!texture) texture = GpuTexture(*White.Internal());
        SDL_GPUTextureSamplerBinding binding{texture, SamplerFor(image.GetFilter())};
        SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
        SDL_GPUBufferBinding vertices{mesh.VertexBuffer, 0};
        SDL_BindGPUVertexBuffers(pass, 0, &vertices, 1);
        SDL_GPUBufferBinding indices{mesh.IndexBuffer, 0};
        SDL_BindGPUIndexBuffer(pass, &indices, SDL_GPU_INDEXELEMENTSIZE_32BIT);
        SDL_DrawGPUIndexedPrimitives(pass, Uint32(mesh.Indices.size()), 1, 0, 0, 0);
    }
    SDL_EndGPURenderPass(pass);
}

} // namespace BitMotor::Detail
