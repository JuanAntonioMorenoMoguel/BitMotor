#include "Printing.h"
#include "Testing.h"

#include "BitMotor/Core/Screen.h"
#include "BitMotor/Render/CustomRenderer.h"
#include "BitMotor/Render/Gpu.h"
#include "Render/DebugPanelInternal.h"
#include "Render/GpuInternal.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <vector>

using namespace BitMotor;

// The sandbox of the tests has no GPU: these check what Gpu does before it touches one.

namespace {

// The smallest stream that passes for SPIR-V: its magic number and four more words.
std::vector<std::uint8_t> FakeSpirv() {
    std::vector<std::uint8_t> code(20, 0);
    code[0] = 0x03, code[1] = 0x02, code[2] = 0x23, code[3] = 0x07;
    return code;
}

} // namespace

TEST(GpuGlobalsAreStd140Sized) {
    CHECK_EQ(int(sizeof(Gpu::Globals)), 256);
    CHECK_EQ(int(offsetof(Gpu::Globals, Width)), 16);
    CHECK_EQ(int(offsetof(Gpu::Globals, MouseX)), 32);
    CHECK_EQ(int(offsetof(Gpu::Globals, Values)), 48);
}

TEST(GpuSetGlobalReachesTheShaders) {
    Gpu::SetGlobal(3, {1, 2, 3, 4});
    CHECK_EQ(Gpu::Global(3), Vec4(1, 2, 3, 4));
    const Gpu::Globals& globals = Gpu::CurrentGlobals();
    CHECK(globals.Values[3] == Vec4(1, 2, 3, 4));
    Detail::UpdateGlobals(0.5);
    CHECK(globals.Values[3] == Vec4(1, 2, 3, 4)); // kept from frame to frame
    Gpu::SetGlobal(13, {9, 9, 9, 9});             // out of range: ignored
    CHECK_EQ(Gpu::Global(13), Vec4());
    Gpu::SetGlobal(3, {});
}

TEST(GpuGlobalsCountRealTime) {
    const float before = Gpu::CurrentGlobals().RealTime;
    Detail::UpdateGlobals(0.25);
    CHECK_NEAR(Gpu::CurrentGlobals().RealTime - before, 0.25f, 1e-4f);
}

TEST(ShaderFromMemoryWantsSpirv) {
    const std::vector<std::uint8_t> code = FakeSpirv();
    Shader vertex = Shader::FromMemory(code, ShaderStage::Vertex, {.Uniforms = 2});
    CHECK(bool(vertex));
    CHECK(vertex.Stage() == ShaderStage::Vertex);
    CHECK_EQ(vertex.Internal()->Resources.Uniforms, 2);
    CHECK(vertex.Internal()->Code == code);

    std::vector<std::uint8_t> text = {'#', 'v', 'e',  'r', 's', 'i', 'o', 'n', ' ', '4',
                                      '5', '0', '\n', 'v', 'o', 'i', 'd', ' ', 'm', 'a'};
    CHECK(!Shader::FromMemory(text, ShaderStage::Fragment)); // GLSL, not compiled
    std::vector<std::uint8_t> odd = FakeSpirv();
    odd.push_back(0);
    CHECK(!Shader::FromMemory(odd, ShaderStage::Fragment)); // not whole words
    CHECK(!Shader::Load("no/such/shader.spv", ShaderStage::Fragment));
}

TEST(ShaderFullscreenIsBuiltin) {
    Shader a = Shader::Fullscreen(), b = Shader::Fullscreen();
    CHECK(bool(a));
    CHECK(a.Stage() == ShaderStage::Vertex);
    CHECK(a.Internal()->Builtin);
    CHECK(a.Internal() == b.Internal());
}

TEST(PipelineNeedsBothStagesAndAGpu) {
    const std::vector<std::uint8_t> code = FakeSpirv();
    Shader vertex = Shader::FromMemory(code, ShaderStage::Vertex);
    Shader fragment = Shader::FromMemory(code, ShaderStage::Fragment);
    PipelineInfo info;
    info.Vertex = vertex;
    CHECK(!Pipeline::Create(info)); // no fragment shader
    info.Vertex = fragment;
    info.Fragment = vertex;
    CHECK(!Pipeline::Create(info)); // stages swapped
    info.Vertex = vertex;
    info.Fragment = fragment;
    info.Targets.clear();
    CHECK(!Pipeline::Create(info));
    info.Targets = {TargetFormat::Rgba16F};
    CHECK(!Pipeline::Create(info)); // no GPU in the tests
}

