// AC ilib: gl — SDL2 shared library implementation
// Build: see Makefile → libacgl.so (Linux) / acgl.dll (Windows)
#include "gl_c.h"
#include <SDL2/SDL.h>
#include <unordered_map>
#include <string>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cctype>
#include <algorithm>
#include <vector>

// ── Mini expression evaluator for curveshape(expr) ───────────────────────
// Supports: x, numbers, +, -, *, /, ^ (power), unary -, ()
// Functions: abs(), sqrt(), sin(), cos(), tan()

static double _curve_eval(const char* s, double xval) {
    struct P {
        const char* p; double x;
        void sp() { while (*p == ' ') p++; }
        bool kw(const char* k, int n) { return strncmp(p,k,n)==0 && p[n]=='('; }
        double fn_call() {
            if(kw("abs",3))  { p+=4; double v=add(); sp(); if(*p==')')p++; return fabs(v); }
            if(kw("sqrt",4)) { p+=5; double v=add(); sp(); if(*p==')')p++; return sqrt(v); }
            if(kw("sin",3))  { p+=4; double v=add(); sp(); if(*p==')')p++; return sin(v);  }
            if(kw("cos",3))  { p+=4; double v=add(); sp(); if(*p==')')p++; return cos(v);  }
            if(kw("tan",3))  { p+=4; double v=add(); sp(); if(*p==')')p++; return tan(v);  }
            return 0.0;
        }
        double atom() {
            sp();
            if(*p=='(') { p++; double v=add(); sp(); if(*p==')')p++; return v; }
            if((*p>='0'&&*p<='9')||(*p=='.'&&*(p+1)>='0'&&*(p+1)<='9')) {
                char* e; double v=strtod(p,&e); p=e; return v;
            }
            if(*p=='x') { p++; return x; }
            if(*p=='-') { p++; return -atom(); }
            if(*p=='+') { p++; return atom(); }
            // check known function names
            if(*p>='a'&&*p<='z') return fn_call();
            return 0.0;
        }
        double pow_() {
            double v=atom(); sp();
            if(*p=='^') { p++; double e=pow_(); return std::pow(v,e); } // right-assoc
            return v;
        }
        double mul() {
            double v=pow_(); sp();
            while(*p=='*'||*p=='/') { char op=*p++; sp(); double r=pow_(); v=(op=='*')?v*r:v/r; sp(); }
            return v;
        }
        double add() {
            double v=mul(); sp();
            while(*p=='+'||*p=='-') { char op=*p++; sp(); double r=mul(); v=(op=='+')?v+r:v-r; sp(); }
            return v;
        }
    } parser;
    parser.p = s; parser.x = xval;
    return parser.add();
}

// Bresenham circle draw (SDL2 has no built-in circle)
static void _draw_circle(SDL_Renderer* ren, int cx, int cy, int radius) {
    int x = radius, y = 0, d = 1 - radius;
    while (x >= y) {
        SDL_RenderDrawPoint(ren, cx+x, cy+y); SDL_RenderDrawPoint(ren, cx-x, cy+y);
        SDL_RenderDrawPoint(ren, cx+x, cy-y); SDL_RenderDrawPoint(ren, cx-x, cy-y);
        SDL_RenderDrawPoint(ren, cx+y, cy+x); SDL_RenderDrawPoint(ren, cx-y, cy+x);
        SDL_RenderDrawPoint(ren, cx+y, cy-x); SDL_RenderDrawPoint(ren, cx-y, cy-x);
        y++;
        if (d <= 0) d += 2*y + 1;
        else { x--; d += 2*(y-x) + 1; }
    }
}

// ── Object ────────────────────────────────────────────────────────────────

struct GlObj {
    int     x = 0, y = 0, w = 50, h = 50;
    uint8_t r = 255, g = 255, b = 255;
    float   vx = 0, vy = 0;
    float   speed = 0;
    float   direction = 0;
    std::string curveshape;
    float   vertex_x = 0, vertex_y = 0;

