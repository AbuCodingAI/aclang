// AC ilib: gl — Rust FFI (libacgl.so / acgl.dll)
use std::ffi::{CString, c_char};
use std::os::raw::{c_int, c_float};

type U8 = u8;

#[link(name = "acgl")]
extern "C" {
    fn _raw_ac_gl_init() -> c_int;
    fn _raw_ac_gl_quit();

    fn _raw_ac_gl_screen_create(w: c_int, h: c_int, title: *const c_char) -> c_int;
    fn _raw_ac_gl_screen_set_bg(r: U8, g: U8, b: U8);
    fn _raw_ac_gl_screen_set_fps(fps: c_int);
    fn _raw_ac_gl_screen_w() -> c_int;
    fn _raw_ac_gl_screen_h() -> c_int;

    fn _raw_ac_gl_obj_create(name: *const c_char);
    fn _raw_ac_gl_obj_geometry(name: *const c_char, w: c_int, h: c_int);
    fn _raw_ac_gl_obj_square(name: *const c_char, size: c_int);
    fn _raw_ac_gl_obj_pos(name: *const c_char, x: c_int, y: c_int);
    fn _raw_ac_gl_obj_color(name: *const c_char, r: U8, g: U8, b: U8);
    fn _raw_ac_gl_obj_velocity(name: *const c_char, vx: c_float, vy: c_float);
    fn _raw_ac_gl_obj_set_speed(name: *const c_char, speed: c_float);
    fn _raw_ac_gl_obj_set_direction(name: *const c_char, deg: c_float);
    fn _raw_ac_gl_obj_speed_mult(name: *const c_char, mult: c_float);
    fn _raw_ac_gl_obj_move_x(name: *const c_char, dx: c_int);
    fn _raw_ac_gl_obj_move_y(name: *const c_char, dy: c_int);
    fn _raw_ac_gl_obj_x(name: *const c_char) -> c_int;
    fn _raw_ac_gl_obj_y(name: *const c_char) -> c_int;
    fn _raw_ac_gl_obj_w(name: *const c_char) -> c_int;
    fn _raw_ac_gl_obj_h(name: *const c_char) -> c_int;
    fn _raw_ac_gl_obj_curveshape(name: *const c_char, expr: *const c_char);
    fn _raw_ac_gl_obj_vertex(name: *const c_char, vx: c_float, vy: c_float);
    fn _raw_ac_gl_obj_to_draw(name: *const c_char);
    fn _raw_ac_gl_obj_circle_fall(name: *const c_char, fraction: c_float, dir: *const c_char);
    fn _raw_ac_gl_obj_circle_fell(name: *const c_char, fraction: c_float, dir: *const c_char) -> c_int;
    fn _raw_ac_gl_obj_set_spawn(name: *const c_char);
    fn _raw_ac_gl_obj_regen(name: *const c_char);
    fn _raw_ac_gl_obj_animate(name: *const c_char, dir: *const c_char, speed: c_float);
    fn _raw_ac_gl_hitbox_many_overlap() -> c_int;

    fn _raw_ac_gl_draw_create(name: *const c_char);
    fn _raw_ac_gl_draw_curveshape(name: *const c_char, expr: *const c_char);
    fn _raw_ac_gl_draw_vertex(name: *const c_char, vx: c_float, vy: c_float);
    fn _raw_ac_gl_draw_line(name: *const c_char, x1: c_float, y1: c_float, x2: c_float, y2: c_float, r: U8, g: U8, b: U8);
    fn _raw_ac_gl_draw_circle(name: *const c_char, cx: c_float, cy: c_float, radius: c_float, r: U8, g: U8, b: U8);
    fn _raw_ac_gl_draw_clear(name: *const c_char);
    fn _raw_ac_gl_draw_to_obj(name: *const c_char);
    fn _raw_ac_gl_is_draw(name: *const c_char) -> c_int;
    fn _raw_ac_gl_is_obj(name: *const c_char) -> c_int;

    fn _raw_ac_gl_hitbox_overlap(a: *const c_char, b: *const c_char) -> c_int;
    fn _raw_ac_gl_hitbox_overlap_boundary(name: *const c_char) -> c_int;
    fn _raw_ac_gl_hitbox_overlap_pattern(name: *const c_char, pat: *const c_char) -> c_int;

    fn _raw_ac_gl_key_pressed(key: *const c_char) -> c_int;
    fn _raw_ac_gl_key_just_pressed(key: *const c_char) -> c_int;

    fn _raw_ac_gl_frame_begin() -> c_int;
    fn _raw_ac_gl_frame_update(dt: c_float);
    fn _raw_ac_gl_frame_render();
    fn _raw_ac_gl_frame_end();
    fn _raw_ac_gl_delta_time() -> c_float;
}

