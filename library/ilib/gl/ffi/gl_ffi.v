// AC ilib: gl — V FFI (libacgl.so / acgl.dll)
module main

#flag -L @VMODROOT/../
#flag -lacgl
#flag -Wl,-rpath,@VMODROOT/../
#include "gl_c.h"

fn C.ac_gl_init() int
fn C.ac_gl_quit()

fn C.ac_gl_screen_create(w int, h int, title &char) int
fn C.ac_gl_screen_set_bg(r u8, g u8, b u8)
fn C.ac_gl_screen_set_fps(fps int)
fn C.ac_gl_screen_w() int
fn C.ac_gl_screen_h() int

fn C.ac_gl_obj_create(name &char)
fn C.ac_gl_obj_geometry(name &char, w int, h int)
fn C.ac_gl_obj_square(name &char, size int)
fn C.ac_gl_obj_pos(name &char, x int, y int)
fn C.ac_gl_obj_color(name &char, r u8, g u8, b u8)
fn C.ac_gl_obj_velocity(name &char, vx f32, vy f32)
fn C.ac_gl_obj_set_speed(name &char, speed f32)
fn C.ac_gl_obj_set_direction(name &char, deg f32)
fn C.ac_gl_obj_speed_mult(name &char, mult f32)
fn C.ac_gl_obj_move_x(name &char, dx int)
fn C.ac_gl_obj_move_y(name &char, dy int)
fn C.ac_gl_obj_x(name &char) int
fn C.ac_gl_obj_y(name &char) int
fn C.ac_gl_obj_w(name &char) int
fn C.ac_gl_obj_h(name &char) int
fn C.ac_gl_obj_curveshape(name &char, expr &char)
fn C.ac_gl_obj_vertex(name &char, vx f32, vy f32)
fn C.ac_gl_obj_to_draw(name &char)
fn C.ac_gl_obj_circle_fall(name &char, fraction f32, dir &char)
fn C.ac_gl_obj_circle_fell(name &char) int
fn C.ac_gl_obj_set_spawn(name &char)
fn C.ac_gl_obj_regen(name &char)
fn C.ac_gl_obj_animate(name &char, dir &char, speed f32)
fn C.ac_gl_hitbox_many_overlap() int

fn C.ac_gl_draw_create(name &char)
fn C.ac_gl_draw_curveshape(name &char, expr &char)
fn C.ac_gl_draw_vertex(name &char, vx f32, vy f32)
fn C.ac_gl_draw_line(name &char, x1 f32, y1 f32, x2 f32, y2 f32, r u8, g u8, b u8)
fn C.ac_gl_draw_circle(name &char, cx f32, cy f32, radius f32, r u8, g u8, b u8)
fn C.ac_gl_draw_clear(name &char)
fn C.ac_gl_draw_to_obj(name &char)
fn C.ac_gl_is_draw(name &char) int
fn C.ac_gl_is_obj(name &char) int

fn C.ac_gl_hitbox_overlap(a &char, b &char) int
fn C.ac_gl_hitbox_overlap_boundary(name &char) int
fn C.ac_gl_hitbox_overlap_pattern(name &char, pat &char) int

fn C.ac_gl_key_pressed(key &char) int
fn C.ac_gl_key_just_pressed(key &char) int

fn C.ac_gl_frame_begin() int
fn C.ac_gl_frame_update(dt f32)
fn C.ac_gl_frame_render()
fn C.ac_gl_frame_end()
fn C.ac_gl_delta_time() f32

fn gl_init() bool  { return C.ac_gl_init() != 0 }
fn gl_quit()       { C.ac_gl_quit() }

fn gl_screen_create(w int, h int, title string) bool { return C.ac_gl_screen_create(w, h, title.str) != 0 }
fn gl_screen_set_bg(r u8, g u8, b u8)                { C.ac_gl_screen_set_bg(r, g, b) }
fn gl_screen_set_fps(fps int)                          { C.ac_gl_screen_set_fps(fps) }
fn gl_screen_w() int                                   { return C.ac_gl_screen_w() }
fn gl_screen_h() int                                   { return C.ac_gl_screen_h() }