    // CircleFall animation
    bool    cf_active   = false;
    bool    cf_done     = false;
    float   cf_fraction = 0;     // e.g. 0.25 = 1/4 circle
    float   cf_angle    = 0;     // current angle (radians)
    float   cf_pivot_x  = 0;
    float   cf_pivot_y  = 0;
    float   cf_radius   = 0;

    // Spawn memory (set via set_spawn, restored via regen)
    bool    has_spawn   = false;
    float   spawn_x = 0, spawn_y = 0;
    float   spawn_vx = 0, spawn_vy = 0;
};

// ── Draw type — pure geometry, no object properties ───────────────────────

struct GlDraw {
    std::string curveshape;      // rendered curve expression
    float vertex_x = 0;          // curve anchor x
    float vertex_y = 0;          // curve anchor y
    struct Cmd {
        enum { LINE, CIRCLE } kind;
        float x1, y1, x2, y2, radius;
        uint8_t r, g, b;
    };
    std::vector<Cmd> cmds;
};

// ── Global state ──────────────────────────────────────────────────────────

static SDL_Window*   _win    = nullptr;
static SDL_Renderer* _ren    = nullptr;
static int           _scr_w  = 800;
static int           _scr_h  = 600;
static int           _fps    = 60;
static uint8_t       _bg_r   = 0, _bg_g = 0, _bg_b = 0;
static uint64_t      _t_last = 0;
static bool          _alive  = false;

static std::unordered_map<std::string, GlObj>  _objs;
static std::unordered_map<std::string, GlDraw> _draws;
static std::unordered_map<std::string, bool>   _held;
static std::unordered_map<std::string, bool>   _just;

// ── Key name normalisation ────────────────────────────────────────────────

static std::string _norm_key(const char* sdl_name) {
    std::string s(sdl_name);
    for (char& c : s) c = static_cast<char>(tolower(c));
    // SDL uses "Up", "Down", "Left", "Right", "Space", "Return"
    if (s == "return") return "enter";
    return s;
}

// ── Direction/velocity helpers ────────────────────────────────────────────

static constexpr float PI = 3.14159265f;

static void _update_vel(GlObj& o) {
    float rad = o.direction * PI / 180.0f;
    o.vx =  o.speed * sinf(rad);
    o.vy = -o.speed * cosf(rad);
}

// ── Wildcard matching ("p%" → any name starting with "p") ─────────────────

static bool _wmatch(const std::string& name, const char* pat) {
    size_t pl = strlen(pat);
    if (pl == 0) return true;
    if (pat[pl - 1] == '%') {
        size_t pre = pl - 1;
        return name.size() >= pre && name.compare(0, pre, pat, pre) == 0;
    }
    return name == pat;
}

// ═════════════════════════════════════════════════════════════════════════════
extern "C" {
// ═════════════════════════════════════════════════════════════════════════════

// ── Init / quit ───────────────────────────────────────────────────────────

int ac_gl_init(void) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "[gl] SDL_Init: %s\n", SDL_GetError());
        return 0;
    }
    return 1;
}

void ac_gl_quit(void) {
    if (_ren) { SDL_DestroyRenderer(_ren); _ren = nullptr; }
    if (_win) { SDL_DestroyWindow(_win);   _win = nullptr; }
    SDL_Quit();
    _alive = false;
}

// ── Screen ────────────────────────────────────────────────────────────────

int ac_gl_screen_create(int w, int h, const char* title) {
    _scr_w = w; _scr_h = h;
    _win = SDL_CreateWindow(title ? title : "AC",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        w, h, SDL_WINDOW_SHOWN);
    if (!_win) { fprintf(stderr, "[gl] CreateWindow: %s\n", SDL_GetError()); return 0; }
    _ren = SDL_CreateRenderer(_win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!_ren) { fprintf(stderr, "[gl] CreateRenderer: %s\n", SDL_GetError()); return 0; }
    _alive = true;
    _t_last = SDL_GetTicks64();
    return 1;
}

