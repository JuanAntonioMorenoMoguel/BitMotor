# BitMotor

A C++20 library to make simple games. It unifies nyiengilib and is the base that ProjectEGIDA, MONOCHROME and
RogueInvasion will migrate to. The full design (modules, API and roadmap) is in [docs/BitMotor.md](docs/BitMotor.md).

> **Status: phase 1 in progress.** Done: project skeleton (phase 0), Math and Colors. Next: Args, Files, Data,
> Events, the game loop, Time, Window, Screen and Input. The modules arrive phase by phase (see the roadmap).

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
X11/Wayland development packages (see the list in `.github/workflows/ci.yml`).

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
| `cmake/BitMotor.cmake` | `BitMotor_Prepare` and internal helpers |
| `tests/` | Internal tests with a tiny framework (`TEST`, `CHECK`, `CHECK_EQ`, `CHECK_NEAR`). `BitMotorTests <filter>` runs only the tests whose name contains the filter |
| `examples/` | One folder per example with a single `Main.cpp`. They also run in ctest with `--frames 60` |
| `docs/` | Design document |

## Conventions

English code. PascalCase for types, functions, methods, fields and enum values; camelCase for parameters.
Everything lives in `namespace BitMotor`. Formatting with `.clang-format` (4 spaces, 120 columns).

## Dependencies

| Package | Version (vcpkg baseline `5d68747a`) | Used by |
|---|---|---|
| SDL3 | 3.4.20 | Window and events |

More dependencies are added in the phase that needs them.