fn _cs(s: &str) -> CString { CString::new(s).unwrap_or_default() }

pub fn gl_init() -> bool        { unsafe { _raw_ac_gl_init() != 0 } }
pub fn gl_quit()                { unsafe { _raw_ac_gl_quit() } }

pub fn gl_screen_create(w: i32, h: i32, title: &str) -> bool {
    let ct = _cs(title); unsafe { _raw_ac_gl_screen_create(w, h, ct.as_ptr()) != 0 }
}
pub fn gl_screen_set_bg(r: u8, g: u8, b: u8) { unsafe { _raw_ac_gl_screen_set_bg(r, g, b) } }
pub fn gl_screen_set_fps(fps: i32)             { unsafe { _raw_ac_gl_screen_set_fps(fps) } }
pub fn gl_screen_w() -> i32                    { unsafe { _raw_ac_gl_screen_w() } }
pub fn gl_screen_h() -> i32                    { unsafe { _raw_ac_gl_screen_h() } }

pub fn gl_obj_create(name: &str)                    { let cn = _cs(name); unsafe { _raw_ac_gl_obj_create(cn.as_ptr()) } }
pub fn gl_obj_geometry(name: &str, w: i32, h: i32)  { let cn = _cs(name); unsafe { _raw_ac_gl_obj_geometry(cn.as_ptr(), w, h) } }
pub fn gl_obj_square(name: &str, size: i32)          { let cn = _cs(name); unsafe { _raw_ac_gl_obj_square(cn.as_ptr(), size) } }
pub fn gl_obj_pos(name: &str, x: i32, y: i32)        { let cn = _cs(name); unsafe { _raw_ac_gl_obj_pos(cn.as_ptr(), x, y) } }
pub fn gl_obj_color(name: &str, r: u8, g: u8, b: u8){ let cn = _cs(name); unsafe { _raw_ac_gl_obj_color(cn.as_ptr(), r, g, b) } }
pub fn gl_obj_velocity(name: &str, vx: f32, vy: f32) { let cn = _cs(name); unsafe { _raw_ac_gl_obj_velocity(cn.as_ptr(), vx, vy) } }
pub fn gl_obj_set_speed(name: &str, s: f32)           { let cn = _cs(name); unsafe { _raw_ac_gl_obj_set_speed(cn.as_ptr(), s) } }
pub fn gl_obj_set_direction(name: &str, deg: f32)     { let cn = _cs(name); unsafe { _raw_ac_gl_obj_set_direction(cn.as_ptr(), deg) } }
pub fn gl_obj_speed_mult(name: &str, m: f32)          { let cn = _cs(name); unsafe { _raw_ac_gl_obj_speed_mult(cn.as_ptr(), m) } }
pub fn gl_obj_move_x(name: &str, dx: i32)            { let cn = _cs(name); unsafe { _raw_ac_gl_obj_move_x(cn.as_ptr(), dx) } }
pub fn gl_obj_move_y(name: &str, dy: i32)            { let cn = _cs(name); unsafe { _raw_ac_gl_obj_move_y(cn.as_ptr(), dy) } }
pub fn gl_obj_x(name: &str) -> i32                   { let cn = _cs(name); unsafe { _raw_ac_gl_obj_x(cn.as_ptr()) } }
pub fn gl_obj_y(name: &str) -> i32                   { let cn = _cs(name); unsafe { _raw_ac_gl_obj_y(cn.as_ptr()) } }
pub fn gl_obj_w(name: &str) -> i32                   { let cn = _cs(name); unsafe { _raw_ac_gl_obj_w(cn.as_ptr()) } }
pub fn gl_obj_h(name: &str) -> i32                   { let cn = _cs(name); unsafe { _raw_ac_gl_obj_h(cn.as_ptr()) } }
pub fn gl_obj_curveshape(name: &str, expr: &str)     { let cn=_cs(name); let ce=_cs(expr); unsafe { _raw_ac_gl_obj_curveshape(cn.as_ptr(), ce.as_ptr()) } }
pub fn gl_obj_vertex(name: &str, vx: f32, vy: f32)  { let cn=_cs(name); unsafe { _raw_ac_gl_obj_vertex(cn.as_ptr(), vx, vy) } }
pub fn gl_obj_to_draw(name: &str)                    { let cn=_cs(name); unsafe { _raw_ac_gl_obj_to_draw(cn.as_ptr()) } }
pub fn gl_obj_circle_fall(name: &str, frac: f32, dir: &str) { let cn=_cs(name); let cd=_cs(dir); unsafe { _raw_ac_gl_obj_circle_fall(cn.as_ptr(), frac, cd.as_ptr()) } }
pub fn gl_obj_circle_fell(name: &str, frac: f32, dir: &str) -> bool { let cn=_cs(name); let cd=_cs(dir); unsafe { _raw_ac_gl_obj_circle_fell(cn.as_ptr(), frac, cd.as_ptr()) != 0 } }
pub fn gl_obj_set_spawn(name: &str)                  { let cn=_cs(name); unsafe { _raw_ac_gl_obj_set_spawn(cn.as_ptr()) } }
pub fn gl_obj_regen(name: &str)                      { let cn=_cs(name); unsafe { _raw_ac_gl_obj_regen(cn.as_ptr()) } }
pub fn gl_obj_animate(name: &str, dir: &str, speed: f32) { let cn=_cs(name); let cd=_cs(dir); unsafe { _raw_ac_gl_obj_animate(cn.as_ptr(), cd.as_ptr(), speed) } }
pub fn gl_hitbox_many_overlap() -> bool              { unsafe { _raw_ac_gl_hitbox_many_overlap() != 0 } }