fn gl_obj_create(name string)                    { C.ac_gl_obj_create(name.str) }
fn gl_obj_geometry(name string, w int, h int)    { C.ac_gl_obj_geometry(name.str, w, h) }
fn gl_obj_square(name string, size int)           { C.ac_gl_obj_square(name.str, size) }
fn gl_obj_pos(name string, x int, y int)          { C.ac_gl_obj_pos(name.str, x, y) }
fn gl_obj_color(name string, r u8, g u8, b u8)   { C.ac_gl_obj_color(name.str, r, g, b) }
fn gl_obj_velocity(name string, vx f32, vy f32)  { C.ac_gl_obj_velocity(name.str, vx, vy) }
fn gl_obj_set_speed(name string, s f32)           { C.ac_gl_obj_set_speed(name.str, s) }
fn gl_obj_set_direction(name string, deg f32)     { C.ac_gl_obj_set_direction(name.str, deg) }
fn gl_obj_speed_mult(name string, m f32)          { C.ac_gl_obj_speed_mult(name.str, m) }
fn gl_obj_move_x(name string, dx int)            { C.ac_gl_obj_move_x(name.str, dx) }
fn gl_obj_move_y(name string, dy int)            { C.ac_gl_obj_move_y(name.str, dy) }
fn gl_obj_x(name string) int                     { return C.ac_gl_obj_x(name.str) }
fn gl_obj_y(name string) int                     { return C.ac_gl_obj_y(name.str) }
fn gl_obj_w(name string) int                     { return C.ac_gl_obj_w(name.str) }
fn gl_obj_h(name string) int                     { return C.ac_gl_obj_h(name.str) }
fn gl_obj_curveshape(name string, expr string)   { C.ac_gl_obj_curveshape(name.str, expr.str) }
fn gl_obj_vertex(name string, vx f32, vy f32)   { C.ac_gl_obj_vertex(name.str, vx, vy) }
fn gl_obj_to_draw(name string)                         { C.ac_gl_obj_to_draw(name.str) }
fn gl_obj_circle_fall(name string, frac f32, dir string) { C.ac_gl_obj_circle_fall(name.str, frac, dir.str) }
fn gl_obj_circle_fell(name string) bool                { return C.ac_gl_obj_circle_fell(name.str) != 0 }
fn gl_obj_set_spawn(name string)                       { C.ac_gl_obj_set_spawn(name.str) }
fn gl_obj_regen(name string)                           { C.ac_gl_obj_regen(name.str) }
fn gl_obj_animate(name string, dir string, speed f32)  { C.ac_gl_obj_animate(name.str, dir.str, speed) }
fn gl_hitbox_many_overlap() bool                       { return C.ac_gl_hitbox_many_overlap() != 0 }

fn gl_draw_create(name string)                   { C.ac_gl_draw_create(name.str) }
fn gl_draw_curveshape(name string, expr string)  { C.ac_gl_draw_curveshape(name.str, expr.str) }
fn gl_draw_vertex(name string, vx f32, vy f32)  { C.ac_gl_draw_vertex(name.str, vx, vy) }
fn gl_draw_line(name string, x1 f32, y1 f32, x2 f32, y2 f32, r u8, g u8, b u8) {
    C.ac_gl_draw_line(name.str, x1, y1, x2, y2, r, g, b)
}
fn gl_draw_circle(name string, cx f32, cy f32, radius f32, r u8, g u8, b u8) {
    C.ac_gl_draw_circle(name.str, cx, cy, radius, r, g, b)
}
fn gl_draw_clear(name string)                    { C.ac_gl_draw_clear(name.str) }
fn gl_draw_to_obj(name string)                   { C.ac_gl_draw_to_obj(name.str) }
fn gl_is_draw(name string) bool                  { return C.ac_gl_is_draw(name.str) != 0 }
fn gl_is_obj(name string) bool                   { return C.ac_gl_is_obj(name.str) != 0 }

fn gl_hitbox_overlap(a string, b string) bool             { return C.ac_gl_hitbox_overlap(a.str, b.str) != 0 }
fn gl_hitbox_boundary(name string) bool                   { return C.ac_gl_hitbox_overlap_boundary(name.str) != 0 }
fn gl_hitbox_overlap_pattern(name string, pat string) bool { return C.ac_gl_hitbox_overlap_pattern(name.str, pat.str) != 0 }

fn gl_key_pressed(k string) bool      { return C.ac_gl_key_pressed(k.str) != 0 }
fn gl_key_just_pressed(k string) bool { return C.ac_gl_key_just_pressed(k.str) != 0 }

fn gl_frame_begin() bool    { return C.ac_gl_frame_begin() != 0 }
fn gl_frame_update(dt f32)  { C.ac_gl_frame_update(dt) }
fn gl_frame_render()        { C.ac_gl_frame_render() }
fn gl_frame_end()           { C.ac_gl_frame_end() }
fn gl_delta_time() f32      { return C.ac_gl_delta_time() }

// ── Namespace struct — AC-generated V uses gl.screen.create(...) etc. ──

struct AcGlScreen {}
fn (o AcGlScreen) create(w int, h int, title string) bool { return gl_screen_create(w, h, title) }
fn (o AcGlScreen) set_bg(r u8, g u8, b u8)                { gl_screen_set_bg(r, g, b) }
fn (o AcGlScreen) set_fps(fps int)                          { gl_screen_set_fps(fps) }
fn (o AcGlScreen) w() int                                   { return gl_screen_w() }
fn (o AcGlScreen) h() int                                   { return gl_screen_h() }

