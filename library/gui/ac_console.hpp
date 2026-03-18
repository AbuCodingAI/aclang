#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>
#include <iostream>
#include "ac_error.hpp"

// Obj.sidebar — console sidebar covering 1/3 of screen
// sidebar.config item=console
// sidebar.region = left
// sidebar.interactive = true
// sidebar.display $message$
// sidebar.ask($prompt$)
// sidebar.getinput

class ACConsole {
public:
    enum class Region { LEFT, RIGHT, TOP, BOTTOM };

    Region region      = Region::LEFT;
    bool   interactive = true;
    int    screenW = 1280, screenH = 720;
    int    x = 0, y = 0, w = 0, h = 0;

    SDL_Color bgColor   = {30,  30,  30,  220};
    SDL_Color textColor = {200, 255, 200, 255};

    TTF_Font* font = nullptr;
    std::vector<std::string> lines;
    std::string inputBuffer;
    bool waitingForInput = false;

    void init(SDL_Renderer* /*renderer*/, int sw, int sh, Region r = Region::LEFT) {
        screenW = sw; screenH = sh; region = r;
        TTF_Init();
        // Try common system fonts
        const char* fonts[] = {
            "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
            "/usr/share/fonts/truetype/ubuntu/UbuntuMono-R.ttf",
            nullptr
        };
        for (int i = 0; fonts[i]; i++) {
            font = TTF_OpenFont(fonts[i], 14);
            if (font) break;
        }
        recalcBounds();
    }

    void recalcBounds() {
        switch (region) {
            case Region::LEFT:
                x = 0; y = 0; w = screenW / 3; h = screenH; break;
            case Region::RIGHT:
                x = screenW * 2 / 3; y = 0; w = screenW / 3; h = screenH; break;
            case Region::TOP:
                x = 0; y = 0; w = screenW; h = screenH / 3; break;
            case Region::BOTTOM:
                x = 0; y = screenH * 2 / 3; w = screenW; h = screenH / 3; break;
        }
    }

    // sidebar.display $message$
    void display(const std::string& msg) {
        lines.push_back(msg);
        std::cout << "[Console] " << msg << "\n";
    }

    // sidebar.ask($prompt$)
    void ask(const std::string& prompt) {
        display(prompt);
        waitingForInput = true;
        inputBuffer.clear();
    }

    // sidebar.getinput — returns current input (call after ask + user typed + Enter)
    std::string getInput() const { return inputBuffer; }

    // Handle SDL text input events for interactive console
    bool handleEvent(const SDL_Event& e) {
        if (!interactive || !waitingForInput) return false;
        if (e.type == SDL_TEXTINPUT) {
            inputBuffer += e.text.text;
            return true;
        }
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_RETURN) {
            display("> " + inputBuffer);
            waitingForInput = false;
            return true;
        }
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_BACKSPACE) {
            if (!inputBuffer.empty()) inputBuffer.pop_back();
            return true;
        }
        return false;
    }

    void draw(SDL_Renderer* renderer) {
        // Background panel
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
        SDL_Rect panel = {x, y, w, h};
        SDL_RenderFillRect(renderer, &panel);

        if (!font) return;

        int lineH = 18, padding = 6;
        int maxLines = (h - padding * 2) / lineH;
        int start = (int)lines.size() > maxLines ? (int)lines.size() - maxLines : 0;

        for (int i = start; i < (int)lines.size(); i++) {
            SDL_Surface* surf = TTF_RenderText_Blended(font, lines[i].c_str(), textColor);
            if (!surf) continue;
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            SDL_Rect dst = {x + padding, y + padding + (i - start) * lineH, surf->w, surf->h};
            SDL_RenderCopy(renderer, tex, nullptr, &dst);
            SDL_DestroyTexture(tex);
            SDL_FreeSurface(surf);
        }

        // Input line
        if (waitingForInput && font) {
            std::string prompt = "> " + inputBuffer + "_";
            SDL_Surface* surf = TTF_RenderText_Blended(font, prompt.c_str(), {255, 255, 100, 255});
            if (surf) {
                SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                int iy = y + h - lineH - padding;
                SDL_Rect dst = {x + padding, iy, surf->w, surf->h};
                SDL_RenderCopy(renderer, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
                SDL_FreeSurface(surf);
            }
        }
    }

    void destroy() {
        if (font) TTF_CloseFont(font);
        TTF_Quit();
    }
};
