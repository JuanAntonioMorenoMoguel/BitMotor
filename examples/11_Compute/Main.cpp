// 11_Compute: the GPU does the work, not only the drawing. Compute shaders (Shaders/*.comp, compiled by
// BitMotor_Shaders like the others) run in a compute pass of the CustomRenderer's frame:
// - Simulate.comp moves 65536 particles that live in a storage buffer on the GPU. The CPU only fills it once;
//   every frame the shader reads and writes it, and the draw that follows reads the same buffer.
// - Background.comp writes the HDR scene's pixels directly (an image2D), before the particles are added.
// - Hold the mouse to pull the particles. In a Debug build, edit Shaders/Simulate.comp and save: the shader is
//   compiled again and the change shows without restarting. Esc quits.
#include <BitMotor/BitMotor.h>
#include <BitMotor/Render/CustomRenderer.h>

#include <format>
#include <vector>

using namespace BitMotor;

// As Particle.glsl declares it (std430: 32 bytes).
struct Particle {
    Vec2 Position;
    Vec2 Velocity;
    Vec4 Tint;
};
static_assert(sizeof(Particle) == 32);

constexpr int Count = 65536;

struct ComputeRenderer : CustomRenderer {
    ComputePipeline simulate, background;
    Pipeline particles, composite;
    Buffer buffer = Buffer::Create(BufferUsage::Storage, Count * sizeof(Particle));
    Target scene = Target::Create(TargetFormat::Rgba16F);
    Vec4 pull;

    void Init() override {
        simulate = ComputePipeline::Create(Shader::Load("11_Compute/Shaders/Simulate.comp.spv", ShaderStage::Compute,
                                                        {.Uniforms = 2, .WriteBuffers = 1, .ThreadsX = 64}));
        background =
            ComputePipeline::Create(Shader::Load("11_Compute/Shaders/Background.comp.spv", ShaderStage::Compute,
                                                 {.Uniforms = 1, .WriteTargets = 1, .ThreadsX = 8, .ThreadsY = 8}));
        PipelineInfo info;
        info.Vertex = Shader::Load("11_Compute/Shaders/Particles.vert.spv", ShaderStage::Vertex,
                                   {.Uniforms = 1, .StorageBuffers = 1});
        info.Fragment = Shader::Load("11_Compute/Shaders/Particles.frag.spv", ShaderStage::Fragment);
        info.Targets = {TargetFormat::Rgba16F};
        info.Blending = Blend::Additive;
        particles = Pipeline::Create(info);
        info.Vertex = Shader::Fullscreen();
        info.Fragment = Shader::Load("11_Compute/Shaders/Composite.frag.spv", ShaderStage::Fragment, {.Samplers = 1});
        info.Targets = {TargetFormat::Rgba8};
        info.Blending = Blend::None;
        composite = Pipeline::Create(info);

        // The particles start in a ring; from now on only the GPU touches them.
        std::vector<Particle> start(Count);
        Random random{11};
        const Vec2 centre = Screen::Size() * 0.5f;
        for (Particle& p : start) {
            const Vec2 direction = random.Direction();
            p.Position = centre + direction * random.Range(60, 160);
            p.Velocity = Vec2(-direction.Y, direction.X) * random.Range(20, 60);
            const Color tint = Color::Hex(0xFFA030).Lerp(Color::Hex(0x40C0FF), random.Unit());
            p.Tint = {tint.R, tint.G, tint.B, 0.35f};
        }
        buffer.Upload(start);
    }

    void Render(Frame& frame) override {
        frame.BeginCompute({&buffer});
        frame.Bind(simulate);
        frame.Push(ShaderStage::Compute, 1, pull);
        frame.Dispatch((Count + 63) / 64);
        frame.End();

        frame.BeginCompute({}, {&scene});
        frame.Bind(background);
        frame.Dispatch((scene.Width() + 7) / 8, (scene.Height() + 7) / 8);
        frame.End();

        frame.Begin(scene); // keeps what the compute shader wrote
        frame.Bind(particles);
        frame.BindStorage(ShaderStage::Vertex, 0, buffer);
        frame.Draw(6, Count);
        frame.End();

        frame.Begin(frame.Screen());
        frame.BindTexture(0, scene);
        frame.Fullscreen(composite);
        frame.Draw2D();
        frame.End();
    }
};

struct ComputeExample : Game {
    void Update() override {
        if (Input::KeyPressed(Key::Escape)) Quit();
        const Vec2 mouse = Input::Mouse();
        const float strength = Input::MouseDown(MouseButton::Left) ? 1.0f : 0.0f;
        Services::Get<ComputeRenderer>().pull = {mouse.X, mouse.Y, strength, float(Count)};
    }

    void Draw() override {
        Text::Draw(std::format("{} particles moved by a compute shader. Hold the mouse to pull.", Count), {8, 8},
                   {.Shadow = Colors::Black});
    }
};

int main() {
    return Run<ComputeExample, ComputeRenderer>(
        {.Title = "BitMotor - 11 Compute", .Width = 640, .Height = 360, .IntegerScaling = false, .TickRate = 0});
}
