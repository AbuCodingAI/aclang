// AC ilib: gl — GUI / Game Library (C++ header, wraps libacgl.so)
// use ilib gl
#pragma once
#include "gl_c.h"
#include <string>
#include <cstdint>

namespace ac_gl {

// ── Colour helpers ────────────────────────────────────────────────────────

struct Color { uint8_t r, g, b; };
constexpr Color BLACK  = {0,0,0};
constexpr Color WHITE  = {255,255,255};
constexpr Color RED    = {255,0,0};
constexpr Color GREEN  = {0,200,0};
constexpr Color BLUE   = {0,0,255};
constexpr Color YELLOW = {255,255,0};
constexpr Color CYAN   = {0,255,255};
constexpr Color MAGENTA= {255,0,255};

inline Color color(uint8_t r, uint8_t g, uint8_t b) { return {r,g,b}; }

} // namespace ac_gl

// ── Namespace object — AC-generated C++ uses gl.screen.create(...) etc. ──

struct _AcGlScreen {
    int create(int w, int h, const std::string& title = "AC") const {
        return ac_gl_screen_create(w, h, title.c_str());
    }
    void set_bg(uint8_t r, uint8_t g, uint8_t b) const { ac_gl_screen_set_bg(r,g,b); }
    void set_fps(int fps) const { ac_gl_screen_set_fps(fps); }
    int  w() const { return ac_gl_screen_w(); }
    int  h() const { return ac_gl_screen_h(); }
};

struct _AcGlObj {
    void create(const std::string& name) const                         { ac_gl_obj_create(name.c_str()); }
    void geometry(const std::string& n, int w, int h) const            { ac_gl_obj_geometry(n.c_str(),w,h); }
    void square(const std::string& n, int size) const                  { ac_gl_obj_square(n.c_str(),size); }
    void pos(const std::string& n, int x, int y) const                 { ac_gl_obj_pos(n.c_str(),x,y); }
    void color(const std::string& n, uint8_t r, uint8_t g, uint8_t b) const { ac_gl_obj_color(n.c_str(),r,g,b); }
    void velocity(const std::string& n, float vx, float vy) const      { ac_gl_obj_velocity(n.c_str(),vx,vy); }
    void set_speed(const std::string& n, float s) const                { ac_gl_obj_set_speed(n.c_str(),s); }
    void set_direction(const std::string& n, float deg) const          { ac_gl_obj_set_direction(n.c_str(),deg); }
    void speed_mult(const std::string& n, float m) const               { ac_gl_obj_speed_mult(n.c_str(),m); }
    void move_x(const std::string& n, int dx) const                    { ac_gl_obj_move_x(n.c_str(),dx); }
    void move_y(const std::string& n, int dy) const                    { ac_gl_obj_move_y(n.c_str(),dy); }
    int  x(const std::string& n) const { return ac_gl_obj_x(n.c_str()); }
    int  y(const std::string& n) const { return ac_gl_obj_y(n.c_str()); }
    int  w(const std::string& n) const { return ac_gl_obj_w(n.c_str()); }
    int  h(const std::string& n) const { return ac_gl_obj_h(n.c_str()); }
    void curveshape(const std::string& n, const std::string& expr) const { ac_gl_obj_curveshape(n.c_str(), expr.c_str()); }
    void vertex(const std::string& n, float vx, float vy) const          { ac_gl_obj_vertex(n.c_str(), vx, vy); }
    void to_draw(const std::string& n) const                              { ac_gl_obj_to_draw(n.c_str()); }
    void circle_fall(const std::string& n, float frac, const std::string& dir) const { ac_gl_obj_circle_fall(n.c_str(), frac, dir.c_str()); }
    bool circle_fell(const std::string& n, float frac=0.25f, const std::string& dir="right") const { return ac_gl_obj_circle_fell(n.c_str(), frac, dir.c_str()) != 0; }
    void set_spawn(const std::string& n) const                            { ac_gl_obj_set_spawn(n.c_str()); }
    void regen(const std::string& n) const                                { ac_gl_obj_regen(n.c_str()); }
    void animate(const std::string& n, const std::string& dir, float speed) const { ac_gl_obj_animate(n.c_str(), dir.c_str(), speed); }
};

struct _AcGlDraw {
    void create(const std::string& n) const                                              { ac_gl_draw_create(n.c_str()); }
    void curveshape(const std::string& n, const std::string& expr) const                 { ac_gl_draw_curveshape(n.c_str(), expr.c_str()); }
    void vertex(const std::string& n, float vx, float vy) const                          { ac_gl_draw_vertex(n.c_str(), vx, vy); }
    void line(const std::string& n, float x1, float y1, float x2, float y2,
              uint8_t r, uint8_t g, uint8_t b) const                                     { ac_gl_draw_line(n.c_str(),x1,y1,x2,y2,r,g,b); }
    void circle(const std::string& n, float cx, float cy, float radius,
                uint8_t r, uint8_t g, uint8_t b) const                                   { ac_gl_draw_circle(n.c_str(),cx,cy,radius,r,g,b); }
    void clear(const std::string& n) const                                               { ac_gl_draw_clear(n.c_str()); }
    void to_obj(const std::string& n) const                                              { ac_gl_draw_to_obj(n.c_str()); }
};

struct _AcGlHitbox {
    bool overlap(const std::string& a, const std::string& b) const {
        return ac_gl_hitbox_overlap(a.c_str(), b.c_str()) != 0;
    }
    bool boundary(const std::string& name) const {
        return ac_gl_hitbox_overlap_boundary(name.c_str()) != 0;
    }
    bool overlap_pattern(const std::string& name, const std::string& pat) const {
        return ac_gl_hitbox_overlap_pattern(name.c_str(), pat.c_str()) != 0;
    }
    bool many_overlap() const { return ac_gl_hitbox_many_overlap() != 0; }
};

struct _AcGlKey {
    bool pressed(const std::string& k) const      { return ac_gl_key_pressed(k.c_str()) != 0; }
    bool just_pressed(const std::string& k) const { return ac_gl_key_just_pressed(k.c_str()) != 0; }
};

struct _AcGlFrame {
    bool   begin()           const { return ac_gl_frame_begin() != 0; }
    void   update(float dt)  const { ac_gl_frame_update(dt); }
    void   render()          const { ac_gl_frame_render(); }
    void   end()             const { ac_gl_frame_end(); }
    float  delta()           const { return ac_gl_delta_time(); }
};

struct _AcGlNS {
    _AcGlScreen screen;
    _AcGlObj    obj;
    _AcGlDraw   draw;
    _AcGlHitbox hitbox;
    _AcGlKey    key;
    _AcGlFrame  frame;

    bool init()    const { return ac_gl_init() != 0; }
    void quit()    const { ac_gl_quit(); }
    bool is_draw(const std::string& n) const { return ac_gl_is_draw(n.c_str()) != 0; }
    bool is_obj(const std::string& n)  const { return ac_gl_is_obj(n.c_str())  != 0; }
};

inline _AcGlNS gl;
