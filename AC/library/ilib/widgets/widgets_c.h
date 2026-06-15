/* AC ilib: widgets — C API header (libacwidgets.so / acwidgets.dll)
   All backends link to this shared library.
   use ilib widgets
   Link: gcc output.c -L./library/widgets -lacwidgets $(pkg-config --cflags gtk+-3.0)
*/
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>

typedef intptr_t ac_widget_t;  /* opaque handle returned by every constructor */

/* ── Init ────────────────────────────────────────────────────────────────── */
void ac_widgets_init(void);

/* ── Screen ──────────────────────────────────────────────────────────────── */
ac_widget_t ac_widgets_screen_new(const char* title, const char* geometry);
void        ac_widgets_screen_mainloop(ac_widget_t screen);
void        ac_widgets_screen_update(ac_widget_t screen);
void        ac_widgets_screen_destroy(ac_widget_t screen);

/* ── display (label) ─────────────────────────────────────────────────────── */
ac_widget_t ac_widgets_display_new(ac_widget_t master, const char* text);
void        ac_widgets_display_pack(ac_widget_t w);
void        ac_widgets_display_set(ac_widget_t w, const char* text);
const char* ac_widgets_display_get(ac_widget_t w);

/* ── ask (text entry) ────────────────────────────────────────────────────── */
ac_widget_t ac_widgets_ask_new(ac_widget_t master, int width);
void        ac_widgets_ask_pack(ac_widget_t w);
const char* ac_widgets_ask_get(ac_widget_t w);
void        ac_widgets_ask_set(ac_widget_t w, const char* text);

/* ── btn (button) ────────────────────────────────────────────────────────── */
ac_widget_t ac_widgets_btn_new(ac_widget_t master, const char* text);
void        ac_widgets_btn_pack(ac_widget_t w);
void        ac_widgets_btn_on_click(ac_widget_t w, void (*cb)(void*), void* userdata);

/* ── ckbtn (checkbox) ────────────────────────────────────────────────────── */
ac_widget_t ac_widgets_ckbtn_new(ac_widget_t master, const char* text);
void        ac_widgets_ckbtn_pack(ac_widget_t w);
int         ac_widgets_ckbtn_get(ac_widget_t w);
void        ac_widgets_ckbtn_set(ac_widget_t w, int value);

/* ── dropdown (combobox) ─────────────────────────────────────────────────── */
ac_widget_t ac_widgets_dropdown_new(ac_widget_t master);
void        ac_widgets_dropdown_pack(ac_widget_t w);
void        ac_widgets_dropdown_add(ac_widget_t w, const char* item);
const char* ac_widgets_dropdown_get(ac_widget_t w);
void        ac_widgets_dropdown_set(ac_widget_t w, const char* item);

/* ── advance (progress bar) ──────────────────────────────────────────────── */
ac_widget_t ac_widgets_advance_new(ac_widget_t master, int length);
void        ac_widgets_advance_pack(ac_widget_t w);
void        ac_widgets_advance_set(ac_widget_t w, double value);
double      ac_widgets_advance_get(ac_widget_t w);

/* ── slider (scale) ──────────────────────────────────────────────────────── */
ac_widget_t ac_widgets_slider_new(ac_widget_t master, double from_val, double to_val, const char* orient);
void        ac_widgets_slider_pack(ac_widget_t w);
double      ac_widgets_slider_get(ac_widget_t w);
void        ac_widgets_slider_set(ac_widget_t w, double value);

/* ── group (labelled frame) ──────────────────────────────────────────────── */
ac_widget_t ac_widgets_group_new(ac_widget_t master, const char* text);
void        ac_widgets_group_pack(ac_widget_t w);

/* ── listbox ─────────────────────────────────────────────────────────────── */
ac_widget_t ac_widgets_listbox_new(ac_widget_t master, int width, int height);
void        ac_widgets_listbox_pack(ac_widget_t w);
void        ac_widgets_listbox_add(ac_widget_t w, const char* item);
const char* ac_widgets_listbox_item(ac_widget_t w, int index);
int         ac_widgets_listbox_count(ac_widget_t w);

/* ── sketch (drawing canvas) ─────────────────────────────────────────────── */
ac_widget_t ac_widgets_sketch_new(ac_widget_t master, int width, int height);
void        ac_widgets_sketch_pack(ac_widget_t w);
void        ac_widgets_sketch_clear(ac_widget_t w);
void        ac_widgets_sketch_line(ac_widget_t w, double x1, double y1, double x2, double y2, uint8_t r, uint8_t g, uint8_t b);
void        ac_widgets_sketch_rect(ac_widget_t w, double x1, double y1, double x2, double y2, uint8_t r, uint8_t g, uint8_t b);
void        ac_widgets_sketch_circle(ac_widget_t w, double cx, double cy, double radius, uint8_t r, uint8_t g, uint8_t b);
void        ac_widgets_sketch_text(ac_widget_t w, double x, double y, const char* text, uint8_t r, uint8_t g, uint8_t b);

/* ── universal helpers ───────────────────────────────────────────────────────── */
void        ac_widgets_pack(ac_widget_t w);
void        ac_widgets_add(ac_widget_t w, const char* item);
void        ac_widgets_set_d(ac_widget_t w, double v);

/* ── lazy / spaced-pack ──────────────────────────────────────────────────────── */
void        ac_widgets_set_lazy(ac_widget_t w);
void        ac_widgets_pack_spaced(ac_widget_t w, int space_x, int space_y);

#ifdef __cplusplus
}
#endif