pub fn gl_draw_create(name: &str)                    { let cn=_cs(name); unsafe { _raw_ac_gl_draw_create(cn.as_ptr()) } }
pub fn gl_draw_curveshape(name: &str, expr: &str)    { let cn=_cs(name); let ce=_cs(expr); unsafe { _raw_ac_gl_draw_curveshape(cn.as_ptr(), ce.as_ptr()) } }
pub fn gl_draw_vertex(name: &str, vx: f32, vy: f32) { let cn=_cs(name); unsafe { _raw_ac_gl_draw_vertex(cn.as_ptr(), vx, vy) } }
pub fn gl_draw_line(name: &str, x1: f32, y1: f32, x2: f32, y2: f32, r: u8, g: u8, b: u8) {
    let cn=_cs(name); unsafe { _raw_ac_gl_draw_line(cn.as_ptr(), x1, y1, x2, y2, r, g, b) }
}
pub fn gl_draw_circle(name: &str, cx: f32, cy: f32, radius: f32, r: u8, g: u8, b: u8) {
    let cn=_cs(name); unsafe { _raw_ac_gl_draw_circle(cn.as_ptr(), cx, cy, radius, r, g, b) }
}
pub fn gl_draw_clear(name: &str)                     { let cn=_cs(name); unsafe { _raw_ac_gl_draw_clear(cn.as_ptr()) } }
pub fn gl_draw_to_obj(name: &str)                    { let cn=_cs(name); unsafe { _raw_ac_gl_draw_to_obj(cn.as_ptr()) } }
pub fn gl_is_draw(name: &str) -> bool                { let cn=_cs(name); unsafe { _raw_ac_gl_is_draw(cn.as_ptr()) != 0 } }
pub fn gl_is_obj(name: &str) -> bool                 { let cn=_cs(name); unsafe { _raw_ac_gl_is_obj(cn.as_ptr()) != 0 } }

pub fn gl_hitbox_overlap(a: &str, b: &str) -> bool {
    let ca = _cs(a); let cb = _cs(b); unsafe { _raw_ac_gl_hitbox_overlap(ca.as_ptr(), cb.as_ptr()) != 0 }
}
pub fn gl_hitbox_boundary(name: &str) -> bool {
    let cn = _cs(name); unsafe { _raw_ac_gl_hitbox_overlap_boundary(cn.as_ptr()) != 0 }
}
pub fn gl_hitbox_overlap_pattern(name: &str, pat: &str) -> bool {
    let cn = _cs(name); let cp = _cs(pat); unsafe { _raw_ac_gl_hitbox_overlap_pattern(cn.as_ptr(), cp.as_ptr()) != 0 }
}

