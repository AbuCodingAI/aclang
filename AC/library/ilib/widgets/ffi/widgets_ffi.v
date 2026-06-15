// AC ilib: widgets — V FFI (libacwidgets.so / acwidgets.dll)
// Inlined by AC->V compiler when "use ilib widgets" is declared
module main

#flag -L @VMODROOT/../
#flag -lacwidgets
#flag -Wl,-rpath,@VMODROOT/../
#include "widgets_c.h"

fn C.ac_widgets_init()

fn C.ac_widgets_screen_new(title &char, geometry &char) isize
fn C.ac_widgets_screen_mainloop(h isize)
fn C.ac_widgets_screen_update(h isize)
fn C.ac_widgets_screen_destroy(h isize)

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

// ── V wrapper types ───────────────────────────────────────────────────────────

struct AcScreen { _h isize }
fn Screen(title string, geometry string) AcScreen {
	C.ac_widgets_init()
	return AcScreen{ _h: C.ac_widgets_screen_new(title.str, geometry.str) }
}
fn (s AcScreen) mainloop()  { C.ac_widgets_screen_mainloop(s._h) }
fn (s AcScreen) update()    { C.ac_widgets_screen_update(s._h) }
fn (s AcScreen) destroy()   { C.ac_widgets_screen_destroy(s._h) }

struct AcDisplay { _h isize }
fn display(master AcScreen, text string) AcDisplay {
	return AcDisplay{ _h: C.ac_widgets_display_new(master._h, text.str) }
}
fn (d AcDisplay) pack()          { C.ac_widgets_display_pack(d._h) }
fn (d AcDisplay) set(v string)   { C.ac_widgets_display_set(d._h, v.str) }
fn (d AcDisplay) get() string    { return unsafe { cstring_to_vstring(C.ac_widgets_display_get(d._h)) } }
fn (d AcDisplay) config(v string){ C.ac_widgets_display_set(d._h, v.str) }

struct AcAsk { _h isize }
fn ask(master AcScreen, width int) AcAsk { return AcAsk{ _h: C.ac_widgets_ask_new(master._h, width) } }
fn (a AcAsk) pack()        { C.ac_widgets_ask_pack(a._h) }
fn (a AcAsk) get() string  { return unsafe { cstring_to_vstring(C.ac_widgets_ask_get(a._h)) } }
fn (a AcAsk) set(v string) { C.ac_widgets_ask_set(a._h, v.str) }

struct AcBtn { _h isize }
fn btn(master AcScreen, text string) AcBtn { return AcBtn{ _h: C.ac_widgets_btn_new(master._h, text.str) } }
fn (b AcBtn) pack() { C.ac_widgets_btn_pack(b._h) }

struct AcCkbtn { _h isize }
fn ckbtn(master AcScreen, text string) AcCkbtn { return AcCkbtn{ _h: C.ac_widgets_ckbtn_new(master._h, text.str) } }
fn (c AcCkbtn) pack()       { C.ac_widgets_ckbtn_pack(c._h) }
fn (c AcCkbtn) get() bool   { return C.ac_widgets_ckbtn_get(c._h) != 0 }
fn (c AcCkbtn) set(v bool)  { C.ac_widgets_ckbtn_set(c._h, if v { 1 } else { 0 }) }

struct AcRadbtn { _h isize }
fn radbtn(master AcScreen, text string) AcRadbtn { return AcRadbtn{ _h: C.ac_widgets_ckbtn_new(master._h, text.str) } }
fn (r AcRadbtn) pack()     { C.ac_widgets_ckbtn_pack(r._h) }
fn (r AcRadbtn) get() bool { return C.ac_widgets_ckbtn_get(r._h) != 0 }

struct AcDropdown { _h isize }
fn dropdown(master AcScreen, values string) AcDropdown {
	d := AcDropdown{ _h: C.ac_widgets_dropdown_new(master._h) }
	for v in values.split(',') { C.ac_widgets_dropdown_add(d._h, v.trim().str) }
	return d
}
fn (d AcDropdown) pack()        { C.ac_widgets_dropdown_pack(d._h) }
fn (d AcDropdown) get() string  { return unsafe { cstring_to_vstring(C.ac_widgets_dropdown_get(d._h)) } }
fn (d AcDropdown) set(v string) { C.ac_widgets_dropdown_set(d._h, v.str) }

