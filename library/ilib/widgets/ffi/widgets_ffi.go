// AC ilib: widgets — Go CGO FFI (libacwidgets.so / acwidgets.dll)
// Inlined by AC->GO compiler when "use ilib widgets" is declared
package main

/*
#cgo CFLAGS: -I${SRCDIR}/../
#cgo LDFLAGS: -L${SRCDIR}/../ -lacwidgets -Wl,-rpath,${SRCDIR}/../
#include "widgets_c.h"
#include <stdlib.h>
*/
import "C"
import "unsafe"
import "strings"
import "fmt"

func _ws(s string) (*C.char, func()) { cs := C.CString(s); return cs, func() { C.free(unsafe.Pointer(cs)) } }

func widgets_screen_new(title, geometry string) C.ac_widget_t {
	ct, ft := _ws(title); defer ft()
	cg, fg := _ws(geometry); defer fg()
	return C.ac_widgets_screen_new(ct, cg)
}
func widgets_screen_mainloop(h C.ac_widget_t) { C.ac_widgets_screen_mainloop(h) }
func widgets_screen_update(h C.ac_widget_t)   { C.ac_widgets_screen_update(h) }
func widgets_screen_destroy(h C.ac_widget_t)  { C.ac_widgets_screen_destroy(h) }

func widgets_display_new(m C.ac_widget_t, text string) C.ac_widget_t {
	ct, ft := _ws(text); defer ft(); return C.ac_widgets_display_new(m, ct)
}
func widgets_display_pack(h C.ac_widget_t)          { C.ac_widgets_display_pack(h) }
func widgets_display_set(h C.ac_widget_t, v string) { ct, ft := _ws(v); defer ft(); C.ac_widgets_display_set(h, ct) }
func widgets_display_get(h C.ac_widget_t) string    { return C.GoString(C.ac_widgets_display_get(h)) }

func widgets_ask_new(m C.ac_widget_t, width int) C.ac_widget_t { return C.ac_widgets_ask_new(m, C.int(width)) }
func widgets_ask_pack(h C.ac_widget_t)          { C.ac_widgets_ask_pack(h) }
func widgets_ask_get(h C.ac_widget_t) string    { return C.GoString(C.ac_widgets_ask_get(h)) }
func widgets_ask_set(h C.ac_widget_t, v string) { ct, ft := _ws(v); defer ft(); C.ac_widgets_ask_set(h, ct) }

func widgets_btn_new(m C.ac_widget_t, text string) C.ac_widget_t {
	ct, ft := _ws(text); defer ft(); return C.ac_widgets_btn_new(m, ct)
}
func widgets_btn_pack(h C.ac_widget_t) { C.ac_widgets_btn_pack(h) }

func widgets_ckbtn_new(m C.ac_widget_t, text string) C.ac_widget_t {
	ct, ft := _ws(text); defer ft(); return C.ac_widgets_ckbtn_new(m, ct)
}
func widgets_ckbtn_pack(h C.ac_widget_t)        { C.ac_widgets_ckbtn_pack(h) }
func widgets_ckbtn_get(h C.ac_widget_t) bool    { return C.ac_widgets_ckbtn_get(h) != 0 }
func widgets_ckbtn_set(h C.ac_widget_t, v bool) { b := 0; if v { b = 1 }; C.ac_widgets_ckbtn_set(h, C.int(b)) }

func widgets_dropdown_new(m C.ac_widget_t) C.ac_widget_t { return C.ac_widgets_dropdown_new(m) }
func widgets_dropdown_pack(h C.ac_widget_t)              { C.ac_widgets_dropdown_pack(h) }
func widgets_dropdown_add(h C.ac_widget_t, item string)  { ct, ft := _ws(item); defer ft(); C.ac_widgets_dropdown_add(h, ct) }
func widgets_dropdown_get(h C.ac_widget_t) string        { return C.GoString(C.ac_widgets_dropdown_get(h)) }
func widgets_dropdown_set(h C.ac_widget_t, item string)  { ct, ft := _ws(item); defer ft(); C.ac_widgets_dropdown_set(h, ct) }

func widgets_advance_new(m C.ac_widget_t, length int) C.ac_widget_t { return C.ac_widgets_advance_new(m, C.int(length)) }
func widgets_advance_pack(h C.ac_widget_t)            { C.ac_widgets_advance_pack(h) }
func widgets_advance_set(h C.ac_widget_t, v float64)  { C.ac_widgets_advance_set(h, C.double(v)) }
func widgets_advance_get(h C.ac_widget_t) float64     { return float64(C.ac_widgets_advance_get(h)) }

func widgets_slider_new(m C.ac_widget_t, from_val, to_val float64, orient string) C.ac_widget_t {
	co, fo := _ws(orient); defer fo(); return C.ac_widgets_slider_new(m, C.double(from_val), C.double(to_val), co)
}
func widgets_slider_pack(h C.ac_widget_t)           { C.ac_widgets_slider_pack(h) }
func widgets_slider_get(h C.ac_widget_t) float64    { return float64(C.ac_widgets_slider_get(h)) }
func widgets_slider_set(h C.ac_widget_t, v float64) { C.ac_widgets_slider_set(h, C.double(v)) }