TEST(BufferUploadsWaitInOrder) {
    Buffer buffer = Buffer::Create(BufferUsage::Storage, 64);
    CHECK_EQ(int(buffer.Size()), 64);
    CHECK(buffer.Usage() == BufferUsage::Storage);
    const float values[4] = {1, 2, 3, 4};
    buffer.Upload(values, sizeof(values));
    buffer.Upload(values, 8, 32);
    const auto& pending = buffer.Internal()->Pending;
    CHECK_EQ(int(pending.size()), 2);
    CHECK_EQ(int(pending[1].Offset), 32);
    CHECK_EQ(int(pending[1].Bytes.size()), 8);
    float back[4];
    std::memcpy(back, pending[0].Bytes.data(), sizeof(back));
    CHECK_EQ(back[2], 3.0f);
    CHECK(!buffer.Readback(back, sizeof(back))); // no GPU to read from
}

TEST(BufferGrowsToFit) {
    Buffer buffer = Buffer::Create(BufferUsage::Vertex, 100);
    std::vector<std::uint8_t> big(300, 7);
    buffer.Upload(big);
    CHECK_EQ(int(buffer.Size()), 400); // 100, doubled until it fits
    buffer.Upload(nullptr, 10);        // nothing to upload
    CHECK_EQ(int(buffer.Internal()->Pending.size()), 1);
    CHECK_EQ(int(Buffer::Create(BufferUsage::Index, 0).Size()), 16); // never empty
}

TEST(TargetsFollowTheScreenUntilCreated) {
    Target fixed = Target::Create(64, 32, TargetFormat::R32F, Filter::Nearest);
    CHECK_EQ(fixed.Width(), 64);
    CHECK_EQ(fixed.Height(), 32);
    CHECK(fixed.Format() == TargetFormat::R32F);
    CHECK(fixed.GetFilter() == Filter::Nearest);
    CHECK(!Target::Create(0, 10));
    CHECK(!fixed.Save("never.png")); // not on a GPU

    Target half = Target::Create(TargetFormat::Rgba16F, 0.5f);
    const Vec2 screen = Screen::Size();
    const int expected = std::max(1, int(std::lround(screen.X * 0.5f)));
    CHECK_EQ(half.Width(), expected);
    CHECK(half == half);
    CHECK(!(half == fixed));
}

TEST(PingPongSwaps) {
    PingPong trails = PingPong::Create(TargetFormat::Rgba16F);
    Target first = trails.Current();
    CHECK(first == trails.A);
    CHECK(trails.Previous() == trails.B);
    trails.Swap();
    CHECK(trails.Current() == trails.B);
    CHECK(trails.Previous() == first);
}

TEST(FrameIgnoresWorkOutsideAPass) {
    Detail::TargetData screenData;
    screenData.External = true;
    Target screen;
    Detail::TargetAccess::Wrap(screen, screenData);
    Frame frame(nullptr, &screen);
    CHECK(frame.Screen() == screen);
    Buffer buffer = Buffer::Create(BufferUsage::Storage, 16);
    frame.BindStorage(ShaderStage::Vertex, 0, buffer);
    frame.Draw(3);
    frame.Push(ShaderStage::Fragment, 0, Gpu::CurrentGlobals()); // block 0 is BitMotor's: refused
    frame.Draw2D();                                              // no pass: a warning
    frame.End();
    CHECK(frame.NativePass() == nullptr);
    CHECK(buffer.Internal()->Native == nullptr); // nothing reached the GPU
}

TEST(CustomRendererLivesWithTheGame) {
    struct Counting : CustomRenderer {
        int Inits = 0, Shutdowns = 0;
        void Init() override { ++Inits; }
        void Render(Frame&) override {}
        void Shutdown() override { ++Shutdowns; }
    } renderer;
    CHECK(Detail::ActiveRenderer() == nullptr);
    Detail::UseCustomRenderer(&renderer);
    CHECK(Detail::ActiveRenderer() == &renderer);
    CHECK_EQ(renderer.Inits, 1);
    Detail::ShutdownCustomRenderer();
    CHECK_EQ(renderer.Shutdowns, 1);
    CHECK(Detail::ActiveRenderer() == nullptr);
    Detail::ShutdownCustomRenderer(); // twice: nothing
    CHECK_EQ(renderer.Shutdowns, 1);
}

TEST(DebugPanelTogglesWithF1) {
    CHECK(!Detail::DebugPanelOpen());
    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.scancode = SDL_SCANCODE_F1;
    CHECK(!Detail::DebugPanelEvent(event)); // without ImGui running it takes nothing
    CHECK(Detail::DebugPanelOpen());
    event.key.repeat = true;
    Detail::DebugPanelEvent(event); // a held key does not toggle it again
    CHECK(Detail::DebugPanelOpen());
    event.key.repeat = false;
    Detail::DebugPanelEvent(event);
    CHECK(!Detail::DebugPanelOpen());
    bool filled = false;
    Detail::SetDebugPanelOpen(true);
    Detail::BuildDebugPanel([&] { filled = true; });
    CHECK(!filled); // not started (no window)
    Detail::SetDebugPanelOpen(false);
}
