/* AC ilib: gl — C API header (libacgl.so / acgl.dll)
   All backends link to this shared library.
   use ilib gl
*/
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>

/* ── Init / shutdown ─────────────────────────────────────────────────────── */
int  ac_gl_init(void);
void ac_gl_quit(void);

/* ── Screen ──────────────────────────────────────────────────────────────── */
int  ac_gl_screen_create(int w, int h, const char* title);
void ac_gl_screen_set_bg(uint8_t r, uint8_t g, uint8_t b);
void ac_gl_screen_set_fps(int fps);
int  ac_gl_screen_w(void);
int  ac_gl_screen_h(void);

/* ── Objects ─────────────────────────────────────────────────────────────── */
void ac_gl_obj_create(const char* name);
void ac_gl_obj_geometry(const char* name, int w, int h);   /* rectangle */
void ac_gl_obj_square(const char* name, int size);          /* square shorthand */
void ac_gl_obj_pos(const char* name, int x, int y);
void ac_gl_obj_color(const char* name, uint8_t r, uint8_t g, uint8_t b);
void ac_gl_obj_velocity(const char* name, float vx, float vy); /* px/sec */
void ac_gl_obj_set_speed(const char* name, float speed);       /* px/sec, uses direction */
void ac_gl_obj_set_direction(const char* name, float deg);     /* degrees; 0=up, 90=right */
void ac_gl_obj_speed_mult(const char* name, float mult);       /* speed *= mult (ball.speed@=-1) */
void ac_gl_obj_move_x(const char* name, int dx);
void ac_gl_obj_move_y(const char* name, int dy);
int  ac_gl_obj_x(const char* name);
int  ac_gl_obj_y(const char* name);
int  ac_gl_obj_w(const char* name);
int  ac_gl_obj_h(const char* name);

/* ── Curve shape / trajectory (objects) ─────────────────────────────────── */
void ac_gl_obj_curveshape(const char* name, const char* expr);
void ac_gl_obj_vertex(const char* name, float vx, float vy);

/* ── CircleFall animation ────────────────────────────────────────────────── */
void ac_gl_obj_circle_fall(const char* name, float fraction, const char* dir);
int  ac_gl_obj_circle_fell(const char* name, float fraction, const char* dir);

/* ── Spawn / regen ───────────────────────────────────────────────────────── */
void ac_gl_obj_set_spawn(const char* name);
void ac_gl_obj_regen(const char* name);

/* ── Direction animate ───────────────────────────────────────────────────── */
void ac_gl_obj_animate(const char* name, const char* dir, float speed);

/* ── Many-overlap query ──────────────────────────────────────────────────── */
int  ac_gl_hitbox_many_overlap(void);

/* ── Draw type — pure geometry, zero object properties ──────────────────── */
void ac_gl_draw_create(const char* name);
void ac_gl_draw_curveshape(const char* name, const char* expr);
void ac_gl_draw_vertex(const char* name, float vx, float vy);
void ac_gl_draw_line(const char* name, float x1, float y1, float x2, float y2,
                     uint8_t r, uint8_t g, uint8_t b);
void ac_gl_draw_circle(const char* name, float cx, float cy, float radius,
                       uint8_t r, uint8_t g, uint8_t b);
void ac_gl_draw_clear(const char* name);   /* remove all draw commands */

/* ── Type conversion ─────────────────────────────────────────────────────── */
void ac_gl_obj_to_draw(const char* name);  /* object → draw (strips all obj properties) */
void ac_gl_draw_to_obj(const char* name);  /* draw → object (gives default obj properties) */
int  ac_gl_is_draw(const char* name);      /* 1 if name is draw type */
int  ac_gl_is_obj(const char* name);       /* 1 if name is obj type */

/* ── Collision ───────────────────────────────────────────────────────────── */
/* Returns 1 if bounding boxes overlap, 0 otherwise */
int  ac_gl_hitbox_overlap(const char* a, const char* b);
/* 1 if object touches any screen edge */
int  ac_gl_hitbox_overlap_boundary(const char* name);
/* Wildcard: "p%" matches all objects whose name starts with "p" */
int  ac_gl_hitbox_overlap_pattern(const char* name, const char* pattern);

/* ── Input ───────────────────────────────────────────────────────────────── */
/* Key names: "w","a","s","d","up","down","left","right","space","enter","escape" */
int  ac_gl_key_pressed(const char* key);       /* held this frame */
int  ac_gl_key_just_pressed(const char* key);  /* pressed this frame only */

/* ── Frame loop ──────────────────────────────────────────────────────────── */
/* Typical loop: while(ac_gl_frame_begin()) {
       dt = ac_gl_delta_time();
       ac_gl_frame_update(dt);
       // your logic here
       ac_gl_frame_render();
       ac_gl_frame_end();
   } */
int   ac_gl_frame_begin(void);       /* poll events; returns 0 when window closed */
void  ac_gl_frame_update(float dt);  /* move all objects by their velocity * dt */
void  ac_gl_frame_render(void);      /* clear + draw all objects */
void  ac_gl_frame_end(void);         /* present + frame-rate cap */
float ac_gl_delta_time(void);        /* seconds since last frame_begin */

/* ── High-level helpers (used by AC-compiled code on all backends) ─────────── */
/* Combines ac_gl_init + ac_gl_screen_create */
void ac_gl_screen_init(int w, int h, const char* title);
/* Set FPS from string like "60fps" */
void ac_gl_screen_animate(const char* fps_spec);
/* Set background by color name ("green", "black", etc.) */
void ac_gl_screen_set_bg_by_name(const char* color_name);
/* Set object color by name */
void ac_gl_obj_color_by_name(const char* name, const char* color_name);
/* Set position from string specs like "center", "right", "screen-50" */
void ac_gl_obj_pos_from_spec(const char* name, const char* x_spec, const char* y_spec);
/* Configure item shape from spec string like "square(50)", "geometry(100 @ 300)" */
void ac_gl_obj_config_item(const char* name, const char* item_spec);
/* Save current position as spawn point */
void ac_gl_obj_save_spawn(const char* name);

#ifdef __cplusplus
}
#endif
