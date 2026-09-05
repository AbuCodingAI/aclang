// AC ilib: widgets — V FFI (libacwidgets.so / acwidgets.dll)
// Inlined by AC->V compiler when "use ilib widgets" is declared
module main

#flag -L @AC_LIBDIR@
#flag -lacwidgets
#flag -Wl,-rpath,@AC_LIBDIR@
#include "@AC_LIBDIR@/widgets_c.h"

fn C.ac_widgets_init()

fn C.ac_widgets_screen_new(title &char) isize
fn C.ac_widgets_screen_mainloop(h isize)
fn C.ac_widgets_screen_update(h isize)
fn C.ac_widgets_screen_destroy(h isize)
fn C.ac_widgets_screen_dimensions(h isize, width int, height int)

fn C.ac_widgets_display_new(master isize, text &char) isize
fn C.ac_widgets_display_pack(h isize)
fn C.ac_widgets_display_set(h isize, text &char)
fn C.ac_widgets_display_get(h isize) &char

fn C.ac_widgets_ask_new(master isize, width int) isize
fn C.ac_widgets_ask_pack(h isize)
fn C.ac_widgets_ask_get(h isize) &char
fn C.ac_widgets_ask_set(h isize, text &char)

fn C.ac_widgets_btn_new(master isize, text &char) isize
fn C.ac_widgets_btn_pack(h isize)

fn C.ac_widgets_ckbtn_new(master isize, text &char) isize
fn C.ac_widgets_ckbtn_pack(h isize)
fn C.ac_widgets_ckbtn_get(h isize) int
fn C.ac_widgets_ckbtn_set(h isize, value int)

fn C.ac_widgets_dropdown_new(master isize) isize
fn C.ac_widgets_dropdown_pack(h isize)
fn C.ac_widgets_dropdown_add(h isize, item &char)
fn C.ac_widgets_dropdown_get(h isize) &char
fn C.ac_widgets_dropdown_set(h isize, item &char)

fn C.ac_widgets_advance_new(master isize, length int) isize
fn C.ac_widgets_advance_pack(h isize)
fn C.ac_widgets_advance_set(h isize, value f64)
fn C.ac_widgets_advance_get(h isize) f64

fn C.ac_widgets_slider_new(master isize, from_val f64, to_val f64, orient &char) isize
fn C.ac_widgets_slider_pack(h isize)
fn C.ac_widgets_slider_get(h isize) f64
fn C.ac_widgets_slider_set(h isize, value f64)

fn C.ac_widgets_group_new(master isize, text &char) isize
fn C.ac_widgets_group_pack(h isize)

fn C.ac_widgets_listbox_new(master isize, width int, height int) isize
fn C.ac_widgets_listbox_pack(h isize)
fn C.ac_widgets_listbox_add(h isize, item &char)
fn C.ac_widgets_listbox_item(h isize, index int) &char
fn C.ac_widgets_listbox_count(h isize) int

fn C.ac_widgets_sketch_new(master isize, width int, height int) isize
fn C.ac_widgets_sketch_pack(h isize)
fn C.ac_widgets_sketch_clear(h isize)
fn C.ac_widgets_sketch_line(h isize, x1 f64, y1 f64, x2 f64, y2 f64, r u8, g u8, b u8)
fn C.ac_widgets_sketch_rect(h isize, x1 f64, y1 f64, x2 f64, y2 f64, r u8, g u8, b u8)
fn C.ac_widgets_sketch_circle(h isize, cx f64, cy f64, radius f64, r u8, g u8, b u8)
fn C.ac_widgets_sketch_text(h isize, x f64, y f64, text &char, r u8, g u8, b u8)

fn C.ac_widgets_btn_on_click(h isize, cb fn (voidptr), userdata voidptr)

fn C.ac_widgets_textbox_new(master isize, color &char, font &char) isize
fn C.ac_widgets_textbox_pack(h isize)
fn C.ac_widgets_textbox_write(h isize, text &char)
fn C.ac_widgets_textbox_get(h isize) &char
fn C.ac_widgets_textbox_find(h isize, needle &char) &char
fn C.ac_widgets_textbox_fix(h isize, text &char)

// V's widget FFI had NO button-callback support at all before this (mirrors Go's same
// gap, see widgets_ffi.go's matching comment) — `btn(master, text, Callback)` never wired
// the callback anywhere. ac_widget_callbacks is a simple append-only registry; the
// userdata passed to C is the slice index encoded as a voidptr (not a real pointer — V
// values can move/be collected, so only a stable integer handle is safe to hand to C for
// an indefinite lifetime).
__global ( ac_widget_callbacks []fn () )

fn ac_widget_cb_trampoline(userdata voidptr) {
	id := int(usize(userdata))
	if id >= 0 && id < ac_widget_callbacks.len {
		ac_widget_callbacks[id]()
	}
}

