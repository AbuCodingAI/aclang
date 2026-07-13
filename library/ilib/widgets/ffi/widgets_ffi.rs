// AC ilib: widgets — Rust FFI (libacwidgets.so / acwidgets.dll)
// Inlined by AC->RS compiler when "use ilib widgets" is declared.
// All values are i64 handles matching AC's untyped IR.
use std::ffi::{CString, CStr, c_char};
use std::os::raw::{c_int, c_double};

type AcW = isize;

#[link(name = "acwidgets")]
extern "C" {
    fn ac_widgets_init();
    fn ac_widgets_pack(h: AcW);
    fn ac_widgets_set_lazy(h: AcW);
    fn ac_widgets_pack_spaced(h: AcW, sx: c_int, sy: c_int);
    fn ac_widgets_add(h: AcW, item: *const c_char);
    fn ac_widgets_set_d(h: AcW, v: c_double);

    fn ac_widgets_screen_new(title: *const c_char, geometry: *const c_char) -> AcW;
    fn ac_widgets_screen_mainloop(h: AcW);
    fn ac_widgets_screen_update(h: AcW);
    fn ac_widgets_screen_destroy(h: AcW);

    fn ac_widgets_display_new(master: AcW, text: *const c_char) -> AcW;
    fn ac_widgets_display_set(h: AcW, text: *const c_char);
    fn ac_widgets_display_get(h: AcW) -> *const c_char;

    fn ac_widgets_ask_new(master: AcW, width: c_int) -> AcW;
    fn ac_widgets_ask_get(h: AcW) -> *const c_char;
    fn ac_widgets_ask_set(h: AcW, text: *const c_char);

    fn ac_widgets_btn_new(master: AcW, text: *const c_char) -> AcW;
    fn ac_widgets_btn_on_click(h: AcW, cb: extern "C" fn(*mut std::ffi::c_void), userdata: *mut std::ffi::c_void);

    fn ac_widgets_ckbtn_new(master: AcW, text: *const c_char) -> AcW;
    fn ac_widgets_ckbtn_get(h: AcW) -> c_int;
    fn ac_widgets_ckbtn_set(h: AcW, value: c_int);

    fn ac_widgets_dropdown_new(master: AcW) -> AcW;
    fn ac_widgets_dropdown_get(h: AcW) -> *const c_char;
    fn ac_widgets_dropdown_set(h: AcW, item: *const c_char);

    fn ac_widgets_advance_new(master: AcW, length: c_int) -> AcW;
    fn ac_widgets_advance_get(h: AcW) -> c_double;

    fn ac_widgets_slider_new(master: AcW, from_val: c_double, to_val: c_double, orient: *const c_char) -> AcW;
    fn ac_widgets_slider_get(h: AcW) -> c_double;

    fn ac_widgets_group_new(master: AcW, text: *const c_char) -> AcW;
    fn ac_widgets_listbox_new(master: AcW, width: c_int, height: c_int) -> AcW;
    fn ac_widgets_listbox_item(h: AcW, index: c_int) -> *const c_char;
    fn ac_widgets_listbox_count(h: AcW) -> c_int;
    fn ac_widgets_sketch_new(master: AcW, width: c_int, height: c_int) -> AcW;
    fn ac_widgets_sketch_clear(h: AcW);
    fn ac_widgets_sketch_line(h: AcW, x1: c_double, y1: c_double, x2: c_double, y2: c_double, r: u8, g: u8, b: u8);
    fn ac_widgets_sketch_rect(h: AcW, x1: c_double, y1: c_double, x2: c_double, y2: c_double, r: u8, g: u8, b: u8);
    fn ac_widgets_sketch_circle(h: AcW, cx: c_double, cy: c_double, radius: c_double, r: u8, g: u8, b: u8);
    fn ac_widgets_sketch_text(h: AcW, x: c_double, y: c_double, text: *const c_char, r: u8, g: u8, b: u8);
}

fn _cs(s: &str) -> CString { CString::new(s).unwrap_or_default() }
unsafe fn _gs(p: *const c_char) -> String {
    if p.is_null() { String::new() } else { CStr::from_ptr(p).to_string_lossy().into_owned() }
}

static _WIDGETS_INIT: std::sync::Once = std::sync::Once::new();
fn _init() { _WIDGETS_INIT.call_once(|| { unsafe { ac_widgets_init() } }); }

// ── Extension trait: all widget methods callable on i64 handles ───────────────
pub trait AcWidgetExt {
    fn pack(self);
    fn pack_s(self, sx: i64, sy: i64);
    fn mainloop(self);
    fn update(self);
    fn destroy(self);
    fn add(self, item: &str);
    fn set(self, v: i64);
    fn set_val(self, v: f64);
    fn get(self) -> String;
    fn get_val(self) -> f64;
    fn clear(self);
}

impl AcWidgetExt for i64 {
    fn pack(self)               { unsafe { ac_widgets_pack(self as AcW) } }
    fn pack_s(self, sx: i64, sy: i64) { unsafe { ac_widgets_pack_spaced(self as AcW, sx as c_int, sy as c_int) } }
    fn mainloop(self)           { unsafe { ac_widgets_screen_mainloop(self as AcW) } }
    fn update(self)             { unsafe { ac_widgets_screen_update(self as AcW) } }
    fn destroy(self)            { unsafe { ac_widgets_screen_destroy(self as AcW) } }
    fn add(self, item: &str)    { let c = _cs(item); unsafe { ac_widgets_add(self as AcW, c.as_ptr()) } }
    fn set(self, v: i64)        { unsafe { ac_widgets_set_d(self as AcW, v as c_double) } }
    fn set_val(self, v: f64)    { unsafe { ac_widgets_set_d(self as AcW, v as c_double) } }
    fn get(self) -> String      { unsafe { _gs(ac_widgets_display_get(self as AcW)) } }
    fn get_val(self) -> f64     { unsafe { ac_widgets_advance_get(self as AcW) } }
    fn clear(self)              { unsafe { ac_widgets_sketch_clear(self as AcW) } }
}

