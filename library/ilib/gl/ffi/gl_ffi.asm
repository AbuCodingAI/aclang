; AC ilib: gl — x86-64 NASM extern declarations (libacgl.so / acgl.dll)
; Link: nasm -f elf64 output.asm && gcc output.o -L../../library/gl -lacgl -Wl,-rpath,../../library/gl -o output

section .text

; Init / quit
; ac_gl_init()  -> eax: int (1=ok)
; ac_gl_quit()  -> void
extern ac_gl_init
extern ac_gl_quit

; Screen
; ac_gl_screen_create(edi: int w, esi: int h, rdx: const char* title) -> eax: int
extern ac_gl_screen_create
; ac_gl_screen_set_bg(dil: u8 r, sil: u8 g, dl: u8 b) -> void
extern ac_gl_screen_set_bg
; ac_gl_screen_set_fps(edi: int) -> void
extern ac_gl_screen_set_fps
; ac_gl_screen_w() -> eax: int
extern ac_gl_screen_w
; ac_gl_screen_h() -> eax: int
extern ac_gl_screen_h

; Objects — rdi: const char* name
; ac_gl_obj_create(rdi) -> void
extern ac_gl_obj_create
; ac_gl_obj_geometry(rdi, esi: int w, edx: int h) -> void
extern ac_gl_obj_geometry
; ac_gl_obj_square(rdi, esi: int size) -> void
extern ac_gl_obj_square
; ac_gl_obj_pos(rdi, esi: int x, edx: int y) -> void
extern ac_gl_obj_pos
; ac_gl_obj_color(rdi, sil: u8 r, dl: u8 g, cl: u8 b) -> void
extern ac_gl_obj_color
; ac_gl_obj_velocity(rdi, xmm0: float vx, xmm1: float vy) -> void
extern ac_gl_obj_velocity
; ac_gl_obj_set_speed(rdi, xmm0: float speed) -> void
extern ac_gl_obj_set_speed
; ac_gl_obj_set_direction(rdi, xmm0: float deg) -> void
extern ac_gl_obj_set_direction
; ac_gl_obj_speed_mult(rdi, xmm0: float mult) -> void
extern ac_gl_obj_speed_mult
; ac_gl_obj_move_x(rdi, esi: int dx) -> void
extern ac_gl_obj_move_x
; ac_gl_obj_move_y(rdi, esi: int dy) -> void
extern ac_gl_obj_move_y
; ac_gl_obj_x/y/w/h(rdi) -> eax: int
extern ac_gl_obj_x
extern ac_gl_obj_y
extern ac_gl_obj_w
extern ac_gl_obj_h
; ac_gl_obj_curveshape(rdi: name, rsi: const char* expr) -> void
extern ac_gl_obj_curveshape
; ac_gl_obj_vertex(rdi: name, xmm0: float vx, xmm1: float vy) -> void
extern ac_gl_obj_vertex
; ac_gl_obj_to_draw(rdi: name) -> void
extern ac_gl_obj_to_draw

; CircleFall / regen / animate
; ac_gl_obj_circle_fall(rdi: name, xmm0: float fraction, rsi: const char* dir) -> void
extern ac_gl_obj_circle_fall
; ac_gl_obj_circle_fell(rdi: name) -> eax: int (1=done)
extern ac_gl_obj_circle_fell
; ac_gl_obj_set_spawn(rdi: name) -> void
extern ac_gl_obj_set_spawn
; ac_gl_obj_regen(rdi: name) -> void
extern ac_gl_obj_regen
; ac_gl_obj_animate(rdi: name, rsi: const char* dir, xmm0: float speed) -> void
extern ac_gl_obj_animate
; ac_gl_hitbox_many_overlap() -> eax: int (1=many overlapping)
extern ac_gl_hitbox_many_overlap

; Draw type — pure geometry entities
; ac_gl_draw_create(rdi: name) -> void
extern ac_gl_draw_create
; ac_gl_draw_curveshape(rdi: name, rsi: const char* expr) -> void
extern ac_gl_draw_curveshape
; ac_gl_draw_vertex(rdi: name, xmm0: float vx, xmm1: float vy) -> void
extern ac_gl_draw_vertex
; ac_gl_draw_line(rdi: name, xmm0: x1, xmm1: y1, xmm2: x2, xmm3: y2, sil: r, dl: g, cl: b) -> void
extern ac_gl_draw_line
; ac_gl_draw_circle(rdi: name, xmm0: cx, xmm1: cy, xmm2: radius, sil: r, dl: g, cl: b) -> void
extern ac_gl_draw_circle
; ac_gl_draw_clear(rdi: name) -> void
extern ac_gl_draw_clear
; ac_gl_draw_to_obj(rdi: name) -> void
extern ac_gl_draw_to_obj
; ac_gl_is_draw(rdi: name) -> eax: int (1=draw type)
extern ac_gl_is_draw
; ac_gl_is_obj(rdi: name) -> eax: int (1=obj type)
extern ac_gl_is_obj

; Collision
; ac_gl_hitbox_overlap(rdi: const char* a, rsi: const char* b) -> eax: int
extern ac_gl_hitbox_overlap
; ac_gl_hitbox_overlap_boundary(rdi: const char* name) -> eax: int
extern ac_gl_hitbox_overlap_boundary
; ac_gl_hitbox_overlap_pattern(rdi: const char* name, rsi: const char* pat) -> eax: int
extern ac_gl_hitbox_overlap_pattern

; Input
; ac_gl_key_pressed(rdi: const char* key) -> eax: int (1=held)
extern ac_gl_key_pressed
; ac_gl_key_just_pressed(rdi: const char* key) -> eax: int (1=just this frame)
extern ac_gl_key_just_pressed

; Frame loop
; ac_gl_frame_begin() -> eax: int (0=quit)
extern ac_gl_frame_begin
; ac_gl_frame_update(xmm0: float dt) -> void
extern ac_gl_frame_update
; ac_gl_frame_render() -> void
extern ac_gl_frame_render
; ac_gl_frame_end() -> void
extern ac_gl_frame_end
; ac_gl_delta_time() -> xmm0: float
extern ac_gl_delta_time
