#include "BitMotor/Core/Game.h"

#include "BitMotor/Core/Log.h"
#include "Core/CommandLine.h"

#include <SDL3/SDL.h>

#include <charconv>

namespace BitMotor {

namespace {

bool QuitRequested = false;

// `--frames N`: quit after N frames (0 = never).
long long MaxFrames() {
    auto args = Detail::CommandLine();
    for (size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] != "--frames") continue;
        long long frames = 0;
        const std::string& value = args[i + 1];
        auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), frames);
        if (error == std::errc() && end == value.data() + value.size() && frames > 0) return frames;
        Log::Warn("--frames needs a positive number, got '{}'", value);
    }
    return 0;
}

} // namespace

void Quit() { QuitRequested = true; }

namespace Detail {

int Run(const Config& config, Game& game) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        Log::Error("SDL_Init: {}", SDL_GetError());
        return 1;
    }
    SDL_Window* window = SDL_CreateWindow(config.Title.c_str(), config.WindowWidth, config.WindowHeight,
                                          SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window) {
        Log::Error("SDL_CreateWindow: {}", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    // Provisional: SDL's 2D renderer only clears the screen until Renderer2D arrives in phase 3.
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        Log::Error("SDL_CreateRenderer: {}", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderVSync(renderer, 1);

    const long long maxFrames = MaxFrames();
    QuitRequested = false;
    game.Start();

    long long frame = 0;
    while (!QuitRequested) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) QuitRequested = true;
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) QuitRequested = true;
        }
        SDL_SetRenderDrawColor(renderer, 24, 26, 36, 255);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
        if (maxFrames > 0 && ++frame >= maxFrames) QuitRequested = true;
    }

    game.Stop();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

} // namespace Detail

} // namespace BitMotor