void ac_gl_screen_set_bg(uint8_t r, uint8_t g, uint8_t b) { _bg_r=r; _bg_g=g; _bg_b=b; }
void ac_gl_screen_set_fps(int fps) { _fps = fps > 0 ? fps : 60; }
int  ac_gl_screen_w(void) { return _scr_w; }
int  ac_gl_screen_h(void) { return _scr_h; }

// ── Objects ───────────────────────────────────────────────────────────────

void ac_gl_obj_create(const char* name) { _objs[name] = GlObj{}; }

void ac_gl_obj_geometry(const char* name, int w, int h) {
    auto it = _objs.find(name);
    if (it != _objs.end()) { it->second.w = w; it->second.h = h; }
}

void ac_gl_obj_square(const char* name, int size) { ac_gl_obj_geometry(name, size, size); }

void ac_gl_obj_pos(const char* name, int x, int y) {
    auto it = _objs.find(name);
    if (it != _objs.end()) { it->second.x = x; it->second.y = y; }
}

void ac_gl_obj_color(const char* name, uint8_t r, uint8_t g, uint8_t b) {
    auto it = _objs.find(name);
    if (it != _objs.end()) { it->second.r = r; it->second.g = g; it->second.b = b; }
}

void ac_gl_obj_velocity(const char* name, float vx, float vy) {
    auto it = _objs.find(name);
    if (it == _objs.end()) return;
    it->second.vx = vx; it->second.vy = vy;
    it->second.speed = sqrtf(vx*vx + vy*vy);
}

void ac_gl_obj_set_speed(const char* name, float speed) {
    auto it = _objs.find(name);
    if (it == _objs.end()) return;
    it->second.speed = speed;
    _update_vel(it->second);
}

void ac_gl_obj_set_direction(const char* name, float deg) {
    auto it = _objs.find(name);
    if (it == _objs.end()) return;
    it->second.direction = deg;
    _update_vel(it->second);
}

void ac_gl_obj_speed_mult(const char* name, float mult) {
    auto it = _objs.find(name);
    if (it == _objs.end()) return;
    it->second.vx    *= mult;
    it->second.vy    *= mult;
    it->second.speed  = fabsf(it->second.speed * mult);
}

void ac_gl_obj_move_x(const char* name, int dx) {
    auto it = _objs.find(name);
    if (it != _objs.end()) it->second.x += dx;
}

void ac_gl_obj_move_y(const char* name, int dy) {
    auto it = _objs.find(name);
    if (it != _objs.end()) it->second.y += dy;
}

int ac_gl_obj_x(const char* n) { auto i=_objs.find(n); return i!=_objs.end()?i->second.x:0; }
int ac_gl_obj_y(const char* n) { auto i=_objs.find(n); return i!=_objs.end()?i->second.y:0; }
int ac_gl_obj_w(const char* n) { auto i=_objs.find(n); return i!=_objs.end()?i->second.w:0; }
int ac_gl_obj_h(const char* n) { auto i=_objs.find(n); return i!=_objs.end()?i->second.h:0; }

// ── Curveshape / vertex (objects) ────────────────────────────────────────

void ac_gl_obj_curveshape(const char* name, const char* expr) {
    auto it = _objs.find(name);
    if (it != _objs.end()) it->second.curveshape = expr ? expr : "";
}
void ac_gl_obj_vertex(const char* name, float vx, float vy) {
    auto it = _objs.find(name);
    if (it != _objs.end()) { it->second.vertex_x = vx; it->second.vertex_y = vy; }
}

// ── Draw type ─────────────────────────────────────────────────────────────

void ac_gl_draw_create(const char* name) { _draws[name] = GlDraw{}; }

