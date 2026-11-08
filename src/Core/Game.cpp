#include "BitMotor/Core/Game.h"

#include "Audio/AudioInternal.h"
#include "BitMotor/Core/Args.h"
#include "BitMotor/Core/Events.h"
#include "BitMotor/Core/Log.h"
#include "BitMotor/Core/Screen.h"
#include "BitMotor/Core/Settings.h"
#include "BitMotor/Core/Window.h"
#include "BitMotor/Version.h"
#include "Core/DisplayInternal.h"
#include "Core/EventsInternal.h"
#include "Core/FilesInternal.h"
#include "Core/InputInternal.h"
#include "Core/ScenesInternal.h"
#include "Core/ServicesInternal.h"
#include "Core/SettingsInternal.h"
#include "Core/TasksInternal.h"
#include "Core/TimeInternal.h"
#include "Platform/PlatformInternal.h"
#include "Render/DebugPanelInternal.h"
#include "Render/GpuInternal.h"
#include "Render/ImageInternal.h"
#include "Render/RendererInternal.h"
#include "UI/UiInternal.h"

#include <SDL3/SDL.h>

#include <cctype>
#include <chrono>
#include <filesystem>

namespace BitMotor {

namespace {

bool QuitRequested = false;

// `--frames N`: quit after N frames (0 = never).
long long MaxFrames() {
    int frames = Args::Get("--frames", 0);
    if (frames < 0) Log::Warn("--frames needs a positive number, got {}", frames);
    return frames > 0 ? frames : 0;
}

// Window events become BitMotor events; keyboard, mouse and gamepads go to Input.
void PollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        const bool panelTookIt = Detail::DebugPanelEvent(event); // the panel sees every event first
        switch (event.type) {
        case SDL_EVENT_QUIT:
            QuitRequested = true;
            break;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            Events::Emit(WindowResized{event.window.data1, event.window.data2});
            break;
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            Detail::SetFocused(true);
            Events::Emit(FocusChanged{true});
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            Detail::SetFocused(false);
            Events::Emit(FocusChanged{false});
            break;
        default:
            if (!panelTookIt) Detail::HandleInputEvent(event);
            break;
        }
    }
}

// Config::Icon on the window. A missing or broken file only logs an error.
void SetIcon(SDL_Window* window, const std::string& icon) {
    if (icon.empty()) return;
    Detail::Image image = Detail::LoadImage(icon);
    if (!image) return;
    SDL_Surface* surface =
        SDL_CreateSurfaceFrom(image.Width, image.Height, SDL_PIXELFORMAT_RGBA32, image.Pixels.data(), image.Width * 4);
    if (!surface || !SDL_SetWindowIcon(window, surface)) Log::Warn("Window icon {}: {}", icon, SDL_GetError());
    SDL_DestroySurface(surface);
}

// What the system shows for the app (audio mixers, the about box on some desktops) and its id.
void SetAppMetadata(const Config& config) {
    std::string id;
    for (const std::string* part : {&config.Organization, &config.Title}) {
        if (!id.empty()) id += '.';
        for (char c : *part)
            if (std::isalnum(static_cast<unsigned char>(c))) id += char(std::tolower(static_cast<unsigned char>(c)));
    }
    SDL_SetAppMetadata(config.Title.c_str(), config.Version.empty() ? nullptr : config.Version.c_str(), id.c_str());
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_CREATOR_STRING, config.Organization.c_str());
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_TYPE_STRING, "game");
}

} // namespace

void Quit() { QuitRequested = true; }

namespace Detail {

int Run(const Config& config, Game& game, void (*setup)()) {
    SetAppMetadata(config);
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        Log::Error("SDL_Init: {}", SDL_GetError());
        return 1;
    }
    // The save folder and the player's settings first, so the window opens in the mode they chose.
    SetIdentity(config.Organization, config.Title);
    SetSettingsDefaults(config.Fullscreen, config.VSync);
    PreloadSettings();
    const bool fullscreen = Settings::Get().Fullscreen, vsync = Settings::Get().VSync;

    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (fullscreen) flags |= SDL_WINDOW_FULLSCREEN;
    SDL_Window* window = SDL_CreateWindow(config.Title.c_str(), config.WindowWidth, config.WindowHeight, flags);
    if (!window) {
        Log::Error("SDL_CreateWindow: {}", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SetIcon(window, config.Icon);
    AttachDisplay(window, config.Width, config.Height, vsync, config.IntegerScaling, config.NativeResolution);
    if (!StartRenderer(window, vsync, config.LinearColor)) {
        AttachDisplay(nullptr, 0, 0, vsync, config.IntegerScaling);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    StartDebugPanel(window);
    SetDebugPanelOpen(Args::Has("--debug"));

    ResetTime(config.TickRate);
    StartInput();
    StartAudio();
    const long long maxFrames = MaxFrames();
    // `--screenshot file.png`: the last frame of a `--frames N` run (relative to the current folder).
    const std::string lastShot = Args::Get("--screenshot");
    if (!lastShot.empty() && maxFrames == 0) Log::Warn("--screenshot needs --frames N");
    QuitRequested = false;
    if (setup) setup();
    Log::Info("{}{}{} (BitMotor {})", config.Title, config.Version.empty() ? "" : " ", config.Version, Version());
    game.Start();
    ApplySceneChanges(); // the first scene enters before the first Update

    long long frame = 0;
    auto last = std::chrono::steady_clock::now();
    while (!QuitRequested) {
        auto now = std::chrono::steady_clock::now();
        double realDt = std::chrono::duration<double>(now - last).count();
        last = now;
        // `--frames N` runs are the same every time, however slow the machine: each frame is 1/60 s.
        if (maxFrames > 0) realDt = 1.0 / 60.0;

        PollEvents();
        InputFrame();
        UpdateWatches(realDt);

        int updates = BeginFrame(realDt);
        for (int i = 0; i < updates && !QuitRequested; ++i) {
            BeginUpdate();
            InputTick();
            UpdateServices();
            UpdateTasks();
            game.Update();
            UpdateScenes();
        }
        FlushEvents();
        FlushPlatform();
        UpdateAudio(realDt);

        BeginDraw();
        BeginFrame2D();
        BeginUi();
        DrawScenes();
        game.Draw();
        EndUi();
        BuildDebugPanel([&game] {
            game.DebugPanel();
            DebugPanelScenes();
        });
        if (!lastShot.empty() && maxFrames > 0 && frame + 1 == maxFrames)
            Screen::Screenshot(std::filesystem::absolute(std::filesystem::path(lastShot)));
        EndFrame2D();

        if (maxFrames > 0 && ++frame >= maxFrames) QuitRequested = true;
    }

    ShutdownScenes();
    ClearTasks();
    game.Stop();
    ShutdownCustomRenderer(); // before its service goes
    StopServices();           // after the game's Stop, which may still use them
    FlushEvents();
    FlushPlatform();
    ResetPlatform();
    StopInput();
    StopAudio();
    ResetAudio();
    StopDebugPanel();
    StopRenderer();
    AttachDisplay(nullptr, 0, 0, vsync, config.IntegerScaling);
    ForgetSettings();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

} // namespace Detail

} // namespace BitMotor
