// 10_CustomRenderer: a game that draws its own way, on the GPU, with its own shaders (Shaders/*.vert, *.frag,
// compiled to SPIR-V by BitMotor_Shaders in CMake).
// - 600 circles bounce around. They live in a storage buffer and are drawn in one instanced draw: the
//   vertex shader builds each quad from gl_VertexIndex, the fragment shader cuts the circle by its distance.
// - They leave trails: two HDR targets that swap every frame (PingPong). Each frame starts with last frame,
//   a little darker (Fade.frag, with its own uniform block), and adds the circles on top.
// - Composite.frag brings the HDR trails onto the screen with a tone curve; its exposure is a global
//   (Gpu::SetGlobal) that every shader can read. BitMotor's 2D text is drawn on top with Frame::Draw2D.
// - Hold the mouse to pull the circles. F1 opens the debug panel (ImGui): the trails and the exposure.
// Esc quits.
#include <BitMotor/BitMotor.h>
#include <BitMotor/Render/CustomRenderer.h> // not in BitMotor.h: only for games that draw their own way

#include <imgui.h>

#include <cmath>
#include <format>
#include <vector>

using namespace BitMotor;

// As the shaders read it from the storage buffer (std430: 32 bytes).
struct Bubble {
    Vec2 Position;
    float Radius = 4;
    float Unused = 0;
    Vec4 Tint;
};
static_assert(sizeof(Bubble) == 32);

// The renderer: its pipelines, the circles' buffer and the trails. The game fills Circles every frame.
struct TrailsRenderer : CustomRenderer {
    Pipeline circles, fade, composite;
    Buffer buffer = Buffer::Create(BufferUsage::Storage, 1 << 16);
    PingPong trails = PingPong::Create(TargetFormat::Rgba16F);
    int count = 0;
    float keep = 0.92f; // how much of the trails stays each frame

    void Init() override {
        Shader circleVertex = Shader::Load("10_CustomRenderer/Shaders/Circles.vert.spv", ShaderStage::Vertex,
                                           {.Uniforms = 1, .StorageBuffers = 1});
        Shader circleFragment = Shader::Load("10_CustomRenderer/Shaders/Circles.frag.spv", ShaderStage::Fragment);
        Shader fadeFragment = Shader::Load("10_CustomRenderer/Shaders/Fade.frag.spv", ShaderStage::Fragment,
                                           {.Uniforms = 2, .Samplers = 1});
        Shader compositeFragment = Shader::Load("10_CustomRenderer/Shaders/Composite.frag.spv", ShaderStage::Fragment,
                                                {.Uniforms = 1, .Samplers = 1});
        PipelineInfo info;
        info.Vertex = circleVertex;
        info.Fragment = circleFragment;
        info.Targets = {TargetFormat::Rgba16F};
        info.Blending = Blend::Additive;
        circles = Pipeline::Create(info);

        info.Vertex = Shader::Fullscreen();
        info.Fragment = fadeFragment;
        info.Blending = Blend::None;
        fade = Pipeline::Create(info);

        info.Fragment = compositeFragment;
        info.Targets = {TargetFormat::Rgba8}; // the screen
        composite = Pipeline::Create(info);
    }

    void Upload(const std::vector<Bubble>& items) {
        buffer.Upload(items);
        count = int(items.size());
    }

    void Render(Frame& frame) override {
        // Last frame's trails, darker, then the circles added on top.
        frame.Begin(trails.Current(), Colors::Black);
        frame.BindTexture(0, trails.Previous());
        frame.Push(ShaderStage::Fragment, 1, Vec4(keep, 0, 0, 0));
        frame.Fullscreen(fade);
        frame.Bind(circles);
        frame.BindStorage(ShaderStage::Vertex, 0, buffer);
        frame.Draw(6, count);
        frame.End();

        // Onto the screen, and BitMotor's text over it.
        frame.Begin(frame.Screen());
        frame.BindTexture(0, trails.Current());
        frame.Fullscreen(composite);
        frame.Draw2D();
        frame.End();
        trails.Swap();
    }
};

struct CustomRendererExample : Game {
    std::vector<Bubble> circles;
    std::vector<Vec2> velocities;
    Random random{7};
    float exposure = 1.5f;

    void Start() override {
        const Vec2 size = Screen::Size();
        for (int i = 0; i < 600; ++i) {
            Bubble bubble;
            bubble.Position = {random.Range(0, size.X), random.Range(0, size.Y)};
            bubble.Radius = random.Range(2, 5);
            const Color tint = Color::Hex(0x30E0FF).Lerp(Color::Hex(0xFF40C0), random.Unit()); // cyan to pink
            bubble.Tint = {tint.R, tint.G, tint.B, 0.5f};
            circles.push_back(bubble);
            velocities.push_back(random.Direction() * random.Range(30, 120));
        }
    }

    void Update() override {
        if (Input::KeyPressed(Key::Escape)) Quit();
        const float dt = Time::Delta();
        const Vec2 size = Screen::Size();
        const bool pulling = Input::MouseDown(MouseButton::Left);
        const Vec2 mouse = Input::Mouse();
        for (std::size_t i = 0; i < circles.size(); ++i) {
            Vec2& p = circles[i].Position;
            Vec2& v = velocities[i];
            if (pulling) v += Normalize(mouse - p) * 400 * dt;
            p += v * dt;
            if (p.X < 0 || p.X > size.X) v.X = -v.X, p.X = std::clamp(p.X, 0.0f, size.X);
            if (p.Y < 0 || p.Y > size.Y) v.Y = -v.Y, p.Y = std::clamp(p.Y, 0.0f, size.Y);
        }
    }

    void Draw() override {
        Services::Get<TrailsRenderer>().Upload(circles);
        Gpu::SetGlobal(0, {exposure, 0, 0, 0});
        Text::Draw(std::format("{} circles, one draw. Hold the mouse to pull. F1: debug panel", circles.size()), {8, 8},
                   {.Shadow = Colors::Black});
    }

    void DebugPanel() override {
        auto& renderer = Services::Get<TrailsRenderer>();
        ImGui::SetNextWindowSize({280, 0}, ImGuiCond_FirstUseEver);
        ImGui::Begin("10_CustomRenderer");
        ImGui::SliderFloat("Trails", &renderer.keep, 0.0f, 0.99f);
        ImGui::SliderFloat("Exposure", &exposure, 0.2f, 5.0f);
        ImGui::Text("%.0f FPS", double(ImGui::GetIO().Framerate));
        ImGui::End();
    }
};

int main() {
    return Run<CustomRendererExample, TrailsRenderer>(
        {.Title = "BitMotor - 10 CustomRenderer", .Width = 640, .Height = 360, .IntegerScaling = false});
}
