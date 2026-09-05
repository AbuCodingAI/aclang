// AC ilib: camera — Go CGO FFI (libaccamera.so / libaccamera.dll)
package main

/*
#cgo CFLAGS: -I${SRCDIR}/library/ilib/camera
#cgo LDFLAGS: -L${SRCDIR}/library/ilib/camera -laccamera -Wl,-rpath,${SRCDIR}/library/ilib/camera
#include <stdlib.h>
#include "camera_c.h"
*/
import "C"
import "unsafe"

// int64 (not int) return types — AC's Go codegen declares call results as int64 by
// default (see os_ffi.go's identical convention); a plain `int` return here was a
// "cannot use camera_init() (value of type int) as int64 value" compile error at
// every call site.
func camera_init() int64           { return int64(C.ac_camera_init()) }
func camera_capture(f string) int64 { cs := C.CString(f); defer C.free(unsafe.Pointer(cs)); return int64(C.ac_camera_capture(cs)) }
func camera_capture_latest(f string) int64 { cs := C.CString(f); defer C.free(unsafe.Pointer(cs)); return int64(C.ac_camera_capture_latest(cs)) }
func camera_capture_first(f string) int64  { cs := C.CString(f); defer C.free(unsafe.Pointer(cs)); return int64(C.ac_camera_capture_first(cs)) }
func camera_release()                    { C.ac_camera_release() }
func sidebar_config(k, v string) {
    ck := C.CString(k); defer C.free(unsafe.Pointer(ck))
    cv := C.CString(v); defer C.free(unsafe.Pointer(cv))
    C.ac_sidebar_config(ck, cv)
}
func sidebar_setregion(r string)       { cs := C.CString(r); defer C.free(unsafe.Pointer(cs)); C.ac_sidebar_setregion(cs) }
// AC has no bool literals — `sidebar.setinteractive(0)`/`(1)` passes a plain integer,
// not `true`/`false`, so the param must accept int64 (matching camera_init et al above),
// not bool ("cannot use 0 (untyped int constant) as bool value").
func sidebar_setinteractive(v int64)   { vi := 0; if v != 0 { vi = 1 }; C.ac_sidebar_setinteractive(C.int(vi)) }
func sidebar_display(m string)         { cs := C.CString(m); defer C.free(unsafe.Pointer(cs)); C.ac_sidebar_display(cs) }
func sidebar_ask(p string) string      { cs := C.CString(p); defer C.free(unsafe.Pointer(cs)); return C.GoString(C.ac_sidebar_ask(cs)) }
func sidebar_getinput() string         { return C.GoString(C.ac_sidebar_getinput()) }
func screen_setmode(m string)          { cs := C.CString(m); defer C.free(unsafe.Pointer(cs)); C.ac_screen_setmode(cs) }
func screen_update()                   { C.ac_screen_update() }
