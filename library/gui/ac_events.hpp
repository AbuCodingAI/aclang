#pragma once
#include <SDL2/SDL.h>
#include <functional>
#include <unordered_map>
#include <string>

// configure event-listener
// on value=space -> jump(Character)

class ACEventListener {
public:
    using Callback = std::function<void()>;

    // on value=<key> -> callback
    void on(SDL_Keycode key, Callback cb) {
        keyBindings[key] = cb;
    }

    // Convenience: on value=space
    void onSpace(Callback cb) { on(SDLK_SPACE, cb); }
    void onEscape(Callback cb) { on(SDLK_ESCAPE, cb); }
    void onEnter(Callback cb) { on(SDLK_RETURN, cb); }

    // Call this each frame with the SDL event
    // Returns false if quit event received
    bool process(const SDL_Event& e) {
        if (e.type == SDL_QUIT) return false;
        if (e.type == SDL_KEYDOWN) {
            auto it = keyBindings.find(e.key.keysym.sym);
            if (it != keyBindings.end()) it->second();
        }
        return true;
    }

    // Poll all events this frame — returns false on quit
    bool pollAll() {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (!process(e)) return false;
        }
        return true;
    }

private:
    std::unordered_map<SDL_Keycode, Callback> keyBindings;
};