func widgets_group_new(m C.ac_widget_t, text string) C.ac_widget_t {
	ct, ft := _ws(text); defer ft(); return C.ac_widgets_group_new(m, ct)
}
func widgets_group_pack(h C.ac_widget_t) { C.ac_widgets_group_pack(h) }

func widgets_listbox_new(m C.ac_widget_t, width, height int) C.ac_widget_t {
	return C.ac_widgets_listbox_new(m, C.int(width), C.int(height))
}
func widgets_listbox_pack(h C.ac_widget_t)             { C.ac_widgets_listbox_pack(h) }
func widgets_listbox_add(h C.ac_widget_t, item string) { ct, ft := _ws(item); defer ft(); C.ac_widgets_listbox_add(h, ct) }
func widgets_listbox_item(h C.ac_widget_t, idx int) string {
	return C.GoString(C.ac_widgets_listbox_item(h, C.int(idx)))
}
func widgets_listbox_count(h C.ac_widget_t) int { return int(C.ac_widgets_listbox_count(h)) }

func widgets_sketch_new(m C.ac_widget_t, width, height int) C.ac_widget_t {
	return C.ac_widgets_sketch_new(m, C.int(width), C.int(height))
}
func widgets_sketch_pack(h C.ac_widget_t)  { C.ac_widgets_sketch_pack(h) }
func widgets_sketch_clear(h C.ac_widget_t) { C.ac_widgets_sketch_clear(h) }
func widgets_sketch_line(h C.ac_widget_t, x1, y1, x2, y2 float64, r, g, b uint8) {
	C.ac_widgets_sketch_line(h, C.double(x1), C.double(y1), C.double(x2), C.double(y2), C.uint8_t(r), C.uint8_t(g), C.uint8_t(b))
}
func widgets_sketch_rect(h C.ac_widget_t, x1, y1, x2, y2 float64, r, g, b uint8) {
	C.ac_widgets_sketch_rect(h, C.double(x1), C.double(y1), C.double(x2), C.double(y2), C.uint8_t(r), C.uint8_t(g), C.uint8_t(b))
}
func widgets_sketch_circle(h C.ac_widget_t, cx, cy, rad float64, r, g, b uint8) {
	C.ac_widgets_sketch_circle(h, C.double(cx), C.double(cy), C.double(rad), C.uint8_t(r), C.uint8_t(g), C.uint8_t(b))
}
func widgets_sketch_text(h C.ac_widget_t, x, y float64, t string, r, g, b uint8) {
	ct, ft := _ws(t); defer ft()
	C.ac_widgets_sketch_text(h, C.double(x), C.double(y), ct, C.uint8_t(r), C.uint8_t(g), C.uint8_t(b))
}

// ── Go wrapper types ──────────────────────────────────────────────────────────

func _splitComma(s string) []string {
	var out []string
	for _, v := range strings.Split(s, ",") {
		v = strings.TrimSpace(v)
		if v != "" { out = append(out, v) }
	}
	return out
}

func init() { C.ac_widgets_init() }

type AcScreen struct{ _h C.ac_widget_t }
func Screen(title, geometry string) *AcScreen { return &AcScreen{_h: widgets_screen_new(title, geometry)} }
func (s *AcScreen) mainloop()                  { widgets_screen_mainloop(s._h) }
func (s *AcScreen) update()                    { widgets_screen_update(s._h) }
func (s *AcScreen) destroy()                   { widgets_screen_destroy(s._h) }

type AcDisplay struct{ _h C.ac_widget_t }
func display(m *AcScreen, text string) *AcDisplay { return &AcDisplay{_h: widgets_display_new(m._h, text)} }
func (d *AcDisplay) pack()           { widgets_display_pack(d._h) }
func (d *AcDisplay) set(v string)    { widgets_display_set(d._h, v) }
func (d *AcDisplay) get() string     { return widgets_display_get(d._h) }
func (d *AcDisplay) config(v string) { d.set(v) }

type AcAsk struct{ _h C.ac_widget_t }
func ask(m *AcScreen, width int) *AcAsk { return &AcAsk{_h: widgets_ask_new(m._h, width)} }
func (a *AcAsk) pack()        { widgets_ask_pack(a._h) }
func (a *AcAsk) get() string  { return widgets_ask_get(a._h) }
func (a *AcAsk) set(v string) { widgets_ask_set(a._h, v) }

type AcBtn struct{ _h C.ac_widget_t }
func btn(m *AcScreen, text string) *AcBtn { return &AcBtn{_h: widgets_btn_new(m._h, text)} }
func (b *AcBtn) pack() { widgets_btn_pack(b._h) }