pub fn gl_key_pressed(k: &str) -> bool      { let ck = _cs(k); unsafe { _raw_ac_gl_key_pressed(ck.as_ptr()) != 0 } }
pub fn gl_key_just_pressed(k: &str) -> bool { let ck = _cs(k); unsafe { _raw_ac_gl_key_just_pressed(ck.as_ptr()) != 0 } }

pub fn gl_frame_begin() -> bool    { unsafe { _raw_ac_gl_frame_begin() != 0 } }
pub fn gl_frame_update(dt: f32)    { unsafe { _raw_ac_gl_frame_update(dt) } }
pub fn gl_frame_render()           { unsafe { _raw_ac_gl_frame_render() } }
pub fn gl_frame_end()              { unsafe { _raw_ac_gl_frame_end() } }
pub fn gl_delta_time() -> f32      { unsafe { _raw_ac_gl_delta_time() } }

// ── Namespace structs — AC-generated Rust uses gl.screen.create(...) etc. ──

pub struct AcGlScreen;
impl AcGlScreen {
    pub fn create(&self, w: i32, h: i32, title: &str) -> bool { gl_screen_create(w, h, title) }
    pub fn set_bg(&self, r: u8, g: u8, b: u8)                 { gl_screen_set_bg(r, g, b) }
    pub fn set_fps(&self, fps: i32)                            { gl_screen_set_fps(fps) }
    pub fn w(&self) -> i32                                     { gl_screen_w() }
    pub fn h(&self) -> i32                                     { gl_screen_h() }
}

pub struct AcGlObj;
impl AcGlObj {
    pub fn create(&self, name: &str)                      { gl_obj_create(name) }
    pub fn geometry(&self, name: &str, w: i32, h: i32)   { gl_obj_geometry(name, w, h) }
    pub fn square(&self, name: &str, size: i32)            { gl_obj_square(name, size) }
    pub fn pos(&self, name: &str, x: i32, y: i32)          { gl_obj_pos(name, x, y) }
    pub fn color(&self, name: &str, r: u8, g: u8, b: u8) { gl_obj_color(name, r, g, b) }
    pub fn velocity(&self, name: &str, vx: f32, vy: f32)  { gl_obj_velocity(name, vx, vy) }
    pub fn set_speed(&self, name: &str, s: f32)            { gl_obj_set_speed(name, s) }
    pub fn set_direction(&self, name: &str, d: f32)        { gl_obj_set_direction(name, d) }
    pub fn speed_mult(&self, name: &str, m: f32)           { gl_obj_speed_mult(name, m) }
    pub fn move_x(&self, name: &str, dx: i32)             { gl_obj_move_x(name, dx) }
    pub fn move_y(&self, name: &str, dy: i32)             { gl_obj_move_y(name, dy) }
    pub fn x(&self, name: &str) -> i32                    { gl_obj_x(name) }
    pub fn y(&self, name: &str) -> i32                    { gl_obj_y(name) }
    pub fn w(&self, name: &str) -> i32                    { gl_obj_w(name) }
    pub fn h(&self, name: &str) -> i32                    { gl_obj_h(name) }
    pub fn curveshape(&self, name: &str, expr: &str)      { gl_obj_curveshape(name, expr) }
    pub fn vertex(&self, name: &str, vx: f32, vy: f32)   { gl_obj_vertex(name, vx, vy) }
    pub fn to_draw(&self, name: &str)                             { gl_obj_to_draw(name) }
    pub fn circle_fall(&self, name: &str, frac: f32, dir: &str)  { gl_obj_circle_fall(name, frac, dir) }
    pub fn circle_fell(&self, name: &str, frac: f32, dir: &str) -> bool { gl_obj_circle_fell(name, frac, dir) }
    pub fn set_spawn(&self, name: &str)                           { gl_obj_set_spawn(name) }
    pub fn regen(&self, name: &str)                               { gl_obj_regen(name) }
    pub fn animate(&self, name: &str, dir: &str, speed: f32)      { gl_obj_animate(name, dir, speed) }
}