void ac_gl_draw_curveshape(const char* name, const char* expr) {
    auto it = _draws.find(name);
    if (it != _draws.end()) it->second.curveshape = expr ? expr : "";
}
void ac_gl_draw_vertex(const char* name, float vx, float vy) {
    auto it = _draws.find(name);
    if (it != _draws.end()) { it->second.vertex_x = vx; it->second.vertex_y = vy; }
}
void ac_gl_draw_line(const char* name, float x1, float y1, float x2, float y2,
                     uint8_t r, uint8_t g, uint8_t b) {
    auto it = _draws.find(name);
    if (it == _draws.end()) return;
    GlDraw::Cmd c; c.kind = GlDraw::Cmd::LINE;
    c.x1=x1; c.y1=y1; c.x2=x2; c.y2=y2; c.radius=0; c.r=r; c.g=g; c.b=b;
    it->second.cmds.push_back(c);
}
void ac_gl_draw_circle(const char* name, float cx, float cy, float radius,
                       uint8_t r, uint8_t g, uint8_t b) {
    auto it = _draws.find(name);
    if (it == _draws.end()) return;
    GlDraw::Cmd c; c.kind = GlDraw::Cmd::CIRCLE;
    c.x1=cx; c.y1=cy; c.x2=0; c.y2=0; c.radius=radius; c.r=r; c.g=g; c.b=b;
    it->second.cmds.push_back(c);
}
void ac_gl_draw_clear(const char* name) {
    auto it = _draws.find(name);
    if (it != _draws.end()) { it->second.cmds.clear(); it->second.curveshape.clear(); }
}

// ── Type conversion ───────────────────────────────────────────────────────

void ac_gl_obj_to_draw(const char* name) {
    if (_objs.count(name)) {
        _objs.erase(name);
        _draws[name] = GlDraw{};
    }
}
void ac_gl_draw_to_obj(const char* name) {
    if (_draws.count(name)) {
        _draws.erase(name);
        _objs[name] = GlObj{};
    }
}
int ac_gl_is_draw(const char* name) { return _draws.count(name) ? 1 : 0; }
int ac_gl_is_obj(const char* name)  { return _objs.count(name)  ? 1 : 0; }

// ── Collision ─────────────────────────────────────────────────────────────

static bool _aabb(const GlObj& A, const GlObj& B) {
    return !(A.x+A.w <= B.x || B.x+B.w <= A.x ||
             A.y+A.h <= B.y || B.y+B.h <= A.y);
}

int ac_gl_hitbox_overlap(const char* a, const char* b) {
    auto ia = _objs.find(a), ib = _objs.find(b);
    if (ia == _objs.end() || ib == _objs.end()) return 0;
    return _aabb(ia->second, ib->second) ? 1 : 0;
}

int ac_gl_hitbox_overlap_boundary(const char* name) {
    auto it = _objs.find(name);
    if (it == _objs.end()) return 0;
    const GlObj& o = it->second;
    return (o.x <= 0 || o.x+o.w >= _scr_w ||
            o.y <= 0 || o.y+o.h >= _scr_h) ? 1 : 0;
}

int ac_gl_hitbox_overlap_pattern(const char* name, const char* pattern) {
    auto it = _objs.find(name);
    if (it == _objs.end()) return 0;
    const GlObj& A = it->second;
    for (auto& [bname, B] : _objs) {
        if (bname == name) continue;
        if (_wmatch(bname, pattern) && _aabb(A, B)) return 1;
    }
    return 0;
}

// ── Input ─────────────────────────────────────────────────────────────────

int ac_gl_key_pressed(const char* key)      { auto i=_held.find(key); return i!=_held.end()&&i->second?1:0; }
int ac_gl_key_just_pressed(const char* key) { auto i=_just.find(key); return i!=_just.end()&&i->second?1:0; }

// ── Frame loop ────────────────────────────────────────────────────────────

