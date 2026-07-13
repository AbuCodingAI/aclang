// AC ilib: os — Go CGO FFI (libacoos.so)
package main

/*
#cgo CFLAGS: -I${SRCDIR}/library/ilib/os
#cgo LDFLAGS: -L${SRCDIR}/library/ilib/os -lacoos -Wl,-rpath,${SRCDIR}/library/ilib/os
#include "os_c.h"
*/
import "C"
import "unsafe"

func os_bash(cmd string) int64 {
	cs := C.CString(cmd); defer C.free(unsafe.Pointer(cs))
	return int64(C.ac_os_bash(cs))
}
func os_sbash(cmd string) int64 {
	cs := C.CString(cmd); defer C.free(unsafe.Pointer(cs))
	return int64(C.ac_os_sbash(cs))
}
func os_app_open(app string) int64 {
	cs := C.CString(app); defer C.free(unsafe.Pointer(cs))
	return int64(C.ac_os_app_open(cs))
}
func os_mkfile(path string) int64 {
	cs := C.CString(path); defer C.free(unsafe.Pointer(cs))
	return int64(C.ac_os_mkfile(cs))
}
func os_rmfile(path string) int64 {
	cs := C.CString(path); defer C.free(unsafe.Pointer(cs))
	return int64(C.ac_os_rmfile(cs))
}
func os_mkdir(path string) int64 {
	cs := C.CString(path); defer C.free(unsafe.Pointer(cs))
	return int64(C.ac_os_mkdir(cs))
}
func os_rmdir(path string) int64 {
	cs := C.CString(path); defer C.free(unsafe.Pointer(cs))
	return int64(C.ac_os_rmdir(cs))
}
func os_exists(path string) bool {
	cs := C.CString(path); defer C.free(unsafe.Pointer(cs))
	return C.ac_os_exists(cs) != 0
}
func os_cwd() string              { return C.GoString(C.ac_os_cwd()) }
func os_env(key string) string    { cs := C.CString(key); defer C.free(unsafe.Pointer(cs)); return C.GoString(C.ac_os_env(cs)) }
func os_write_to(path, content string) int64 {
	cp := C.CString(path); defer C.free(unsafe.Pointer(cp))
	cc := C.CString(content); defer C.free(unsafe.Pointer(cc))
	return int64(C.ac_os_write_to(cp, cc))
}
func os_append_to(path, content string) int64 {
	cp := C.CString(path); defer C.free(unsafe.Pointer(cp))
	cc := C.CString(content); defer C.free(unsafe.Pointer(cc))
	return int64(C.ac_os_append_to(cp, cc))
}
func os_read(path string) string  { cs := C.CString(path); defer C.free(unsafe.Pointer(cs)); return C.GoString(C.ac_os_read(cs)) }
