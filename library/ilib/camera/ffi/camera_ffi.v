// AC ilib: camera — V C interop (libaccamera.so / libaccamera.dll)
#flag -L library/camera -laccamera
#flag -Wl,-rpath,library/camera
#include "library/camera/camera_c.h"

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

fn camera_init() int { return C.ac_camera_init() }
fn camera_capture(f string) int { return C.ac_camera_capture(f.str) }
fn camera_capture_latest(f string) int { return C.ac_camera_capture_latest(f.str) }
fn camera_capture_first(f string) int  { return C.ac_camera_capture_first(f.str) }
fn camera_release() { C.ac_camera_release() }
fn sidebar_config(k string, v string) { C.ac_sidebar_config(k.str, v.str) }
fn sidebar_setregion(r string) { C.ac_sidebar_setregion(r.str) }
fn sidebar_setinteractive(v bool) { C.ac_sidebar_setinteractive(if v { 1 } else { 0 }) }
fn sidebar_display(m string) { C.ac_sidebar_display(m.str) }
fn sidebar_ask(p string) string { return unsafe { cstring_to_vstring(C.ac_sidebar_ask(p.str)) } }
fn sidebar_getinput() string { return unsafe { cstring_to_vstring(C.ac_sidebar_getinput()) } }
fn screen_setmode(m string) { C.ac_screen_setmode(m.str) }
fn screen_update() { C.ac_screen_update() }