int ac_gl_frame_begin(void) {
    if (!_alive) return 0;
    _just.clear();
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) { _alive = false; return 0; }
        if (e.type == SDL_KEYDOWN) {
            std::string k = _norm_key(SDL_GetKeyName(e.key.keysym.sym));
            _held[k] = true; _just[k] = true;
            if (k == "escape") { _alive = false; return 0; }
        }
        if (e.type == SDL_KEYUP) {
            std::string k = _norm_key(SDL_GetKeyName(e.key.keysym.sym));
            _held[k] = false;
        }
    }
    return 1;
}

float ac_gl_delta_time(void) {
    uint64_t now = SDL_GetTicks64();
    float dt = (float)(now - _t_last) / 1000.0f;
    return dt > 0.1f ? 0.1f : dt;
}

void ac_gl_frame_update(float dt) {
    for (auto& [_, o] : _objs) {
        if (o.cf_active) {
            float target = o.cf_fraction * PI / 2.0f;
            o.cf_angle += PI * dt;  // completes quarter arc in 0.5s
            if (o.cf_angle >= target) { o.cf_angle = target; o.cf_active = false; o.cf_done = true; }
            float cx = o.cf_pivot_x + o.cf_radius * sinf(o.cf_angle);
            float cy = o.cf_pivot_y - o.cf_radius * cosf(o.cf_angle);
            o.x = (int)(cx - o.w * 0.5f);
            o.y = (int)(cy - o.h * 0.5f);
        } else {
            o.x += (int)(o.vx * dt);
            o.y += (int)(o.vy * dt);
        }
    }
}

void ac_gl_frame_render(void) {
    if (!_ren) return;
    SDL_SetRenderDrawColor(_ren, _bg_r, _bg_g, _bg_b, 255);
    SDL_RenderClear(_ren);

    // Objects (rectangle sprites)
    for (auto& [_, o] : _objs) {
        SDL_SetRenderDrawColor(_ren, o.r, o.g, o.b, 255);
        SDL_Rect rect{ o.x, o.y, o.w, o.h };
        SDL_RenderFillRect(_ren, &rect);

        // Draw trajectory curve ghost (dim white) if curveshape is set
        if (!o.curveshape.empty()) {
            SDL_SetRenderDrawColor(_ren, 180, 180, 180, 255);
            const int N = 200;
            float range = (float)_scr_w * 0.6f;
            float step  = range * 2.0f / N;
            int px = -1, py = -1;
            for (int i = 0; i <= N; i++) {
                float xoff = -range + step * i;
                float yval = (float)_curve_eval(o.curveshape.c_str(), (double)xoff);
                int sx = (int)(o.vertex_x + xoff);
                int sy = (int)(o.vertex_y - yval);
                if (px >= 0) SDL_RenderDrawLine(_ren, px, py, sx, sy);
                px = sx; py = sy;
            }
        }
    }

    // Draw entities (pure geometry)
    for (auto& [_, d] : _draws) {
        // Explicit draw commands
        for (auto& cmd : d.cmds) {
            SDL_SetRenderDrawColor(_ren, cmd.r, cmd.g, cmd.b, 255);
            if (cmd.kind == GlDraw::Cmd::LINE) {
                SDL_RenderDrawLine(_ren,
                    (int)cmd.x1, (int)cmd.y1, (int)cmd.x2, (int)cmd.y2);
            } else {
                _draw_circle(_ren, (int)cmd.x1, (int)cmd.y1, (int)cmd.radius);
            }
        }
        // Curve rendering: screen_y = vertex_y - expr(x - vertex_x)
        if (!d.curveshape.empty()) {
            SDL_SetRenderDrawColor(_ren, 255, 255, 255, 255);
            const int N = 300;
            float range = (float)_scr_w * 0.5f;
            float step  = range * 2.0f / N;
            int px = -1, py = -1;
            for (int i = 0; i <= N; i++) {
                float xoff = -range + step * i;
                float yval = (float)_curve_eval(d.curveshape.c_str(), (double)xoff);
                int sx = (int)(d.vertex_x + xoff);
                int sy = (int)(d.vertex_y - yval);
                if (sx < 0 || sx > _scr_w || sy < 0 || sy > _scr_h) { px=-1; continue; }
                if (px >= 0) SDL_RenderDrawLine(_ren, px, py, sx, sy);
                px = sx; py = sy;
            }
        }
    }
}

