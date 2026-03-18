#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <string>
#include <filesystem>
#include "ac_error.hpp"

namespace fs = std::filesystem;

// AC.Search($file.png$) — searches workspace for a file
inline std::string ac_search(const std::string& filename) {
    for (auto& entry : fs::recursive_directory_iterator(".")) {
        if (entry.path().filename() == filename)
            return entry.path().string();
    }
    return ""; // not found — caller should check and fall back to config shape
}

class ACSprite {
public:
    SDL_Texture* texture = nullptr;
    int w = 0, h = 0;
    bool loaded = false;

    // Object.Sprite = AC.Search($file.png$)
    bool load(SDL_Renderer* renderer, const std::string& path) {
        if (path.empty()) return false;
        SDL_Surface* surf = IMG_Load(path.c_str());
        if (!surf) return false;
        texture = SDL_CreateTextureFromSurface(renderer, surf);
        w = surf->w; h = surf->h;
        SDL_FreeSurface(surf);
        loaded = (texture != nullptr);
        return loaded;
    }

    void draw(SDL_Renderer* renderer, int x, int y, int dw = -1, int dh = -1) {
        if (!texture) return;
        SDL_Rect dst = {x, y, dw < 0 ? w : dw, dh < 0 ? h : dh};
        SDL_RenderCopy(renderer, texture, nullptr, &dst);
    }

    void destroy() {
        if (texture) SDL_DestroyTexture(texture);
        texture = nullptr; loaded = false;
    }
};