struct AcAdvance { _h isize }
fn advance(master AcScreen, length int) AcAdvance { return AcAdvance{ _h: C.ac_widgets_advance_new(master._h, length) } }
fn (a AcAdvance) pack()        { C.ac_widgets_advance_pack(a._h) }
fn (a AcAdvance) set(v f64)    { C.ac_widgets_advance_set(a._h, v) }
fn (a AcAdvance) get() f64     { return C.ac_widgets_advance_get(a._h) }

struct AcSlider { _h isize }
fn slider(master AcScreen, from_val f64, to_val f64, orient string) AcSlider {
	return AcSlider{ _h: C.ac_widgets_slider_new(master._h, from_val, to_val, orient.str) }
}
fn (s AcSlider) pack()       { C.ac_widgets_slider_pack(s._h) }
fn (s AcSlider) get() f64    { return C.ac_widgets_slider_get(s._h) }
fn (s AcSlider) set(v f64)   { C.ac_widgets_slider_set(s._h, v) }

struct AcGroup { _h isize }
fn group(master AcScreen, text string) AcGroup {
	return AcGroup{ _h: C.ac_widgets_group_new(master._h, text.str) }
}
fn (g AcGroup) pack() { C.ac_widgets_group_pack(g._h) }

struct AcTabs { _h isize }
fn tabs(master AcScreen) AcTabs { return AcTabs{ _h: C.ac_widgets_group_new(master._h, ''.str) } }
fn (t AcTabs) pack()              { C.ac_widgets_group_pack(t._h) }
fn (t AcTabs) add_tab(_ string)   {}

struct AcScroller { _h isize }
fn scroller(master AcScreen) AcScroller { return AcScroller{ _h: C.ac_widgets_group_new(master._h, ''.str) } }
fn (s AcScroller) pack() { C.ac_widgets_group_pack(s._h) }

struct AcTable { _h isize }
fn table(master AcScreen) AcTable { return AcTable{ _h: C.ac_widgets_listbox_new(master._h, 40, 10) } }
fn (t AcTable) pack()            { C.ac_widgets_listbox_pack(t._h) }
fn (t AcTable) add(row string)   { C.ac_widgets_listbox_add(t._h, row.str) }

struct AcListbox { _h isize }
fn listbox(master AcScreen, width int, height int) AcListbox {
	return AcListbox{ _h: C.ac_widgets_listbox_new(master._h, width, height) }
}
fn (l AcListbox) pack()            { C.ac_widgets_listbox_pack(l._h) }
fn (l AcListbox) add(item string)  { C.ac_widgets_listbox_add(l._h, item.str) }
fn (l AcListbox) get() []string {
	n := C.ac_widgets_listbox_count(l._h)
	mut out := []string{}
	for i in 0..n { out << unsafe { cstring_to_vstring(C.ac_widgets_listbox_item(l._h, i)) } }
	return out
}

struct AcSketch { _h isize }
fn sketch(master AcScreen, width int, height int) AcSketch {
	return AcSketch{ _h: C.ac_widgets_sketch_new(master._h, width, height) }
}
fn (s AcSketch) pack()                                               { C.ac_widgets_sketch_pack(s._h) }
fn (s AcSketch) clear()                                              { C.ac_widgets_sketch_clear(s._h) }
fn (s AcSketch) line(x1 f64, y1 f64, x2 f64, y2 f64, r u8, g u8, b u8) { C.ac_widgets_sketch_line(s._h, x1, y1, x2, y2, r, g, b) }
fn (s AcSketch) rect(x1 f64, y1 f64, x2 f64, y2 f64, r u8, g u8, b u8) { C.ac_widgets_sketch_rect(s._h, x1, y1, x2, y2, r, g, b) }
fn (s AcSketch) circle(cx f64, cy f64, rad f64, r u8, g u8, b u8)       { C.ac_widgets_sketch_circle(s._h, cx, cy, rad, r, g, b) }
fn (s AcSketch) text_at(x f64, y f64, t string, r u8, g u8, b u8)       { C.ac_widgets_sketch_text(s._h, x, y, t.str, r, g, b) }