// ── V wrapper types ───────────────────────────────────────────────────────────

// title is mandatory — no geometry positional arg. Use .dimensions(w, h) instead.
// Function name is lowercase (`screen`, not `Screen`) — V hard-rejects capitalized
// function names ("function names cannot contain uppercase letters, use snake_case
// instead"), unlike every other widget ctor here which was already lowercase by AC-level
// convention. VStrategy lowercases "Screen" specifically at the call site to match (see
// its own comment) — the struct type AcScreen itself can stay capitalized, V only
// restricts function names, not type names.
struct AcScreen { h isize }
fn screen(title string) AcScreen {
	C.ac_widgets_init()
	return AcScreen{ h: C.ac_widgets_screen_new(title.str) }
}
fn (s AcScreen) mainloop()  { C.ac_widgets_screen_mainloop(s.h) }
fn (s AcScreen) update()    { C.ac_widgets_screen_update(s.h) }
fn (s AcScreen) dimensions(w int, h int) { C.ac_widgets_screen_dimensions(s.h, w, h) }
fn (s AcScreen) destroy()   { C.ac_widgets_screen_destroy(s.h) }

struct AcDisplay { h isize }
fn display(master AcScreen, text string) AcDisplay {
	return AcDisplay{ h: C.ac_widgets_display_new(master.h, text.str) }
}
fn (d AcDisplay) pack()          { C.ac_widgets_display_pack(d.h) }
fn (d AcDisplay) set(v string)   { C.ac_widgets_display_set(d.h, v.str) }
fn (d AcDisplay) get() string    { return unsafe { cstring_to_vstring(C.ac_widgets_display_get(d.h)) } }
fn (d AcDisplay) config(v string){ C.ac_widgets_display_set(d.h, v.str) }

struct AcAsk { h isize }
fn ask(master AcScreen, width int) AcAsk { return AcAsk{ h: C.ac_widgets_ask_new(master.h, width) } }
fn (a AcAsk) pack()        { C.ac_widgets_ask_pack(a.h) }
fn (a AcAsk) get() string  { return unsafe { cstring_to_vstring(C.ac_widgets_ask_get(a.h)) } }
fn (a AcAsk) set(v string) { C.ac_widgets_ask_set(a.h, v.str) }

struct AcBtn { h isize }
fn btn(master AcScreen, text string) AcBtn { return AcBtn{ h: C.ac_widgets_btn_new(master.h, text.str) } }
fn (b AcBtn) pack() { C.ac_widgets_btn_pack(b.h) }
fn (b AcBtn) on_click(cb fn ()) {
	id := ac_widget_callbacks.len
	ac_widget_callbacks << cb
	C.ac_widgets_btn_on_click(b.h, ac_widget_cb_trampoline, voidptr(usize(id)))
}

struct AcCkbtn { h isize }
fn ckbtn(master AcScreen, text string) AcCkbtn { return AcCkbtn{ h: C.ac_widgets_ckbtn_new(master.h, text.str) } }
fn (c AcCkbtn) pack()       { C.ac_widgets_ckbtn_pack(c.h) }
fn (c AcCkbtn) get() bool   { return C.ac_widgets_ckbtn_get(c.h) != 0 }
fn (c AcCkbtn) set(v bool)  { C.ac_widgets_ckbtn_set(c.h, if v { 1 } else { 0 }) }

struct AcRadbtn { h isize }
fn radbtn(master AcScreen, text string) AcRadbtn { return AcRadbtn{ h: C.ac_widgets_ckbtn_new(master.h, text.str) } }
fn (r AcRadbtn) pack()     { C.ac_widgets_ckbtn_pack(r.h) }
fn (r AcRadbtn) get() bool { return C.ac_widgets_ckbtn_get(r.h) != 0 }

struct AcDropdown { h isize }
fn dropdown(master AcScreen, values string) AcDropdown {
	d := AcDropdown{ h: C.ac_widgets_dropdown_new(master.h) }
	for v in values.split(',') { C.ac_widgets_dropdown_add(d.h, v.trim_space().str) }
	return d
}
fn (d AcDropdown) pack()          { C.ac_widgets_dropdown_pack(d.h) }
fn (d AcDropdown) add(item string) { C.ac_widgets_dropdown_add(d.h, item.str) }
fn (d AcDropdown) get() string    { return unsafe { cstring_to_vstring(C.ac_widgets_dropdown_get(d.h)) } }
fn (d AcDropdown) set(v string)   { C.ac_widgets_dropdown_set(d.h, v.str) }