// ── Callback wrapper: AC fn(i64)->i64 → extern "C" fn(*mut c_void) ────────────
extern "C" fn _ac_btn_cb(userdata: *mut std::ffi::c_void) {
    if userdata.is_null() { return; }
    let cb = unsafe { &*(userdata as *const fn(i64) -> i64) };
    cb(0);
}

// ── Free constructor functions (matching AC codegen output) ───────────────────

pub const lazy: i64 = -0xAC_i64;  // sentinel — pass as last arg to defer auto-pack

fn _auto_pack(h: i64, lz: i64) {
    if lz == lazy { unsafe { ac_widgets_set_lazy(h as AcW) } }
    else          { unsafe { ac_widgets_pack(h as AcW) } }
}

pub fn Screen(title: &str, geometry: &str) -> i64 {
    _init();
    let ct = _cs(title); let cg = _cs(geometry);
    unsafe { ac_widgets_screen_new(ct.as_ptr(), cg.as_ptr()) as i64 }
}

pub fn display(master: i64, text: &str) -> i64 {
    let ct = _cs(text);
    let h = unsafe { ac_widgets_display_new(master as AcW, ct.as_ptr()) as i64 };
    unsafe { ac_widgets_pack(h as AcW) }; h
}

pub fn display_lazy(master: i64, text: &str) -> i64 {
    let ct = _cs(text);
    let h = unsafe { ac_widgets_display_new(master as AcW, ct.as_ptr()) as i64 };
    unsafe { ac_widgets_set_lazy(h as AcW) }; h
}

pub fn ask(master: i64, width: i64) -> i64 {
    let h = unsafe { ac_widgets_ask_new(master as AcW, width as c_int) as i64 };
    unsafe { ac_widgets_pack(h as AcW) }; h
}

pub fn btn(master: i64, text: &str, cmd: fn(i64) -> i64) -> i64 {
    let ct = _cs(text);
    let h = unsafe { ac_widgets_btn_new(master as AcW, ct.as_ptr()) as i64 };
    let cb_ptr = Box::leak(Box::new(cmd)) as *mut fn(i64) -> i64;
    unsafe { ac_widgets_btn_on_click(h as AcW, _ac_btn_cb, cb_ptr as *mut std::ffi::c_void) }
    unsafe { ac_widgets_pack(h as AcW) }; h
}

pub fn btn_nc(master: i64, text: &str) -> i64 {
    let ct = _cs(text);
    let h = unsafe { ac_widgets_btn_new(master as AcW, ct.as_ptr()) as i64 };
    unsafe { ac_widgets_pack(h as AcW) }; h
}

pub fn ckbtn(master: i64, text: &str) -> i64 {
    let ct = _cs(text);
    let h = unsafe { ac_widgets_ckbtn_new(master as AcW, ct.as_ptr()) as i64 };
    unsafe { ac_widgets_pack(h as AcW) }; h
}

pub fn radbtn(master: i64, text: &str) -> i64 { ckbtn(master, text) }

pub fn dropdown(master: i64) -> i64 {
    let h = unsafe { ac_widgets_dropdown_new(master as AcW) as i64 };
    unsafe { ac_widgets_pack(h as AcW) }; h
}

pub fn advance(master: i64, length: i64) -> i64 {
    let h = unsafe { ac_widgets_advance_new(master as AcW, length as c_int) as i64 };
    unsafe { ac_widgets_pack(h as AcW) }; h
}

pub fn slider(master: i64, from_val: i64, to_val: i64, orient: &str) -> i64 {
    let co = _cs(orient);
    let h = unsafe { ac_widgets_slider_new(master as AcW, from_val as c_double, to_val as c_double, co.as_ptr()) as i64 };
    unsafe { ac_widgets_pack(h as AcW) }; h
}

pub fn group(master: i64, text: &str) -> i64 {
    let ct = _cs(text);
    let h = unsafe { ac_widgets_group_new(master as AcW, ct.as_ptr()) as i64 };
    unsafe { ac_widgets_pack(h as AcW) }; h
}

pub fn tabs(master: i64) -> i64   { group(master, "") }
pub fn scroller(master: i64) -> i64 { group(master, "") }

pub fn table(master: i64) -> i64 {
    let h = unsafe { ac_widgets_listbox_new(master as AcW, 40, 10) as i64 };
    unsafe { ac_widgets_pack(h as AcW) }; h
}

pub fn listbox(master: i64, width: i64, height: i64) -> i64 {
    let h = unsafe { ac_widgets_listbox_new(master as AcW, width as c_int, height as c_int) as i64 };
    unsafe { ac_widgets_pack(h as AcW) }; h
}

pub fn sketch(master: i64, width: i64, height: i64) -> i64 {
    let h = unsafe { ac_widgets_sketch_new(master as AcW, width as c_int, height as c_int) as i64 };
    unsafe { ac_widgets_pack(h as AcW) }; h
}