pub struct AcGlDraw;
impl AcGlDraw {
    pub fn create(&self, name: &str)                                             { gl_draw_create(name) }
    pub fn curveshape(&self, name: &str, expr: &str)                             { gl_draw_curveshape(name, expr) }
    pub fn vertex(&self, name: &str, vx: f32, vy: f32)                          { gl_draw_vertex(name, vx, vy) }
    pub fn line(&self, name: &str, x1: f32, y1: f32, x2: f32, y2: f32, r: u8, g: u8, b: u8) { gl_draw_line(name, x1, y1, x2, y2, r, g, b) }
    pub fn circle(&self, name: &str, cx: f32, cy: f32, rad: f32, r: u8, g: u8, b: u8)        { gl_draw_circle(name, cx, cy, rad, r, g, b) }
    pub fn clear(&self, name: &str)                                              { gl_draw_clear(name) }
    pub fn to_obj(&self, name: &str)                                             { gl_draw_to_obj(name) }
}

pub struct AcGlHitbox;
impl AcGlHitbox {
    pub fn overlap(&self, a: &str, b: &str) -> bool           { gl_hitbox_overlap(a, b) }
    pub fn boundary(&self, name: &str) -> bool                 { gl_hitbox_boundary(name) }
    pub fn overlap_pattern(&self, n: &str, pat: &str) -> bool  { gl_hitbox_overlap_pattern(n, pat) }
    pub fn many_overlap(&self) -> bool                         { gl_hitbox_many_overlap() }
}

pub struct AcGlKey;
impl AcGlKey {
    pub fn pressed(&self, k: &str) -> bool      { gl_key_pressed(k) }
    pub fn just_pressed(&self, k: &str) -> bool { gl_key_just_pressed(k) }
}

pub struct AcGlFrame;
impl AcGlFrame {
    pub fn begin(&self) -> bool     { gl_frame_begin() }
    pub fn update(&self, dt: f32)   { gl_frame_update(dt) }
    pub fn render(&self)            { gl_frame_render() }
    pub fn end(&self)               { gl_frame_end() }
    pub fn delta(&self) -> f32      { gl_delta_time() }
}

pub struct AcGl {
    pub screen: AcGlScreen,
    pub obj:    AcGlObj,
    pub draw:   AcGlDraw,
    pub hitbox: AcGlHitbox,
    pub key:    AcGlKey,
    pub frame:  AcGlFrame,
}
impl AcGl {
    pub fn init(&self) -> bool               { gl_init() }
    pub fn quit(&self)                       { gl_quit() }
    pub fn is_draw(&self, name: &str) -> bool { gl_is_draw(name) }
    pub fn is_obj(&self, name: &str) -> bool  { gl_is_obj(name) }
}

pub static gl: AcGl = AcGl {
    screen: AcGlScreen,
    obj:    AcGlObj,
    draw:   AcGlDraw,
    hitbox: AcGlHitbox,
    key:    AcGlKey,
    frame:  AcGlFrame,
};

// ── ac_gl_* safe wrappers — used by AC-compiled code on all backends ──────────

