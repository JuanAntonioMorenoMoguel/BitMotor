# BitMotor

A C++20 library to make simple games. It unifies nyiengilib and is the base that ProjectEGIDA, MONOCHROME and
RogueInvasion will migrate to. The full design (modules, API and roadmap) is in [docs/BitMotor.md](docs/BitMotor.md).

> **Status: the engine is complete.** Core (phase 1), architecture (2: Services, Scenes, Managers, Tasks, Settings),
> 2D rendering (3: SDL_GPU, sprites, atlas, camera, lighting, particles), text and UI (4: fonts, Rich text,
> Typewriter, translations with `Tr`, shapes and immediate widgets), audio (5: sounds from files or
> functions, music with crossfades, procedural music and BitMusic songs, see [docs/BitMusic.md](docs/BitMusic.md)),
> local Platform (6: achievements, stats, local leaderboards, DLC folders), custom renderers (7: `CustomRenderer`
> with its own GLSL shaders, pipelines, buffers and targets on SDL_GPU, compute shaders, hot reload of shaders in
> Debug, `BitMotor_Shaders` in CMake, and the debug panel with Dear ImGui: F1 or `--debug`), basic 3D and the
> extras (14: TrueType fonts with `Font::Load`, `RenderTexture`, `UI::Dropdown`, `UI::TextField` and the `Wipe` and
> `Slide` scene transitions; see `examples/13_Extras`).
> Not included: Steam, local multiplayer and online features.

## Drawing your own way

A game that needs its own shaders gives `Run` a second type, a `CustomRenderer`; BitMotor keeps the window, the
scaling, screenshots, scene fades and the debug panel. See `examples/10_CustomRenderer` and the comments in
`include/BitMotor/Render/CustomRenderer.h` and `Gpu.h` (not included by `BitMotor.h`). In the game's CMake:

```cmake
BitMotor_Prepare(MyGame COPY Assets)
BitMotor_Shaders(MyGame shaders)   # shaders/*.vert, *.frag -> shaders/<name>.<stage>.spv next to the exe
```

Shader resources follow SDL_GPU's sets: in the vertex stage set 0 holds samplers and storage buffers and set 1
the uniform blocks; in the fragment stage, sets 2 and 3. Uniform block 0 of both is `Gpu::Globals` (time, screen
size, mouse and `Gpu::SetGlobal` values); the game's own blocks are 1 to 3 (`Frame::Push`).

## Building

Needs CMake 3.25+, a C++20 compiler (Visual Studio 2022, GCC 13 or Clang 16+) and
[vcpkg](https://github.com/microsoft/vcpkg) with the `VCPKG_ROOT` environment variable pointing at it.
Dependencies are installed automatically from `vcpkg.json`.

```sh
# Windows (Visual Studio 2022)
cmake --preset windows
cmake --build --preset windows-debug
ctest --preset windows-debug

# Linux (Ninja)
cmake --preset linux
cmake --build --preset linux-debug
ctest --preset linux-debug
```

The build goes to `build/<preset>/`, the executables to `build/<preset>/bin/<Config>/`. On Linux, SDL3 needs the
X11/Wayland development packages:

```bash
sudo apt-get install -y ninja-build pkg-config autoconf automake autoconf-archive libtool libltdl-dev \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev \
  libxtst-dev libxkbcommon-dev libwayland-dev wayland-protocols libegl1-mesa-dev \
  libgl1-mesa-dev libgles2-mesa-dev libdrm-dev libgbm-dev libibus-1.0-dev libdbus-1-dev \
  libudev-dev libasound2-dev libpulse-dev libpipewire-0.3-dev libdecor-0-dev
```

| Option | Default | What it does |
|---|---|---|
| `BITMOTOR_TESTS` | ON as top project, OFF as subdirectory | Builds `BitMotorTests` and registers it (and the examples) with ctest |
| `BITMOTOR_EXAMPLES` | ON as top project, OFF as subdirectory | Builds `examples/` |
| `BITMOTOR_WARNINGS_AS_ERRORS` | OFF (ON in the CI) | Warnings in BitMotor's own code fail the build |
| `BITMOTOR_STEAM_SDK` | empty | Path to the Steamworks SDK (used from phase 6) |

## Using it in a game

```cmake
set(BITMOTOR_PATH "${CMAKE_SOURCE_DIR}/../BitMotor" CACHE PATH "")
add_subdirectory(${BITMOTOR_PATH} BitMotor)
add_executable(MyGame src/Main.cpp)
target_link_libraries(MyGame PRIVATE BitMotor::BitMotor)
BitMotor_Prepare(MyGame COPY Data Assets)
```

Copy BitMotor's `vcpkg.json` dependencies into the game's own `vcpkg.json`.

`BitMotor_Prepare(target [COPY folders...])` leaves the executable ready to run:

- links BitMotor if the game did not;
- puts it in `<build>/bin` unless the game chose another folder;
- on Windows, hides the console in Release (keeping a normal `main()`) and copies the dependency DLLs next to it;
- copies each `COPY` folder next to it, and defines `BITMOTOR_SOURCE_DIR` so that resources can be read from the
  source tree during development.

```cpp
#include <BitMotor/BitMotor.h>

using namespace BitMotor;

struct MyGame : Game {
    void Start() override { Log::Info("Hello from BitMotor {}", Version()); }
};

int main() { return Run<MyGame>({.Title = "My game"}); }
```

## Layout

| Path | Contents |
|---|---|
| `include/BitMotor/` | Public headers. `BitMotor.h` includes everything; one folder per module (`Core/`, ...) |
| `src/` | Implementation, one folder per module. Third-party headers (SDL3...) never leak into the public ones |
| `cmake/BitMotor.cmake` | `BitMotor_Prepare`, `BitMotor_Shaders` (a game's GLSL to SPIR-V) and internal helpers |
| `cmake/CompileShaders.cmake` | Turns BitMotor's own shaders (`src/Render/Shaders`) into `Compiled.h`, which is committed. Only needed after editing a shader: `cmake -P cmake/CompileShaders.cmake` (glslc or glslangValidator from the Vulkan SDK) |
| `tests/` | Internal tests with a tiny framework (`TEST`, `CHECK`, `CHECK_EQ`, `CHECK_NEAR`). `BitMotorTests <filter>` runs only the tests whose name contains the filter |
| `examples/` | One folder per example with a single `Main.cpp`. They also run in ctest with `--frames 60` |
| `docs/` | Design document, and the BitMusic song format (`BitMusic.md`) |

## Conventions

English code. PascalCase for types, functions, methods, fields and enum values; camelCase for parameters.
Everything lives in `namespace BitMotor`. Formatting with `.clang-format` (4 spaces, 120 columns).

## Dependencies

| Package | Version (vcpkg baseline `5d68747a`) | Used by |
|---|---|---|
| SDL3 | 3.4.20 | Window, events and the GPU (SDL_GPU on Vulkan) |
| stb | 2024-07-29 | Reading and writing images (`Texture`, screenshots, the window icon) |
| nlohmann-json | 3.12.0 | Reading and writing JSON in `Data` (never visible in BitMotor's headers) |
| miniaudio | 0.11 | The audio device, and decoding WAV, FLAC and MP3 (compiled inside BitMotor) |
| stb (stb_vorbis) | 2024-07-29 | Decoding OGG Vorbis, through miniaudio |
| imgui (sdl3-binding, sdlgpu3-binding) | 1.93.0 | The debug panel (`Game::DebugPanel`, `Scene::DebugPanel`), public so games can call `ImGui::` |

More dependencies are added in the phase that needs them.
