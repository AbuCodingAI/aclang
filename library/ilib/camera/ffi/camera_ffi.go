// AC ilib: camera — Go CGO FFI (libaccamera.so / libaccamera.dll)
package main

/*
#cgo CFLAGS: -I${SRCDIR}/library/ilib/camera
#cgo LDFLAGS: -L${SRCDIR}/library/ilib/camera -laccamera -Wl,-rpath,${SRCDIR}/library/ilib/camera
#include "camera_c.h"
*/
import "C"
import "unsafe"

func camera_init() int           { return int(C.ac_camera_init()) }
func camera_capture(f string) int { cs := C.CString(f); defer C.free(unsafe.Pointer(cs)); return int(C.ac_camera_capture(cs)) }
func camera_capture_latest(f string) int { cs := C.CString(f); defer C.free(unsafe.Pointer(cs)); return int(C.ac_camera_capture_latest(cs)) }
func camera_capture_first(f string) int  { cs := C.CString(f); defer C.free(unsafe.Pointer(cs)); return int(C.ac_camera_capture_first(cs)) }
func camera_release()                    { C.ac_camera_release() }
func sidebar_config(k, v string) {
    ck := C.CString(k); defer C.free(unsafe.Pointer(ck))
    cv := C.CString(v); defer C.free(unsafe.Pointer(cv))
    C.ac_sidebar_config(ck, cv)
}
func sidebar_setregion(r string)       { cs := C.CString(r); defer C.free(unsafe.Pointer(cs)); C.ac_sidebar_setregion(cs) }
func sidebar_setinteractive(v bool)    { vi := 0; if v { vi = 1 }; C.ac_sidebar_setinteractive(C.int(vi)) }
func sidebar_display(m string)         { cs := C.CString(m); defer C.free(unsafe.Pointer(cs)); C.ac_sidebar_display(cs) }
func sidebar_ask(p string) string      { cs := C.CString(p); defer C.free(unsafe.Pointer(cs)); return C.GoString(C.ac_sidebar_ask(cs)) }
func sidebar_getinput() string         { return C.GoString(C.ac_sidebar_getinput()) }
func screen_setmode(m string)          { cs := C.CString(m); defer C.free(unsafe.Pointer(cs)); C.ac_screen_setmode(cs) }
func screen_update()                   { C.ac_screen_update() }