pub fn ac_gl_init() -> bool                              { gl_init() }
pub fn ac_gl_quit()                                      { gl_quit() }
pub fn ac_gl_screen_init(w: i32, h: i32, title: &str)   { gl_screen_create(w, h, title); }
pub fn ac_gl_screen_animate(spec: &str) {
    let n: i32 = spec.chars().take_while(|c| c.is_ascii_digit()).collect::<String>().parse().unwrap_or(60);
    gl_screen_set_fps(n);
}
pub fn ac_gl_screen_set_bg_by_name(color: &str) {
    let (r,g,b) = _color_by_name(color); gl_screen_set_bg(r,g,b);
}
pub fn ac_gl_obj_create(name: &str)                      { gl_obj_create(name) }
pub fn ac_gl_obj_config_item(name: &str, spec: &str) {
    if spec.starts_with("square(") { let s: i32 = spec[7..].trim_end_matches(')').parse().unwrap_or(50); gl_obj_square(name, s); }
    else if spec.starts_with("geometry(") { let inner = &spec[9..spec.len().saturating_sub(1)]; let mut parts = inner.splitn(2,'@'); let w2: i32 = parts.next().unwrap_or("0").split_whitespace().next().unwrap_or("0").parse().unwrap_or(0); let h2: i32 = parts.next().unwrap_or("0").split_whitespace().next().unwrap_or("0").parse().unwrap_or(0); if w2>0&&h2>0 { gl_obj_geometry(name, w2, h2); } }
    else if spec.starts_with("rect(") { let inner = &spec[5..spec.len().saturating_sub(1)]; let mut p = inner.split(','); let w2: i32 = p.next().unwrap_or("0").trim().parse().unwrap_or(0); let h2: i32 = p.next().unwrap_or("0").trim().parse().unwrap_or(0); if w2>0&&h2>0 { gl_obj_geometry(name, w2, h2); } }
}
pub fn ac_gl_obj_color_by_name(name: &str, color: &str) {
    let (r,g,b) = _color_by_name(color); gl_obj_color(name, r, g, b);
}
pub fn ac_gl_obj_pos_from_spec(name: &str, x_spec: &str, y_spec: &str) {
    let resolve = |spec: &str, is_x: bool| -> i32 {
        let sw = gl_screen_w(); let sh = gl_screen_h();
        let ow = gl_obj_w(name); let oh = gl_obj_h(name);
        match spec {
            "center" => if is_x { (sw-ow)/2 } else { (sh-oh)/2 },
            "right"  => sw - ow,
            "bottom" => sh - oh,
            "top"|"left" => 0,
            s if s.starts_with("screen-") => { let dim = if is_x { sw } else { sh }; dim - s[7..].parse().unwrap_or(0) },
            s => s.parse().unwrap_or(0),
        }
    };
    gl_obj_pos(name, resolve(x_spec, true), resolve(y_spec, false));
}
pub fn ac_gl_obj_move_y(name: &str, dy: i32)             { gl_obj_move_y(name, dy) }
pub fn ac_gl_obj_move_x(name: &str, dx: i32)             { gl_obj_move_x(name, dx) }
pub fn ac_gl_obj_vertex(name: &str, vx: f32, vy: f32)    { gl_obj_vertex(name, vx, vy) }
pub fn ac_gl_obj_curveshape(name: &str, expr: &str)       { gl_obj_curveshape(name, expr) }
pub fn ac_gl_obj_circle_fall(name: &str, frac: f32, dir: &str) { gl_obj_circle_fall(name, frac, dir) }
pub fn ac_gl_obj_circle_fell(name: &str, frac: f32, dir: &str) -> bool { gl_obj_circle_fell(name, frac, dir) }
pub fn ac_gl_obj_regen(name: &str)                        { gl_obj_regen(name) }
pub fn ac_gl_obj_set_spawn(name: &str)                    { gl_obj_set_spawn(name) }
pub fn ac_gl_obj_save_spawn(name: &str)                   { gl_obj_set_spawn(name) }
pub fn ac_gl_obj_animate(name: &str, dir: &str, speed: f32) { gl_obj_animate(name, dir, speed) }
pub fn ac_gl_is_obj(name: &str) -> bool                   { gl_is_obj(name) }
pub fn ac_gl_is_draw(name: &str) -> bool                  { gl_is_draw(name) }
pub fn ac_gl_hitbox_overlap(a: &str, b: &str) -> bool     { gl_hitbox_overlap(a, b) }
pub fn ac_gl_hitbox_many_overlap() -> bool                 { gl_hitbox_many_overlap() }
pub fn ac_gl_frame_begin() -> bool                        { gl_frame_begin() }
pub fn ac_gl_frame_update(dt: f32)                        { gl_frame_update(dt) }
pub fn ac_gl_frame_render()                               { gl_frame_render() }
pub fn ac_gl_frame_end()                                  { gl_frame_end() }
pub fn ac_gl_delta_time() -> f32                          { gl_delta_time() }
pub fn ac_gl_key_pressed(k: &str) -> bool                 { gl_key_pressed(k) }
pub fn ac_gl_key_just_pressed(k: &str) -> bool            { gl_key_just_pressed(k) }

fn _color_by_name(c: &str) -> (u8,u8,u8) {
    match c {
        "white"   => (255,255,255), "black"   => (0,0,0),       "red"     => (255,0,0),
        "green"   => (0,255,0),     "blue"    => (0,0,255),      "yellow"  => (255,255,0),
        "cyan"    => (0,255,255),   "magenta" => (255,0,255),    "gray"|"grey" => (128,128,128),
        "orange"  => (255,165,0),   "purple"  => (128,0,128),   "brown"   => (139,69,19),
        "pink"    => (255,182,193), "lime"    => (0,255,127),   "teal"    => (0,128,128),
        _ => (255,255,255),
    }
}