struct AcGlObj {}
fn (o AcGlObj) create(name string)                    { gl_obj_create(name) }
fn (o AcGlObj) geometry(name string, w int, h int)    { gl_obj_geometry(name, w, h) }
fn (o AcGlObj) square(name string, size int)           { gl_obj_square(name, size) }
fn (o AcGlObj) pos(name string, x int, y int)          { gl_obj_pos(name, x, y) }
fn (o AcGlObj) color(name string, r u8, g u8, b u8)   { gl_obj_color(name, r, g, b) }
fn (o AcGlObj) velocity(name string, vx f32, vy f32)  { gl_obj_velocity(name, vx, vy) }
fn (o AcGlObj) set_speed(name string, s f32)           { gl_obj_set_speed(name, s) }
fn (o AcGlObj) set_direction(name string, d f32)       { gl_obj_set_direction(name, d) }
fn (o AcGlObj) speed_mult(name string, m f32)          { gl_obj_speed_mult(name, m) }
fn (o AcGlObj) move_x(name string, dx int)            { gl_obj_move_x(name, dx) }
fn (o AcGlObj) move_y(name string, dy int)            { gl_obj_move_y(name, dy) }
fn (o AcGlObj) x(name string) int                     { return gl_obj_x(name) }
fn (o AcGlObj) y(name string) int                     { return gl_obj_y(name) }
fn (o AcGlObj) w(name string) int                     { return gl_obj_w(name) }
fn (o AcGlObj) h(name string) int                     { return gl_obj_h(name) }
fn (o AcGlObj) curveshape(name string, expr string)   { gl_obj_curveshape(name, expr) }
fn (o AcGlObj) vertex(name string, vx f32, vy f32)   { gl_obj_vertex(name, vx, vy) }
fn (o AcGlObj) to_draw(name string)                            { gl_obj_to_draw(name) }
fn (o AcGlObj) circle_fall(name string, frac f32, dir string)  { gl_obj_circle_fall(name, frac, dir) }
fn (o AcGlObj) circle_fell(name string) bool                   { return gl_obj_circle_fell(name) }
fn (o AcGlObj) set_spawn(name string)                          { gl_obj_set_spawn(name) }
fn (o AcGlObj) regen(name string)                              { gl_obj_regen(name) }
fn (o AcGlObj) animate(name string, dir string, speed f32)     { gl_obj_animate(name, dir, speed) }

struct AcGlDraw {}
fn (o AcGlDraw) create(name string)                                                    { gl_draw_create(name) }
fn (o AcGlDraw) curveshape(name string, expr string)                                   { gl_draw_curveshape(name, expr) }
fn (o AcGlDraw) vertex(name string, vx f32, vy f32)                                   { gl_draw_vertex(name, vx, vy) }
fn (o AcGlDraw) line(name string, x1 f32, y1 f32, x2 f32, y2 f32, r u8, g u8, b u8) { gl_draw_line(name, x1, y1, x2, y2, r, g, b) }
fn (o AcGlDraw) circle(name string, cx f32, cy f32, rad f32, r u8, g u8, b u8)       { gl_draw_circle(name, cx, cy, rad, r, g, b) }
fn (o AcGlDraw) clear(name string)                                                     { gl_draw_clear(name) }
fn (o AcGlDraw) to_obj(name string)                                                    { gl_draw_to_obj(name) }

struct AcGlHitbox {}
fn (o AcGlHitbox) overlap(a string, b string) bool            { return gl_hitbox_overlap(a, b) }
fn (o AcGlHitbox) boundary(name string) bool                  { return gl_hitbox_boundary(name) }
fn (o AcGlHitbox) overlap_pattern(n string, pat string) bool  { return gl_hitbox_overlap_pattern(n, pat) }
fn (o AcGlHitbox) many_overlap() bool                         { return gl_hitbox_many_overlap() }

struct AcGlKey {}
fn (o AcGlKey) pressed(k string) bool      { return gl_key_pressed(k) }
fn (o AcGlKey) just_pressed(k string) bool { return gl_key_just_pressed(k) }

struct AcGlFrame {}
fn (o AcGlFrame) begin() bool    { return gl_frame_begin() }
fn (o AcGlFrame) update(dt f32)  { gl_frame_update(dt) }
fn (o AcGlFrame) render()        { gl_frame_render() }
fn (o AcGlFrame) end()           { gl_frame_end() }
fn (o AcGlFrame) delta() f32     { return gl_delta_time() }

struct AcGlNS {
    screen AcGlScreen
    obj    AcGlObj
    draw   AcGlDraw
    hitbox AcGlHitbox
    key    AcGlKey
    frame  AcGlFrame
}
fn (o AcGlNS) init() bool               { return gl_init() }
fn (o AcGlNS) quit()                    { gl_quit() }
fn (o AcGlNS) is_draw(name string) bool { return gl_is_draw(name) }
fn (o AcGlNS) is_obj(name string) bool  { return gl_is_obj(name) }

const gl = AcGlNS{}
