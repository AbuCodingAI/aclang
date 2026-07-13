// AC ilib: gl — Go CGO FFI (libacgl.so / acgl.dll)
package main

/*
#cgo CFLAGS: -I${SRCDIR}/../
#cgo LDFLAGS: -L${SRCDIR}/../ -lacgl -Wl,-rpath,${SRCDIR}/../
#include "gl_c.h"
#include <stdlib.h>
*/
import "C"
import "unsafe"

func gl_init() bool  { return C.ac_gl_init() != 0 }
func gl_quit()       { C.ac_gl_quit() }

func gl_screen_create(w, h int, title string) bool {
    ct := C.CString(title); defer C.free(unsafe.Pointer(ct))
    return C.ac_gl_screen_create(C.int(w), C.int(h), ct) != 0
}
func gl_screen_set_bg(r, g, b uint8)  { C.ac_gl_screen_set_bg(C.uint8_t(r), C.uint8_t(g), C.uint8_t(b)) }
func gl_screen_set_fps(fps int)        { C.ac_gl_screen_set_fps(C.int(fps)) }
func gl_screen_w() int                 { return int(C.ac_gl_screen_w()) }
func gl_screen_h() int                 { return int(C.ac_gl_screen_h()) }

func _cn(name string) *C.char { return C.CString(name) }

func gl_obj_create(name string)                { cn := _cn(name); defer C.free(unsafe.Pointer(cn)); C.ac_gl_obj_create(cn) }
func gl_obj_geometry(name string, w, h int)   { cn := _cn(name); defer C.free(unsafe.Pointer(cn)); C.ac_gl_obj_geometry(cn, C.int(w), C.int(h)) }
func gl_obj_square(name string, size int)      { cn := _cn(name); defer C.free(unsafe.Pointer(cn)); C.ac_gl_obj_square(cn, C.int(size)) }
func gl_obj_pos(name string, x, y int)         { cn := _cn(name); defer C.free(unsafe.Pointer(cn)); C.ac_gl_obj_pos(cn, C.int(x), C.int(y)) }
func gl_obj_color(name string, r, g, b uint8) { cn := _cn(name); defer C.free(unsafe.Pointer(cn)); C.ac_gl_obj_color(cn, C.uint8_t(r), C.uint8_t(g), C.uint8_t(b)) }
func gl_obj_velocity(name string, vx, vy float32) { cn := _cn(name); defer C.free(unsafe.Pointer(cn)); C.ac_gl_obj_velocity(cn, C.float(vx), C.float(vy)) }
func gl_obj_set_speed(name string, s float32)      { cn := _cn(name); defer C.free(unsafe.Pointer(cn)); C.ac_gl_obj_set_speed(cn, C.float(s)) }
func gl_obj_set_direction(name string, deg float32){ cn := _cn(name); defer C.free(unsafe.Pointer(cn)); C.ac_gl_obj_set_direction(cn, C.float(deg)) }
func gl_obj_speed_mult(name string, m float32)     { cn := _cn(name); defer C.free(unsafe.Pointer(cn)); C.ac_gl_obj_speed_mult(cn, C.float(m)) }
func gl_obj_move_x(name string, dx int)           { cn := _cn(name); defer C.free(unsafe.Pointer(cn)); C.ac_gl_obj_move_x(cn, C.int(dx)) }
func gl_obj_move_y(name string, dy int)           { cn := _cn(name); defer C.free(unsafe.Pointer(cn)); C.ac_gl_obj_move_y(cn, C.int(dy)) }
func gl_obj_x(name string) int                    { cn := _cn(name); defer C.free(unsafe.Pointer(cn)); return int(C.ac_gl_obj_x(cn)) }
func gl_obj_y(name string) int                    { cn := _cn(name); defer C.free(unsafe.Pointer(cn)); return int(C.ac_gl_obj_y(cn)) }
func gl_obj_w(name string) int                    { cn := _cn(name); defer C.free(unsafe.Pointer(cn)); return int(C.ac_gl_obj_w(cn)) }
func gl_obj_h(name string) int                    { cn := _cn(name); defer C.free(unsafe.Pointer(cn)); return int(C.ac_gl_obj_h(cn)) }
func gl_obj_curveshape(name, expr string) {
    cn, ce := _cn(name), _cn(expr); defer C.free(unsafe.Pointer(cn)); defer C.free(unsafe.Pointer(ce))
    C.ac_gl_obj_curveshape(cn, ce)
}
func gl_obj_vertex(name string, vx, vy float32) {
    cn := _cn(name); defer C.free(unsafe.Pointer(cn)); C.ac_gl_obj_vertex(cn, C.float(vx), C.float(vy))
}
func gl_obj_to_draw(name string) { cn := _cn(name); defer C.free(unsafe.Pointer(cn)); C.ac_gl_obj_to_draw(cn) }
func gl_obj_circle_fall(name string, frac float32, dir string) {
    cn, cd := _cn(name), _cn(dir); defer C.free(unsafe.Pointer(cn)); defer C.free(unsafe.Pointer(cd))
    C.ac_gl_obj_circle_fall(cn, C.float(frac), cd)
}
func gl_obj_circle_fell(name string) bool { cn := _cn(name); defer C.free(unsafe.Pointer(cn)); return C.ac_gl_obj_circle_fell(cn) != 0 }
func gl_obj_set_spawn(name string)        { cn := _cn(name); defer C.free(unsafe.Pointer(cn)); C.ac_gl_obj_set_spawn(cn) }
func gl_obj_regen(name string)            { cn := _cn(name); defer C.free(unsafe.Pointer(cn)); C.ac_gl_obj_regen(cn) }
func gl_obj_animate(name, dir string, speed float32) {
    cn, cd := _cn(name), _cn(dir); defer C.free(unsafe.Pointer(cn)); defer C.free(unsafe.Pointer(cd))
    C.ac_gl_obj_animate(cn, cd, C.float(speed))
}
func gl_hitbox_many_overlap() bool { return C.ac_gl_hitbox_many_overlap() != 0 }

