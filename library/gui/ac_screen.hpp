#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <string>
#include "ac_error.hpp"

// Background config modes
enum class BackgroundMode { COLOR, LIVEFEED };

struct Color { uint8_t r, g, b, a = 255; };

class ACScreen {
public:
    SDL_Window*   window   = nullptr;
    SDL_Renderer* renderer = nullptr;
    int width = 1280, height = 720;
    BackgroundMode bgMode = BackgroundMode::COLOR;
    Color bgColor = {0, 0, 0, 255};

    // Make Screen object + Screen.OBJECT resize WxH
    void init(int w, int h, const std::string& title = "AC App") {
        width = w; height = h;
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0)
            ac_raise_err(SDL_GetError());
        IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
        window = SDL_CreateWindow(title.c_str(),
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            width, height, SDL_WINDOW_SHOWN);
        if (!window) ac_raise_err(SDL_GetError());
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer) ac_raise_err(SDL_GetError());
    }

    // Background.config color=<name>
    void setColor(uint8_t r, uint8_t g, uint8_t b) {
        bgMode = BackgroundMode::COLOR;
        bgColor = {r, g, b, 255};
    }

    // Background.config mode=livefeed
    void setLivefeed() { bgMode = BackgroundMode::LIVEFEED; }

    void clear() {
        SDL_SetRenderDrawColor(renderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
        SDL_RenderClear(renderer);
    }

    void present() { SDL_RenderPresent(renderer); }

    void destroy() {
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window)   SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
    }
};
