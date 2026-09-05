// AC ilib: camera — Rust FFI (libaccamera.so / libaccamera.dll)
use std::os::raw::{c_int, c_char};
use std::ffi::{CString, CStr};

#[link(name = "accamera")]
extern "C" {
    fn ac_camera_init() -> c_int;
    fn ac_camera_capture(filename: *const c_char) -> c_int;
    fn ac_camera_capture_latest(filename: *const c_char) -> c_int;
    fn ac_camera_capture_first(filename: *const c_char) -> c_int;
    fn ac_camera_release();
    fn ac_sidebar_config(key: *const c_char, value: *const c_char);
    fn ac_sidebar_setregion(region: *const c_char);
    fn ac_sidebar_setinteractive(value: c_int);
    fn ac_sidebar_display(message: *const c_char);
    fn ac_sidebar_ask(prompt: *const c_char) -> *const c_char;
    fn ac_sidebar_getinput() -> *const c_char;
    fn ac_screen_setmode(mode: *const c_char);
    fn ac_screen_update();
}

fn _cs(s: &str) -> CString { CString::new(s).unwrap_or_default() }
fn _gs(p: *const c_char) -> String {
    if p.is_null() { return String::new(); }
    unsafe { CStr::from_ptr(p).to_string_lossy().into_owned() }
}

// i64 (not i32) — AC's Rust codegen infers i64 for call results by default (see
// os_ffi.rs's identical convention); a plain i32 return here was a "mismatched types:
// expected i64, found i32" compile error at every call site.
pub fn camera_init() -> i64 { unsafe { ac_camera_init() as i64 } }
pub fn camera_capture(f: &str) -> i64 { let s=_cs(f); unsafe { ac_camera_capture(s.as_ptr()) as i64 } }
pub fn camera_capture_latest(f: &str) -> i64 { let s=_cs(f); unsafe { ac_camera_capture_latest(s.as_ptr()) as i64 } }
pub fn camera_capture_first(f: &str) -> i64 { let s=_cs(f); unsafe { ac_camera_capture_first(s.as_ptr()) as i64 } }
pub fn camera_release() { unsafe { ac_camera_release(); } }
pub fn sidebar_config(k: &str, v: &str) { let ks=_cs(k); let vs=_cs(v); unsafe { ac_sidebar_config(ks.as_ptr(),vs.as_ptr()); } }
pub fn sidebar_setregion(r: &str) { let s=_cs(r); unsafe { ac_sidebar_setregion(s.as_ptr()); } }
// AC has no bool literals — `sidebar.setinteractive(0)`/`(1)` passes a plain integer,
// not `true`/`false` — Rust never implicitly converts int-to-bool ("expected bool,
// found integer"), so the param must accept i64 like the rest of this file's ints.
pub fn sidebar_setinteractive(v: i64) { unsafe { ac_sidebar_setinteractive((v != 0) as c_int); } }
pub fn sidebar_display(m: &str) { let s=_cs(m); unsafe { ac_sidebar_display(s.as_ptr()); } }
pub fn sidebar_ask(p: &str) -> String { let s=_cs(p); _gs(unsafe { ac_sidebar_ask(s.as_ptr()) }) }
pub fn sidebar_getinput() -> String { _gs(unsafe { ac_sidebar_getinput() }) }
pub fn screen_setmode(m: &str) { let s=_cs(m); unsafe { ac_screen_setmode(s.as_ptr()); } }
pub fn screen_update() { unsafe { ac_screen_update(); } }

// Namespace structs — AC-generated Rust uses camera.init(), sidebar.display(), screen.update()
pub struct AcCamera;
impl AcCamera {
    pub fn init(&self) -> i64         { camera_init() }
    pub fn capture(&self, f: &str) -> i64 { camera_capture(f) }
    pub fn capture_latest(&self, f: &str) -> i64 { camera_capture_latest(f) }
    pub fn capture_first(&self, f: &str) -> i64 { camera_capture_first(f) }
    pub fn release(&self)             { camera_release() }
}
pub struct AcSidebar;
impl AcSidebar {
    pub fn config(&self, k: &str, v: &str) { sidebar_config(k, v) }
    pub fn setregion(&self, r: &str)       { sidebar_setregion(r) }
    pub fn setinteractive(&self, v: i64)   { sidebar_setinteractive(v) }
    pub fn display(&self, m: &str)         { sidebar_display(m) }
    pub fn ask(&self, p: &str) -> String   { sidebar_ask(p) }
    pub fn getinput(&self) -> String       { sidebar_getinput() }
}
pub struct AcScreen;
impl AcScreen {
    pub fn setmode(&self, m: &str) { screen_setmode(m) }
    pub fn update(&self)           { screen_update() }
}

pub static camera: AcCamera  = AcCamera;
pub static sidebar: AcSidebar = AcSidebar;
pub static screen: AcScreen   = AcScreen;
