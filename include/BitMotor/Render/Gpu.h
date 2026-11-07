#pragma once
#include "BitMotor/Colors/Color.h"
#include "BitMotor/Math/Vector.h"
#include "BitMotor/Render/Texture.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <memory>
#include <span>
#include <vector>

struct SDL_GPUDevice;

// The GPU, for games that draw their own way (a CustomRenderer). It is SDL_GPU underneath (Vulkan), with
// shaders in SPIR-V: compile GLSL with BitMotor_Shaders in CMake. SDL_GPU decides where each resource of a
// shader goes, so a shader declares them in these sets:
//     vertex shader:    set 0 textures (samplers), then storage buffers · set 1 uniform blocks
//     fragment shader:  set 2 textures (samplers), then storage buffers · set 3 uniform blocks
// Uniform block 0 of both stages is BitMotor's globals (Gpu::Globals, below); the game's own start at 1.
//
//     Shader vertex = Shader::Load("shaders/shapes.vert.spv", ShaderStage::Vertex, {.Uniforms = 2, .StorageBuffers =
//     1}); Shader fragment = Shader::Load("shaders/shapes.frag.spv", ShaderStage::Fragment, {.Uniforms = 1}); Pipeline
//     shapes = Pipeline::Create({.Vertex = vertex, .Fragment = fragment, .Blending = Blend::Alpha}); Buffer instances =
//     Buffer::Create(BufferUsage::Storage, 1 << 20); Target scene = Target::Create(TargetFormat::Rgba16F);       // as
//     big as the screen, always
namespace BitMotor {

enum class ShaderStage { Vertex, Fragment };

// How many resources of each kind the shader declares (SDL_GPU needs to know).
struct ShaderResources {
    int Uniforms = 0;
    int Samplers = 0;
    int StorageBuffers = 0;
};

namespace Detail {
struct ShaderData;
struct PipelineData;
struct BufferData;
struct TargetData;
struct TargetAccess;
} // namespace Detail

class Shader {
public:
    Shader() = default;
    // A SPIR-V file (relative paths: Files::AssetPath). Empty (and a log error) if it cannot be read.
    static Shader Load(const std::filesystem::path& path, ShaderStage stage, ShaderResources resources = {});
    static Shader FromMemory(std::span<const std::uint8_t> spirv, ShaderStage stage, ShaderResources resources = {});
    // BitMotor's own vertex shader for Frame::Fullscreen: one triangle over the whole target, with the
    // texture coordinate (0,0 at the top left) at location 0.
    static Shader Fullscreen();

    ShaderStage Stage() const;
    explicit operator bool() const { return Data != nullptr; }
    const std::shared_ptr<Detail::ShaderData>& Internal() const { return Data; }

private:
    std::shared_ptr<Detail::ShaderData> Data;
};

enum class TargetFormat {
    Rgba8,   // 8 bits per channel, like the screen
    Rgba16F, // half floats: HDR colours, values above 1
    Rgba32F, // full floats: data
    R32F,    // one float
};

enum class Blend {
    None,          // replaces what is there
    Alpha,         // straight alpha over what is there
    Premultiplied, // the colour already times its alpha
    Additive,      // adds light
    Multiply,      // darkens: ink, shadows
};

enum class Topology { Triangles, TriangleStrip, Lines, LineStrip, Points };

// One field of a vertex in a vertex buffer, at `Location` in the vertex shader.
struct VertexAttribute {
    enum class Format { Float, Float2, Float3, Float4, UByte4Norm } Type = Format::Float2;
    int Location = 0;
    int Offset = 0; // bytes from the start of the vertex
};

struct PipelineInfo {
    Shader Vertex = {};
    Shader Fragment = {};
    // The formats of the targets it draws into, in order (Frame::Begin with as many targets).
    std::vector<TargetFormat> Targets = {TargetFormat::Rgba8};
    BitMotor::Blend Blending = Blend::Alpha;
    BitMotor::Topology Topology = Topology::Triangles;
    // Empty: no vertex buffer (the shader builds its vertices from gl_VertexIndex, or reads a storage buffer).
    std::vector<VertexAttribute> Attributes = {};
    int VertexStride = 0; // bytes per vertex
};

class Pipeline {
public:
    Pipeline() = default;
    // Empty (and a log error) if the shaders are missing or SDL_GPU refuses it.
    static Pipeline Create(const PipelineInfo& info);