struct AcAdvance { h isize }
fn advance(master AcScreen, length int) AcAdvance { return AcAdvance{ h: C.ac_widgets_advance_new(master.h, length) } }
fn (a AcAdvance) pack()        { C.ac_widgets_advance_pack(a.h) }
fn (a AcAdvance) set(v f64)    { C.ac_widgets_advance_set(a.h, v) }
fn (a AcAdvance) get() f64     { return C.ac_widgets_advance_get(a.h) }

struct AcSlider { h isize }
fn slider(master AcScreen, from_val f64, to_val f64, orient string) AcSlider {
	return AcSlider{ h: C.ac_widgets_slider_new(master.h, from_val, to_val, orient.str) }
}
fn (s AcSlider) pack()       { C.ac_widgets_slider_pack(s.h) }
fn (s AcSlider) get() f64    { return C.ac_widgets_slider_get(s.h) }
fn (s AcSlider) set(v f64)   { C.ac_widgets_slider_set(s.h, v) }

struct AcGroup { h isize }
fn group(master AcScreen, text string) AcGroup {
	return AcGroup{ h: C.ac_widgets_group_new(master.h, text.str) }
}
fn (g AcGroup) pack() { C.ac_widgets_group_pack(g.h) }

struct AcTabs { h isize }
fn tabs(master AcScreen) AcTabs { return AcTabs{ h: C.ac_widgets_group_new(master.h, ''.str) } }
fn (t AcTabs) pack()              { C.ac_widgets_group_pack(t.h) }
fn (t AcTabs) add_tab(_ string)   {}

struct AcScroller { h isize }
fn scroller(master AcScreen) AcScroller { return AcScroller{ h: C.ac_widgets_group_new(master.h, ''.str) } }
fn (s AcScroller) pack() { C.ac_widgets_group_pack(s.h) }

struct AcTable { h isize }
fn table(master AcScreen) AcTable { return AcTable{ h: C.ac_widgets_listbox_new(master.h, 40, 10) } }
fn (t AcTable) pack()            { C.ac_widgets_listbox_pack(t.h) }
fn (t AcTable) add(row string)   { C.ac_widgets_listbox_add(t.h, row.str) }

struct AcListbox { h isize }
fn listbox(master AcScreen, width int, height int) AcListbox {
	return AcListbox{ h: C.ac_widgets_listbox_new(master.h, width, height) }
}
fn (l AcListbox) pack()            { C.ac_widgets_listbox_pack(l.h) }
fn (l AcListbox) add(item string)  { C.ac_widgets_listbox_add(l.h, item.str) }
fn (l AcListbox) get() []string {
	n := C.ac_widgets_listbox_count(l.h)
	mut out := []string{}
	for i in 0..n { out << unsafe { cstring_to_vstring(C.ac_widgets_listbox_item(l.h, i)) } }
	return out
}

struct AcSketch { h isize }
fn sketch(master AcScreen, width int, height int) AcSketch {
	return AcSketch{ h: C.ac_widgets_sketch_new(master.h, width, height) }
}
fn (s AcSketch) pack()                                               { C.ac_widgets_sketch_pack(s.h) }
fn (s AcSketch) clear()                                              { C.ac_widgets_sketch_clear(s.h) }
fn (s AcSketch) line(x1 f64, y1 f64, x2 f64, y2 f64, r u8, g u8, b u8) { C.ac_widgets_sketch_line(s.h, x1, y1, x2, y2, r, g, b) }
fn (s AcSketch) rect(x1 f64, y1 f64, x2 f64, y2 f64, r u8, g u8, b u8) { C.ac_widgets_sketch_rect(s.h, x1, y1, x2, y2, r, g, b) }
fn (s AcSketch) circle(cx f64, cy f64, rad f64, r u8, g u8, b u8)       { C.ac_widgets_sketch_circle(s.h, cx, cy, rad, r, g, b) }
fn (s AcSketch) text_at(x f64, y f64, t string, r u8, g u8, b u8)       { C.ac_widgets_sketch_text(s.h, x, y, t.str, r, g, b) }

struct AcTextbox { h isize }
fn textbox(master AcScreen, color string, font string) AcTextbox {
	return AcTextbox{ h: C.ac_widgets_textbox_new(master.h, color.str, font.str) }
}
fn (t AcTextbox) pack()             { C.ac_widgets_textbox_pack(t.h) }
fn (t AcTextbox) write(s string)    { C.ac_widgets_textbox_write(t.h, s.str) }
fn (t AcTextbox) get() string       { return unsafe { cstring_to_vstring(C.ac_widgets_textbox_get(t.h)) } }
fn (t AcTextbox) find(needle string) string {
	return unsafe { cstring_to_vstring(C.ac_widgets_textbox_find(t.h, needle.str)) }
}
fn (t AcTextbox) fix(s string)      { C.ac_widgets_textbox_fix(t.h, s.str) }
