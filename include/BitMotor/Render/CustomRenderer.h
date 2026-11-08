#pragma once
#include "BitMotor/Core/Game.h"
#include "BitMotor/Core/Services.h"
#include "BitMotor/Render/Gpu.h"

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <type_traits>

struct SDL_GPUCommandBuffer;
struct SDL_GPURenderPass;

// #include <BitMotor/Render/CustomRenderer.h>: not in BitMotor.h, so that its names (Frame, Target, Buffer...)
// stay out of the games that do not need them.
// A game's own renderer, in place of BitMotor's 2D one. BitMotor still owns the window, the scaling of the
// screen into it (with Config::Width/Height), screenshots, VSync and the debug panel; the renderer draws the
// frame into Frame::Screen.
//     struct InkRenderer : CustomRenderer {
//         Pipeline shapes, post;
//         Target scene = Target::Create(TargetFormat::Rgba16F);
//         void Init() override { shapes = Pipeline::Create(...); post = ...; }
//         void Render(Frame& frame) override {
//             frame.Begin(scene, Colors::White);
//             frame.Bind(shapes);
//             frame.BindStorage(ShaderStage::Vertex, 0, instances);
//             frame.Draw(6, count);
//             frame.End();
//             frame.Begin(frame.Screen());
//             frame.BindTexture(0, scene);
//             frame.Fullscreen(post);
//             frame.Draw2D();                       // BitMotor's sprites, text and UI on top
//             frame.End();
//         }
//     };
//     int main() { return Run<MyGame, InkRenderer>({.Title = "Ink", .Width = 1920, .Height = 1080}); }
//
// The renderer is also a Service: the game reaches it with Services::Get<InkRenderer>().
namespace BitMotor {

class Frame;

class CustomRenderer : public Service {
public:
    // Once the GPU is ready: create pipelines, buffers and targets here.
    virtual void Init() {}
    // The screen changed size (targets made with Target::Create(format, scale) follow it on their own).
    virtual void Resize(int width, int height) { (void)width, (void)height; }
    // Once per frame, after the game's Draw. Whatever ends up in frame.Screen() is shown.
    virtual void Render(Frame& frame) = 0;
    // Before the GPU stops: what Init made can be released here (or left to its destructors).
    virtual void Shutdown() {}
};

// One frame on the GPU: a sequence of passes, each drawing into one or more targets.
class Frame {
public:
    // The screen at its logical size (Config::Width/Height), or at Screen::PixelSize with
    // Config::NativeResolution: what is shown, scaled into the window.
    Target& Screen();

    // Starts a pass into `target` (a clear colour clears it first; without one it keeps what it had).
    void Begin(Target& target, std::optional<Color> clear = std::nullopt);
    // Into several targets at once (a pipeline with as many Targets formats).
    void Begin(std::initializer_list<Target*> targets, std::optional<Color> clear = std::nullopt);
    void End();

    void Bind(const Pipeline& pipeline);
    // Textures for the shaders (sampler slot `slot`, in the stage's texture set).
    void BindTexture(int slot, const Target& target, ShaderStage stage = ShaderStage::Fragment);
    void BindTexture(int slot, const Texture& texture, ShaderStage stage = ShaderStage::Fragment);
    void BindStorage(ShaderStage stage, int slot, const Buffer& buffer);
    void BindVertices(const Buffer& buffer, std::size_t offset = 0);
    void BindIndices(const Buffer& buffer, std::size_t offset = 0);
    // Uniform block `slot` (1 or more: 0 is Gpu::Globals) of a stage, for the next draws. Up to 256 bytes,
    // laid out as std140 (a struct of floats and Vec4s).
    void Push(ShaderStage stage, int slot, const void* data, std::size_t bytes);
    template <class T> void Push(ShaderStage stage, int slot, const T& value) { Push(stage, slot, &value, sizeof(T)); }

    void Draw(int vertices, int instances = 1, int firstVertex = 0, int firstInstance = 0);
    void DrawIndexed(int indices, int instances = 1, int firstIndex = 0, int vertexOffset = 0, int firstInstance = 0);
    // Binds `pipeline` (made with Shader::Fullscreen as its vertex shader) and covers the whole target.
    void Fullscreen(const Pipeline& pipeline);
    // This frame's sprites, text, shapes and UI (everything queued with Screen::Draw and friends), drawn
    // into the pass's first target as BitMotor's 2D renderer would, without lighting: the camera, the
    // layers in order, Glow added. The target must be Rgba8 or Rgba16F.
    void Draw2D();

    // The SDL_GPU objects, to call SDL_GPU directly between BitMotor's calls.
    SDL_GPUCommandBuffer* NativeCommands() const { return Commands; }
    SDL_GPURenderPass* NativePass() const { return Pass; }

    // Internal: made by BitMotor for each frame.
    Frame(SDL_GPUCommandBuffer* commands, Target* screen) : Commands(commands), ScreenTarget(screen) {}
    ~Frame();
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;

private:
    SDL_GPUCommandBuffer* Commands = nullptr;
    SDL_GPURenderPass* Pass = nullptr;
    Target* ScreenTarget = nullptr;
    // The pass's first target (Draw2D draws into it) and the pipeline bound now.
    Target* First = nullptr;
    const Detail::PipelineData* Bound = nullptr;
};

namespace Detail {
// Run with a renderer: it is added as a Service, and the GPU frame calls it instead of the 2D renderer.
void UseCustomRenderer(CustomRenderer* renderer);
} // namespace Detail

// Runs `T` drawn by `R` (a CustomRenderer). `R` is created before Game::Start, so the game can use it there.
template <class T, class R> int Run(const Config& config = {}) {
    static_assert(std::is_base_of_v<CustomRenderer, R>, "Run<Game, R>: R must derive from BitMotor::CustomRenderer");
#ifdef BITMOTOR_SOURCE_DIR
    Detail::SetSourceDir(BITMOTOR_SOURCE_DIR);
#endif
    T game;
    return Detail::Run(config, game, [] { Detail::UseCustomRenderer(&Services::Add<R>()); });
}

} // namespace BitMotor