void ac_gl_frame_end(void) {
    if (_ren) SDL_RenderPresent(_ren);
    uint64_t now  = SDL_GetTicks64();
    uint64_t used = now - _t_last;
    uint64_t cap  = 1000ULL / (uint64_t)_fps;
    if (used < cap) SDL_Delay((uint32_t)(cap - used));
    _t_last = SDL_GetTicks64();
}

// ── CircleFall / regen / animate ──────────────────────────────────────────

void ac_gl_obj_circle_fall(const char* name, float fraction, const char* dir) {
    auto it = _objs.find(name);
    if (it == _objs.end()) return;
    GlObj& o = it->second;
    if (o.cf_active || o.cf_done) return;
    (void)dir; // currently only rightward fall is supported; dir reserved for future
    o.cf_active   = true;
    o.cf_done     = false;
    o.cf_fraction = fraction;
    o.cf_angle    = 0.0f;
    // pivot at bottom-centre of object
    o.cf_pivot_x  = o.x + o.w * 0.5f;
    o.cf_pivot_y  = (float)(o.y + o.h);
    o.cf_radius   = o.h * 0.5f;
}

int ac_gl_obj_circle_fell(const char* name, float /*fraction*/, const char* /*dir*/) {
    auto it = _objs.find(name);
    return (it != _objs.end() && it->second.cf_done) ? 1 : 0;
}

void ac_gl_obj_set_spawn(const char* name) {
    auto it = _objs.find(name);
    if (it == _objs.end()) return;
    GlObj& o = it->second;
    o.spawn_x = (float)o.x;  o.spawn_y = (float)o.y;
    o.spawn_vx = o.vx;       o.spawn_vy = o.vy;
    o.has_spawn = true;
}

void ac_gl_obj_regen(const char* name) {
    auto it = _objs.find(name);
    if (it == _objs.end() || !it->second.has_spawn) return;
    GlObj& o = it->second;
    o.x = (int)o.spawn_x;  o.y = (int)o.spawn_y;
    o.vx = o.spawn_vx;     o.vy = o.spawn_vy;
    o.speed = sqrtf(o.vx*o.vx + o.vy*o.vy);
    o.cf_active = false;  o.cf_done = false;  o.cf_angle = 0.0f;
}

void ac_gl_obj_animate(const char* name, const char* dir, float speed) {
    auto it = _objs.find(name);
    if (it == _objs.end()) return;
    GlObj& o = it->second;
    std::string d(dir ? dir : "");
    if      (d == "right") { o.vx =  speed; o.vy = 0; }
    else if (d == "left")  { o.vx = -speed; o.vy = 0; }
    else if (d == "up")    { o.vx = 0; o.vy = -speed; }
    else if (d == "down")  { o.vx = 0; o.vy =  speed; }
    o.speed = speed;
}

int ac_gl_hitbox_many_overlap(void) {
    std::vector<std::string> names;
    names.reserve(_objs.size());
    for (auto& [n, _] : _objs) names.push_back(n);
    int pairs = 0;
    for (size_t i = 0; i < names.size(); i++)
        for (size_t j = i+1; j < names.size(); j++)
            if (_aabb(_objs[names[i]], _objs[names[j]]) && ++pairs >= 2) return 1;
    return 0;
}

// ── High-level helpers (used by AC-compiled code on all backends) ─────────────