type AcCkbtn struct{ _h C.ac_widget_t }
func ckbtn(m *AcScreen, text string) *AcCkbtn { return &AcCkbtn{_h: widgets_ckbtn_new(m._h, text)} }
func (c *AcCkbtn) pack()      { widgets_ckbtn_pack(c._h) }
func (c *AcCkbtn) get() bool  { return widgets_ckbtn_get(c._h) }
func (c *AcCkbtn) set(v bool) { widgets_ckbtn_set(c._h, v) }

type AcRadbtn struct{ _h C.ac_widget_t }
func radbtn(m *AcScreen, text string) *AcRadbtn { return &AcRadbtn{_h: widgets_ckbtn_new(m._h, text)} }
func (r *AcRadbtn) pack()     { widgets_ckbtn_pack(r._h) }
func (r *AcRadbtn) get() bool { return widgets_ckbtn_get(r._h) }

type AcDropdown struct{ _h C.ac_widget_t }
func dropdown(m *AcScreen, values string) *AcDropdown {
	d := &AcDropdown{_h: widgets_dropdown_new(m._h)}
	for _, v := range _splitComma(values) { widgets_dropdown_add(d._h, v) }
	return d
}
func (d *AcDropdown) pack()        { widgets_dropdown_pack(d._h) }
func (d *AcDropdown) get() string  { return widgets_dropdown_get(d._h) }
func (d *AcDropdown) set(v string) { widgets_dropdown_set(d._h, v) }

type AcAdvance struct{ _h C.ac_widget_t }
func advance(m *AcScreen, length int) *AcAdvance { return &AcAdvance{_h: widgets_advance_new(m._h, length)} }
func (a *AcAdvance) pack()         { widgets_advance_pack(a._h) }
func (a *AcAdvance) set(v float64) { widgets_advance_set(a._h, v) }
func (a *AcAdvance) get() float64  { return widgets_advance_get(a._h) }

type AcSlider struct{ _h C.ac_widget_t }
func slider(m *AcScreen, from_val, to_val float64, orient string) *AcSlider {
	return &AcSlider{_h: widgets_slider_new(m._h, from_val, to_val, orient)}
}
func (s *AcSlider) pack()         { widgets_slider_pack(s._h) }
func (s *AcSlider) get() float64  { return widgets_slider_get(s._h) }
func (s *AcSlider) set(v float64) { widgets_slider_set(s._h, v) }

type AcGroup struct{ _h C.ac_widget_t }
func group(m *AcScreen, text string) *AcGroup { return &AcGroup{_h: widgets_group_new(m._h, text)} }
func (g *AcGroup) pack() { widgets_group_pack(g._h) }

type AcTabs struct{ _h C.ac_widget_t }
func tabs(m *AcScreen) *AcTabs              { return &AcTabs{_h: widgets_group_new(m._h, "")} }
func (t *AcTabs) pack()                      { widgets_group_pack(t._h) }
func (t *AcTabs) add_tab(_ string) *AcTabs  { return t }

type AcScroller struct{ _h C.ac_widget_t }
func scroller(m *AcScreen) *AcScroller { return &AcScroller{_h: widgets_group_new(m._h, "")} }
func (s *AcScroller) pack()             { widgets_group_pack(s._h) }

type AcTable struct{ _h C.ac_widget_t }
func table(m *AcScreen) *AcTable { return &AcTable{_h: widgets_listbox_new(m._h, 40, 10)} }
func (t *AcTable) pack()          { widgets_listbox_pack(t._h) }
func (t *AcTable) add(row interface{}) { widgets_listbox_add(t._h, fmt.Sprintf("%v", row)) }

type AcListbox struct{ _h C.ac_widget_t }
func listbox(m *AcScreen, width, height int) *AcListbox {
	return &AcListbox{_h: widgets_listbox_new(m._h, width, height)}
}
func (l *AcListbox) pack()           { widgets_listbox_pack(l._h) }
func (l *AcListbox) add(item string) { widgets_listbox_add(l._h, item) }
func (l *AcListbox) get() []string {
	n := widgets_listbox_count(l._h)
	out := make([]string, n)
	for i := range out { out[i] = widgets_listbox_item(l._h, i) }
	return out
}

type AcSketch struct{ _h C.ac_widget_t }
func sketch(m *AcScreen, width, height int) *AcSketch {
	return &AcSketch{_h: widgets_sketch_new(m._h, width, height)}
}
func (s *AcSketch) pack()                                          { widgets_sketch_pack(s._h) }
func (s *AcSketch) clear()                                         { widgets_sketch_clear(s._h) }
func (s *AcSketch) line(x1, y1, x2, y2 float64, r, g, b uint8)  { widgets_sketch_line(s._h, x1, y1, x2, y2, r, g, b) }
func (s *AcSketch) rect(x1, y1, x2, y2 float64, r, g, b uint8)  { widgets_sketch_rect(s._h, x1, y1, x2, y2, r, g, b) }
func (s *AcSketch) circle(cx, cy, rad float64, r, g, b uint8)    { widgets_sketch_circle(s._h, cx, cy, rad, r, g, b) }
func (s *AcSketch) text_at(x, y float64, t string, r, g, b uint8){ widgets_sketch_text(s._h, x, y, t, r, g, b) }