func gl_draw_create(name string) { cn := _cn(name); defer C.free(unsafe.Pointer(cn)); C.ac_gl_draw_create(cn) }
func gl_draw_curveshape(name, expr string) {
    cn, ce := _cn(name), _cn(expr); defer C.free(unsafe.Pointer(cn)); defer C.free(unsafe.Pointer(ce))
    C.ac_gl_draw_curveshape(cn, ce)
}
func gl_draw_vertex(name string, vx, vy float32) {
    cn := _cn(name); defer C.free(unsafe.Pointer(cn)); C.ac_gl_draw_vertex(cn, C.float(vx), C.float(vy))
}
func gl_draw_line(name string, x1, y1, x2, y2 float32, r, g, b uint8) {
    cn := _cn(name); defer C.free(unsafe.Pointer(cn))
    C.ac_gl_draw_line(cn, C.float(x1), C.float(y1), C.float(x2), C.float(y2), C.uint8_t(r), C.uint8_t(g), C.uint8_t(b))
}
func gl_draw_circle(name string, cx, cy, radius float32, r, g, b uint8) {
    cn := _cn(name); defer C.free(unsafe.Pointer(cn))
    C.ac_gl_draw_circle(cn, C.float(cx), C.float(cy), C.float(radius), C.uint8_t(r), C.uint8_t(g), C.uint8_t(b))
}
func gl_draw_clear(name string)  { cn := _cn(name); defer C.free(unsafe.Pointer(cn)); C.ac_gl_draw_clear(cn) }
func gl_draw_to_obj(name string) { cn := _cn(name); defer C.free(unsafe.Pointer(cn)); C.ac_gl_draw_to_obj(cn) }
func gl_is_draw(name string) bool { cn := _cn(name); defer C.free(unsafe.Pointer(cn)); return C.ac_gl_is_draw(cn) != 0 }
func gl_is_obj(name string) bool  { cn := _cn(name); defer C.free(unsafe.Pointer(cn)); return C.ac_gl_is_obj(cn) != 0 }

func gl_hitbox_overlap(a, b string) bool {
    ca, cb := _cn(a), _cn(b); defer C.free(unsafe.Pointer(ca)); defer C.free(unsafe.Pointer(cb))
    return C.ac_gl_hitbox_overlap(ca, cb) != 0
}
func gl_hitbox_boundary(name string) bool {
    cn := _cn(name); defer C.free(unsafe.Pointer(cn))
    return C.ac_gl_hitbox_overlap_boundary(cn) != 0
}
func gl_hitbox_overlap_pattern(name, pat string) bool {
    cn, cp := _cn(name), _cn(pat); defer C.free(unsafe.Pointer(cn)); defer C.free(unsafe.Pointer(cp))
    return C.ac_gl_hitbox_overlap_pattern(cn, cp) != 0
}

func gl_key_pressed(k string) bool {
    ck := _cn(k); defer C.free(unsafe.Pointer(ck))
    return C.ac_gl_key_pressed(ck) != 0
}
func gl_key_just_pressed(k string) bool {
    ck := _cn(k); defer C.free(unsafe.Pointer(ck))
    return C.ac_gl_key_just_pressed(ck) != 0
}

func gl_frame_begin() bool      { return C.ac_gl_frame_begin() != 0 }
func gl_frame_update(dt float32) { C.ac_gl_frame_update(C.float(dt)) }
func gl_frame_render()           { C.ac_gl_frame_render() }
func gl_frame_end()              { C.ac_gl_frame_end() }
func gl_delta_time() float32     { return float32(C.ac_gl_delta_time()) }

// ── Namespace structs — AC-generated Go uses gl.screen.create(...) etc. ──

type _AcGlScreen struct{}
func (_AcGlScreen) create(w, h int, title string) bool  { return gl_screen_create(w, h, title) }
func (_AcGlScreen) set_bg(r, g, b uint8)                { gl_screen_set_bg(r, g, b) }
func (_AcGlScreen) set_fps(fps int)                     { gl_screen_set_fps(fps) }
func (_AcGlScreen) w() int                              { return gl_screen_w() }
func (_AcGlScreen) h() int                              { return gl_screen_h() }

