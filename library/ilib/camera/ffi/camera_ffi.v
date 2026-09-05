// AC ilib: camera — V C interop (libaccamera.so / libaccamera.dll)
module main

#flag -L @AC_LIBDIR@ -laccamera
#flag -Wl,-rpath,@AC_LIBDIR@
#include "@AC_LIBDIR@/camera_c.h"

fn C.ac_camera_init() int
fn C.ac_camera_capture(filename &char) int
fn C.ac_camera_capture_latest(filename &char) int
fn C.ac_camera_capture_first(filename &char) int
fn C.ac_camera_release()
fn C.ac_sidebar_config(key &char, value &char)
fn C.ac_sidebar_setregion(region &char)
fn C.ac_sidebar_setinteractive(value int)
fn C.ac_sidebar_display(message &char)
fn C.ac_sidebar_ask(prompt &char) &char
fn C.ac_sidebar_getinput() &char
fn C.ac_screen_setmode(mode &char)
fn C.ac_screen_update()

// i64 (not int) — AC's V codegen infers i64 for call results by default (see
// math_ffi.v/regex_ffi.v's own numeric results); a plain `int` return was a type
// mismatch at every call site assigning into an AC-declared i64 var.
fn camera_init() i64 { return i64(C.ac_camera_init()) }
fn camera_capture(f string) i64 { return i64(C.ac_camera_capture(f.str)) }
fn camera_capture_latest(f string) i64 { return i64(C.ac_camera_capture_latest(f.str)) }
fn camera_capture_first(f string) i64  { return i64(C.ac_camera_capture_first(f.str)) }
fn camera_release() { C.ac_camera_release() }
fn sidebar_config(k string, v string) { C.ac_sidebar_config(k.str, v.str) }
fn sidebar_setregion(r string) { C.ac_sidebar_setregion(r.str) }
// AC has no bool literals — `sidebar.setinteractive(0)`/`(1)` passes a plain integer,
// not `true`/`false`, so the param must accept i64 like the rest of this file's ints.
fn sidebar_setinteractive(v i64) { C.ac_sidebar_setinteractive(if v != 0 { 1 } else { 0 }) }
fn sidebar_display(m string) { C.ac_sidebar_display(m.str) }
fn sidebar_ask(p string) string { return unsafe { cstring_to_vstring(C.ac_sidebar_ask(p.str)) } }
fn sidebar_getinput() string { return unsafe { cstring_to_vstring(C.ac_sidebar_getinput()) } }
fn screen_setmode(m string) { C.ac_screen_setmode(m.str) }
fn screen_update() { C.ac_screen_update() }

// Namespace structs — AC-generated V uses camera.init(), sidebar.display(), screen.update()
struct AcCameraNS {}
fn (o AcCameraNS) init() i64                     { return camera_init() }
fn (o AcCameraNS) capture(f string) i64          { return camera_capture(f) }
fn (o AcCameraNS) capture_latest(f string) i64   { return camera_capture_latest(f) }
fn (o AcCameraNS) capture_first(f string) i64    { return camera_capture_first(f) }
fn (o AcCameraNS) release()                      { camera_release() }

struct AcSidebarNS {}
fn (o AcSidebarNS) config(k string, v string)    { sidebar_config(k, v) }
fn (o AcSidebarNS) setregion(r string)           { sidebar_setregion(r) }
fn (o AcSidebarNS) setinteractive(v i64)         { sidebar_setinteractive(v) }
fn (o AcSidebarNS) display(m string)             { sidebar_display(m) }
fn (o AcSidebarNS) ask(p string) string          { return sidebar_ask(p) }
fn (o AcSidebarNS) getinput() string             { return sidebar_getinput() }

struct AcScreenNS {}
fn (o AcScreenNS) setmode(m string) { screen_setmode(m) }
fn (o AcScreenNS) update()          { screen_update() }

const camera = AcCameraNS{}
const sidebar = AcSidebarNS{}
const screen = AcScreenNS{}
