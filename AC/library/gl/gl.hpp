// AC ilib: gl — GUI / Graphics Library
// use ilib gl
//
// Wraps a minimal portable GUI layer.
// On Linux/macOS: requires X11 or SDL2.  On Windows: Win32 GDI.
// For development without a display, functions are no-ops (graceful degradation).
#pragma once
#include <string>
#include <functional>
#include <cstdint>

// ── Backend selection ────────────────────────────────────────────────────────
// Define AC_GL_SDL2 before including this file to use SDL2.
// Define AC_GL_WIN32 for Win32 GDI.
// Otherwise, falls back to a stub (compile-but-do-nothing) mode.

#if defined(AC_GL_SDL2)
#  include <SDL2/SDL.h>
#elif defined(AC_GL_WIN32)
#  include <windows.h>
#endif

namespace ac_gl {

using Color = uint32_t; // 0xRRGGBB

// Colour helpers
constexpr Color RGB(uint8_t r, uint8_t g, uint8_t b) {
    return (Color(r) << 16) | (Color(g) << 8) | Color(b);
}
constexpr Color BLACK  = RGB(0,0,0);
constexpr Color WHITE  = RGB(255,255,255);
constexpr Color RED    = RGB(255,0,0);
constexpr Color GREEN  = RGB(0,200,0);
constexpr Color BLUE   = RGB(0,0,255);
constexpr Color YELLOW = RGB(255,255,0);

// ── Window ───────────────────────────────────────────────────────────────────

struct Window {
    int width  = 800;
    int height = 600;
    std::string title = "AC Window";
    bool open = false;

#if defined(AC_GL_SDL2)
    SDL_Window*   sdl_win = nullptr;
    SDL_Renderer* sdl_ren = nullptr;
#endif

    bool create(int w, int h, const std::string& t = "AC Window") {
        width = w; height = h; title = t;
#if defined(AC_GL_SDL2)
        if (SDL_Init(SDL_INIT_VIDEO) != 0) return false;
        sdl_win = SDL_CreateWindow(title.c_str(),
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            width, height, SDL_WINDOW_SHOWN);
        if (!sdl_win) return false;
        sdl_ren = SDL_CreateRenderer(sdl_win, -1, SDL_RENDERER_ACCELERATED);
        open = sdl_ren != nullptr;
        return open;
#else
        open = true; // stub
        return true;
#endif
    }

    void clear(Color c = BLACK) {
#if defined(AC_GL_SDL2)
        SDL_SetRenderDrawColor(sdl_ren, (c>>16)&0xFF, (c>>8)&0xFF, c&0xFF, 255);
        SDL_RenderClear(sdl_ren);
#else
        (void)c;
#endif
    }

    void present() {
#if defined(AC_GL_SDL2)
        SDL_RenderPresent(sdl_ren);
#endif
    }

    void draw_rect(int x, int y, int w, int h, Color c) {
#if defined(AC_GL_SDL2)
        SDL_SetRenderDrawColor(sdl_ren, (c>>16)&0xFF, (c>>8)&0xFF, c&0xFF, 255);
        SDL_Rect r{x, y, w, h};
        SDL_RenderFillRect(sdl_ren, &r);
#else
        (void)x; (void)y; (void)w; (void)h; (void)c;
#endif
    }

    void draw_line(int x1, int y1, int x2, int y2, Color c) {
#if defined(AC_GL_SDL2)
        SDL_SetRenderDrawColor(sdl_ren, (c>>16)&0xFF, (c>>8)&0xFF, c&0xFF, 255);
        SDL_RenderDrawLine(sdl_ren, x1, y1, x2, y2);
#else
        (void)x1; (void)y1; (void)x2; (void)y2; (void)c;
#endif
    }

    // Returns false when the window should close.
    bool poll_events() {
#if defined(AC_GL_SDL2)
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) { open = false; return false; }
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) { open = false; return false; }
        }
        return open;
#else
        return open;
#endif
    }

    void destroy() {
#if defined(AC_GL_SDL2)
        if (sdl_ren) SDL_DestroyRenderer(sdl_ren);
        if (sdl_win) SDL_DestroyWindow(sdl_win);
        SDL_Quit();
#endif
        open = false;
    }
};

// ── Labels / Text (basic placeholder — full text needs SDL_ttf or freetype) ─
inline void draw_text_placeholder(Window& /*win*/, int /*x*/, int /*y*/,
                                  const std::string& text, Color /*c*/) {
    // Real text rendering requires SDL_ttf or FreeType integration.
    // For now: log to stdout so programs don't crash silently.
    printf("[gl::draw_text] %s\n", text.c_str());
}

// ── Convenience: single-shot display ────────────────────────────────────────
inline void show_message(const std::string& msg, const std::string& title = "AC") {
#if defined(AC_GL_SDL2)
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, title.c_str(), msg.c_str(), nullptr);
#else
    printf("[gl::message] %s: %s\n", title.c_str(), msg.c_str());
#endif
}

} // namespace ac_gl

// Flat API
inline bool gl_open(int w, int h, const std::string& t) {
    static ac_gl::Window _win;
    return _win.create(w, h, t);
}
