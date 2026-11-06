#include "BitMotor/Core/Game.h"

#include "BitMotor/Core/Args.h"
#include "BitMotor/Core/Events.h"
#include "BitMotor/Core/Log.h"
#include "BitMotor/Core/Window.h"
#include "BitMotor/Version.h"
#include "Core/DisplayInternal.h"
#include "Core/EventsInternal.h"
#include "Core/FilesInternal.h"
#include "Core/InputInternal.h"
#include "Core/ServicesInternal.h"
#include "Core/TasksInternal.h"
#include "Core/TimeInternal.h"

#include <SDL3/SDL.h>

#include <chrono>

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
            break;
        }
    }
}

} // namespace

void Quit() { QuitRequested = true; }

namespace Detail {

int Run(const Config& config, Game& game) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        Log::Error("SDL_Init: {}", SDL_GetError());
        return 1;
    }
    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (config.Fullscreen) flags |= SDL_WINDOW_FULLSCREEN;
    SDL_Window* window = SDL_CreateWindow(config.Title.c_str(), config.WindowWidth, config.WindowHeight, flags);
    if (!window) {
        Log::Error("SDL_CreateWindow: {}", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    // Provisional: SDL's 2D renderer until Renderer2D arrives in phase 3.
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        Log::Error("SDL_CreateRenderer: {}", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderVSync(renderer, config.VSync ? 1 : 0);
    if (config.Width > 0 && config.Height > 0)
        SDL_SetRenderLogicalPresentation(renderer, config.Width, config.Height, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    AttachDisplay(window, renderer, config.Width, config.Height, config.VSync);

    SetIdentity(config.Organization, config.Title);
    ResetTime(config.TickRate);
    StartInput();
    const long long maxFrames = MaxFrames();
    QuitRequested = false;
    Log::Info("{}{}{} (BitMotor {})", config.Title, config.Version.empty() ? "" : " ", config.Version, Version());
    game.Start();

    long long frame = 0;
    auto last = std::chrono::steady_clock::now();
    while (!QuitRequested) {
        auto now = std::chrono::steady_clock::now();
        double realDt = std::chrono::duration<double>(now - last).count();
        last = now;

        PollEvents();
        UpdateWatches(realDt);

        int updates = BeginFrame(realDt);
        for (int i = 0; i < updates && !QuitRequested; ++i) {
            BeginUpdate();
            InputTick();
            UpdateServices();
            UpdateTasks();
            game.Update();
        }
        FlushEvents();

        BeginDraw();
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        game.Draw();
        SDL_RenderPresent(renderer);

        if (maxFrames > 0 && ++frame >= maxFrames) QuitRequested = true;
    }

    ClearTasks();
    game.Stop();
    StopServices(); // after the game's Stop, which may still use them
    FlushEvents();
    StopInput();
    AttachDisplay(nullptr, nullptr, 0, 0, config.VSync);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

} // namespace Detail

} // namespace BitMotor
