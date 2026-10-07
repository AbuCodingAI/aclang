// AC ilib: os — Go CGO FFI (libacoos.so)
package main

/*
#cgo CFLAGS: -I${SRCDIR}/library/ilib/os
#cgo LDFLAGS: -L${SRCDIR}/library/ilib/os -lacoos -Wl,-rpath,${SRCDIR}/library/ilib/os
#include <stdlib.h>
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

func os_isdir(path string) int64 { cs := C.CString(path); defer C.free(unsafe.Pointer(cs)); return int64(C.ac_os_isdir(cs)) }
func os_isfile(path string) int64 { cs := C.CString(path); defer C.free(unsafe.Pointer(cs)); return int64(C.ac_os_isfile(cs)) }
func os_size(path string) int64 { cs := C.CString(path); defer C.free(unsafe.Pointer(cs)); return int64(C.ac_os_size(cs)) }
func os_mtime(path string) int64 { cs := C.CString(path); defer C.free(unsafe.Pointer(cs)); return int64(C.ac_os_mtime(cs)) }
func os_copy(src, dst string) int64 {
	cs := C.CString(src); defer C.free(unsafe.Pointer(cs))
	cd := C.CString(dst); defer C.free(unsafe.Pointer(cd))
	return int64(C.ac_os_copy(cs, cd))
}
func os_move(src, dst string) int64 {
	cs := C.CString(src); defer C.free(unsafe.Pointer(cs))
	cd := C.CString(dst); defer C.free(unsafe.Pointer(cd))
	return int64(C.ac_os_move(cs, cd))
}
func os_listdir(path string) []string {
	cs := C.CString(path); defer C.free(unsafe.Pointer(cs))
	var n C.int
	arr := C.ac_os_listdir(cs, &n)
	if arr == nil { return []string{} }
	defer C.ac_os_free_list(arr, n)
	items := (*[1 << 28]*C.char)(unsafe.Pointer(arr))[:int(n):int(n)]
	out := make([]string, int(n))
	for i, p := range items { out[i] = C.GoString(p) }
	return out
}
func os_tmpdir() string { return C.GoString(C.ac_os_tmpdir()) }
func os_tmpfile(suffix string) string { cs := C.CString(suffix); defer C.free(unsafe.Pointer(cs)); return C.GoString(C.ac_os_tmpfile(cs)) }
func os_mktmpdir() string { return C.GoString(C.ac_os_mktmpdir()) }
func os_chdir(path string) int64 { cs := C.CString(path); defer C.free(unsafe.Pointer(cs)); return int64(C.ac_os_chdir(cs)) }
func os_join(a, b string) string {
	ca := C.CString(a); defer C.free(unsafe.Pointer(ca))
	cb := C.CString(b); defer C.free(unsafe.Pointer(cb))
	return C.GoString(C.ac_os_join(ca, cb))
}
func os_basename(path string) string { cs := C.CString(path); defer C.free(unsafe.Pointer(cs)); return C.GoString(C.ac_os_basename(cs)) }
func os_dirname(path string) string { cs := C.CString(path); defer C.free(unsafe.Pointer(cs)); return C.GoString(C.ac_os_dirname(cs)) }
func os_homedir() string { return C.GoString(C.ac_os_homedir()) }