    explicit operator bool() const { return Data != nullptr; }
    const std::shared_ptr<Detail::PipelineData>& Internal() const { return Data; }

private:
    std::shared_ptr<Detail::PipelineData> Data;
};

enum class BufferUsage {
    Vertex,  // Frame::BindVertices
    Index,   // Frame::BindIndices (32-bit indices)
    Storage, // read by shaders: Frame::BindStorage
};

class Buffer {
public:
    Buffer() = default;
    static Buffer Create(BufferUsage usage, std::size_t bytes);

    // Copies `bytes` to the GPU at `offset`. Uploads wait, in order, until the next pass starts (Frame::Begin)
    // or the next frame. A buffer too small grows to fit (what it held before is lost).
    void Upload(const void* data, std::size_t bytes, std::size_t offset = 0);
    template <class T> void Upload(std::span<const T> items, std::size_t offset = 0) {
        Upload(items.data(), items.size_bytes(), offset);
    }
    template <class T> void Upload(const std::vector<T>& items, std::size_t offset = 0) {
        Upload(items.data(), items.size() * sizeof(T), offset);
    }
    // Reads it back from the GPU, waiting for everything drawn so far (slow: for tools and tests).
    bool Readback(void* out, std::size_t bytes, std::size_t offset = 0) const;

    std::size_t Size() const;
    BufferUsage Usage() const;
    explicit operator bool() const { return Data != nullptr; }
    const std::shared_ptr<Detail::BufferData>& Internal() const { return Data; }

private:
    std::shared_ptr<Detail::BufferData> Data;
};

// An image to draw into and then read from a shader (Frame::BindTexture).
class Target {
public:
    Target() = default;
    // A fixed size.
    static Target Create(int width, int height, TargetFormat format = TargetFormat::Rgba8,
                         Filter filter = Filter::Linear);
    // `scale` times the screen's size (Screen::Size), and it follows it when the window changes.
    static Target Create(TargetFormat format = TargetFormat::Rgba8, float scale = 1, Filter filter = Filter::Linear);

    int Width() const;
    int Height() const;
    Vec2 Size() const { return {float(Width()), float(Height())}; }
    TargetFormat Format() const;
    BitMotor::Filter GetFilter() const;
    // Saves what it holds to a PNG (waits for the GPU; for tools and tests).
    bool Save(const std::filesystem::path& path) const;

    explicit operator bool() const { return Data != nullptr; }
    bool operator==(const Target& other) const { return Data == other.Data; }
    const std::shared_ptr<Detail::TargetData>& Internal() const { return Data; }

private:
    friend struct Detail::TargetAccess;
    std::shared_ptr<Detail::TargetData> Data;
};

// Two targets that swap every frame: draw into Current reading Previous (trails, feedback, simulations).
struct PingPong {
    Target A, B;
    bool Flipped = false;

    static PingPong Create(TargetFormat format = TargetFormat::Rgba8, float scale = 1, Filter filter = Filter::Linear) {
        return {Target::Create(format, scale, filter), Target::Create(format, scale, filter)};
    }
    Target& Current() { return Flipped ? B : A; }
    Target& Previous() { return Flipped ? A : B; }
    void Swap() { Flipped = !Flipped; }
};

namespace Gpu {

// What every shader can read in uniform block 0 (std140): set 1 binding 0 in vertex shaders, set 3
// binding 0 in fragment shaders.
struct Globals {
    float Time = 0, Delta = 0, RealTime = 0, Frame = 0;           // Time::Total, Delta, real seconds, frame number
    float Width = 0, Height = 0, PixelWidth = 0, PixelHeight = 0; // the screen, and 1 / its size
    float MouseX = 0, MouseY = 0, Unused0 = 0, Unused1 = 0;       // Input::Mouse, screen pixels
    Vec4 Values[13] = {};                                         // Gpu::SetGlobal
};
static_assert(sizeof(Globals) == 256);

// The SDL_GPU device (nullptr before the window opens), to call SDL_GPU directly.
SDL_GPUDevice* Native();
// Sets Globals::Values[index] (0..12) for every shader, from now on.
void SetGlobal(int index, Vec4 value);
Vec4 Global(int index);
// This frame's globals as the shaders get them.
const Globals& CurrentGlobals();

} // namespace Gpu

} // namespace BitMotor