type _AcGlObj struct{}
func (_AcGlObj) create(name string)                    { gl_obj_create(name) }
func (_AcGlObj) geometry(name string, w, h int)        { gl_obj_geometry(name, w, h) }
func (_AcGlObj) square(name string, size int)           { gl_obj_square(name, size) }
func (_AcGlObj) pos(name string, x, y int)              { gl_obj_pos(name, x, y) }
func (_AcGlObj) color(name string, r, g, b uint8)      { gl_obj_color(name, r, g, b) }
func (_AcGlObj) velocity(name string, vx, vy float32)  { gl_obj_velocity(name, vx, vy) }
func (_AcGlObj) set_speed(name string, s float32)       { gl_obj_set_speed(name, s) }
func (_AcGlObj) set_direction(name string, d float32)   { gl_obj_set_direction(name, d) }
func (_AcGlObj) speed_mult(name string, m float32)      { gl_obj_speed_mult(name, m) }
func (_AcGlObj) move_x(name string, dx int)            { gl_obj_move_x(name, dx) }
func (_AcGlObj) move_y(name string, dy int)            { gl_obj_move_y(name, dy) }
func (_AcGlObj) x(name string) int                     { return gl_obj_x(name) }
func (_AcGlObj) y(name string) int                     { return gl_obj_y(name) }
func (_AcGlObj) w(name string) int                     { return gl_obj_w(name) }
func (_AcGlObj) h(name string) int                     { return gl_obj_h(name) }
func (_AcGlObj) curveshape(name, expr string)                   { gl_obj_curveshape(name, expr) }
func (_AcGlObj) vertex(name string, vx, vy float32)            { gl_obj_vertex(name, vx, vy) }
func (_AcGlObj) to_draw(name string)                            { gl_obj_to_draw(name) }
func (_AcGlObj) circle_fall(name string, f float32, dir string) { gl_obj_circle_fall(name, f, dir) }
func (_AcGlObj) circle_fell(name string) bool                   { return gl_obj_circle_fell(name) }
func (_AcGlObj) set_spawn(name string)                          { gl_obj_set_spawn(name) }
func (_AcGlObj) regen(name string)                              { gl_obj_regen(name) }
func (_AcGlObj) animate(name, dir string, speed float32)        { gl_obj_animate(name, dir, speed) }

type _AcGlDraw struct{}
func (_AcGlDraw) create(name string)                                          { gl_draw_create(name) }
func (_AcGlDraw) curveshape(name, expr string)                                { gl_draw_curveshape(name, expr) }
func (_AcGlDraw) vertex(name string, vx, vy float32)                         { gl_draw_vertex(name, vx, vy) }
func (_AcGlDraw) line(name string, x1, y1, x2, y2 float32, r, g, b uint8)   { gl_draw_line(name, x1, y1, x2, y2, r, g, b) }
func (_AcGlDraw) circle(name string, cx, cy, rad float32, r, g, b uint8)     { gl_draw_circle(name, cx, cy, rad, r, g, b) }
func (_AcGlDraw) clear(name string)                                           { gl_draw_clear(name) }
func (_AcGlDraw) to_obj(name string)                                          { gl_draw_to_obj(name) }

type _AcGlHitbox struct{}
func (_AcGlHitbox) overlap(a, b string) bool            { return gl_hitbox_overlap(a, b) }
func (_AcGlHitbox) boundary(name string) bool           { return gl_hitbox_boundary(name) }
func (_AcGlHitbox) overlap_pattern(n, pat string) bool  { return gl_hitbox_overlap_pattern(n, pat) }
func (_AcGlHitbox) many_overlap() bool                  { return gl_hitbox_many_overlap() }

type _AcGlKey struct{}
func (_AcGlKey) pressed(k string) bool      { return gl_key_pressed(k) }
func (_AcGlKey) just_pressed(k string) bool { return gl_key_just_pressed(k) }

type _AcGlFrame struct{}
func (_AcGlFrame) begin() bool          { return gl_frame_begin() }
func (_AcGlFrame) update(dt float32)    { gl_frame_update(dt) }
func (_AcGlFrame) render()              { gl_frame_render() }
func (_AcGlFrame) end()                 { gl_frame_end() }
func (_AcGlFrame) delta() float32       { return gl_delta_time() }

type _AcGlNS struct {
    screen _AcGlScreen
    obj    _AcGlObj
    draw   _AcGlDraw
    hitbox _AcGlHitbox
    key    _AcGlKey
    frame  _AcGlFrame
}
func (_AcGlNS) init() bool              { return gl_init() }
func (_AcGlNS) quit()                   { gl_quit() }
func (_AcGlNS) is_draw(name string) bool { return gl_is_draw(name) }
func (_AcGlNS) is_obj(name string) bool  { return gl_is_obj(name) }

var gl = _AcGlNS{}