static struct { const char* name; uint8_t r,g,b; } _color_table[] = {
    {"white",   255,255,255}, {"black",  0,0,0},     {"red",    255,0,0},
    {"green",   0,255,0},     {"blue",   0,0,255},    {"yellow", 255,255,0},
    {"cyan",    0,255,255},   {"magenta",255,0,255},  {"gray",   128,128,128},
    {"grey",    128,128,128}, {"orange", 255,165,0},  {"purple", 128,0,128},
    {"brown",   139,69,19},   {"pink",   255,182,193},{"lime",   0,255,127},
    {"teal",    0,128,128},   {nullptr,  0,0,0}
};

void ac_gl_obj_color_by_name(const char* name, const char* color_name) {
    for (int i = 0; _color_table[i].name; i++) {
        if (strcmp(color_name, _color_table[i].name) == 0) {
            ac_gl_obj_color(name, _color_table[i].r, _color_table[i].g, _color_table[i].b);
            return;
        }
    }
}

void ac_gl_screen_set_bg_by_name(const char* color_name) {
    for (int i = 0; _color_table[i].name; i++) {
        if (strcmp(color_name, _color_table[i].name) == 0) {
            ac_gl_screen_set_bg(_color_table[i].r, _color_table[i].g, _color_table[i].b);
            return;
        }
    }
}

void ac_gl_obj_pos_from_spec(const char* name, const char* x_spec, const char* y_spec) {
    int x = 0, y = 0;
    if (strcmp(x_spec, "center") == 0)         x = (_scr_w - ac_gl_obj_w(name)) / 2;
    else if (strcmp(x_spec, "right")  == 0)    x = _scr_w - ac_gl_obj_w(name);
    else if (strcmp(x_spec, "left")   == 0)    x = 0;
    else if (strncmp(x_spec, "screen-", 7) == 0) x = _scr_w - atoi(x_spec + 7);
    else x = atoi(x_spec);

    if (strcmp(y_spec, "center") == 0)          y = (_scr_h - ac_gl_obj_h(name)) / 2;
    else if (strcmp(y_spec, "bottom") == 0)     y = _scr_h - ac_gl_obj_h(name);
    else if (strcmp(y_spec, "top") == 0)        y = 0;
    else if (strncmp(y_spec, "screen-", 7) == 0) y = _scr_h - atoi(y_spec + 7);
    else y = atoi(y_spec);

    ac_gl_obj_pos(name, x, y);
}

void ac_gl_obj_config_item(const char* name, const char* item_spec) {
    if (strncmp(item_spec, "square(", 7) == 0) {
        int sz = atoi(item_spec + 7);
        ac_gl_obj_square(name, sz);
    } else if (strncmp(item_spec, "geometry(", 9) == 0) {
        int w2 = 0, h2 = 0;
        const char* p = item_spec + 9;
        w2 = atoi(p);
        while (*p && *p != '@') p++;
        if (*p == '@') { p++; while (*p == ' ') p++; h2 = atoi(p); }
        if (w2 > 0 && h2 > 0) ac_gl_obj_geometry(name, w2, h2);
    } else if (strncmp(item_spec, "rect(", 5) == 0) {
        int w2 = 0, h2 = 0;
        sscanf(item_spec + 5, "%d,%d", &w2, &h2);
        if (w2 > 0 && h2 > 0) ac_gl_obj_geometry(name, w2, h2);
    } else if (strncmp(item_spec, "circle(", 7) == 0) {
        int r = atoi(item_spec + 7);
        ac_gl_obj_geometry(name, r * 2, r * 2);
    }
}

void ac_gl_screen_init(int w, int h, const char* title) {
    ac_gl_init();
    ac_gl_screen_create(w, h, title);
}

void ac_gl_screen_animate(const char* fps_spec) {
    int fps = atoi(fps_spec);
    if (fps > 0) ac_gl_screen_set_fps(fps);
}

void ac_gl_obj_save_spawn(const char* name) {
    ac_gl_obj_set_spawn(name);
}

} // extern "C"
