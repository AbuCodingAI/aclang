// AC ilib: native-cpu (ncpu) — Go CGO FFI (libacncpu.so)
package main

/*
#cgo CFLAGS: -I${SRCDIR}/library/ilib/native-cpu
#cgo LDFLAGS: -L${SRCDIR}/library/ilib/native-cpu -lacncpu -Wl,-rpath,${SRCDIR}/library/ilib/native-cpu
#include <stdlib.h>
#include "native-cpu_c.h"
*/
import "C"
import "unsafe"

// Pointer registry (carried over from `pointers`, called bare: ptr_new(...), not ncpu.ptr_new(...))
func ptr_new(value string, size int64, typeID int64) int64 {
	cs := C.CString(value)
	defer C.free(unsafe.Pointer(cs))
	return int64(C.ac_ncpu_ptr_new(unsafe.Pointer(cs), C.int64_t(size), C.int64_t(typeID)))
}
func ptr_deref_into(ptrID int64, out []byte) int64 {
	return int64(C.ac_ncpu_ptr_deref(C.int64_t(ptrID), unsafe.Pointer(&out[0]), C.int64_t(len(out))))
}
func ptr_null() int64 { return int64(C.ac_ncpu_ptr_null()) }
func ptr_is_null(ptrID int64) int64 { return int64(C.ac_ncpu_ptr_is_null(C.int64_t(ptrID))) }
func ptr_eq(a, b int64) int64 { return int64(C.ac_ncpu_ptr_eq(C.int64_t(a), C.int64_t(b))) }
func ptr_copy(ptrID int64) int64 { return int64(C.ac_ncpu_ptr_copy(C.int64_t(ptrID))) }
func ptr_update(ptrID int64, value string, size int64) int64 {
	cs := C.CString(value)
	defer C.free(unsafe.Pointer(cs))
	return int64(C.ac_ncpu_ptr_update(C.int64_t(ptrID), unsafe.Pointer(cs), C.int64_t(size)))
}
func ptr_free(ptrID int64) int64 { return int64(C.ac_ncpu_ptr_free(C.int64_t(ptrID))) }

// Allocation
func dha(size int64, typeID int64) int64 { return int64(C.ac_ncpu_dha(C.int64_t(size), C.int64_t(typeID))) }
func zdha(size int64, typeID int64) int64 { return int64(C.ac_ncpu_zdha(C.int64_t(size), C.int64_t(typeID))) }
func ncpu_realloc(ptrID int64, size int64) int64 { return int64(C.ac_ncpu_realloc(C.int64_t(ptrID), C.int64_t(size))) }
func bmdha(size int64, typeID int64) int64 { return int64(C.ac_ncpu_bmdha(C.int64_t(size), C.int64_t(typeID))) }
func bmzdha(size int64, typeID int64) int64 { return int64(C.ac_ncpu_bmzdha(C.int64_t(size), C.int64_t(typeID))) }
func bmrealloc(ptrID int64, size int64) int64 { return int64(C.ac_ncpu_bmrealloc(C.int64_t(ptrID), C.int64_t(size))) }

// Arena allocator
func arena_create(size int64) int64 { return int64(C.ac_ncpu_arena_create(C.int64_t(size))) }
func arena_alloc(arenaID int64, size int64) int64 { return int64(C.ac_ncpu_arena_alloc(C.int64_t(arenaID), C.int64_t(size))) }
func arena_dealloc(arenaID int64, ptrID int64) int64 { return int64(C.ac_ncpu_arena_dealloc(C.int64_t(arenaID), C.int64_t(ptrID))) }
func arena_destroy(arenaID int64) int64 { return int64(C.ac_ncpu_arena_destroy(C.int64_t(arenaID))) }
func arena_abort(arenaID int64) int64 { return int64(C.ac_ncpu_arena_abort(C.int64_t(arenaID))) }

// Control
func ncpu_abort() { C.ac_ncpu_abort() }

// Messaging (between quickthreads) — spelling "recieve" is intentional
func broadcast(msg string) {
	cs := C.CString(msg)
	defer C.free(unsafe.Pointer(cs))
	C.ac_ncpu_broadcast(cs)
}
func recieve(msg string) int64 {
	cs := C.CString(msg)
	defer C.free(unsafe.Pointer(cs))
	return int64(C.ac_ncpu_recieve(cs))
}

// AC-facing dotted namespace: `ncpu.dha(...)`, `ncpu.abort()`, etc.
type _NcpuNS struct{}

func (_NcpuNS) dha(size int64, typeID int64) int64             { return dha(size, typeID) }
func (_NcpuNS) zdha(size int64, typeID int64) int64            { return zdha(size, typeID) }
func (_NcpuNS) realloc(ptrID int64, size int64) int64          { return ncpu_realloc(ptrID, size) }
func (_NcpuNS) bmdha(size int64, typeID int64) int64           { return bmdha(size, typeID) }
func (_NcpuNS) bmzdha(size int64, typeID int64) int64          { return bmzdha(size, typeID) }
func (_NcpuNS) bmrealloc(ptrID int64, size int64) int64        { return bmrealloc(ptrID, size) }
func (_NcpuNS) arena_create(size int64) int64                  { return arena_create(size) }
func (_NcpuNS) arena_alloc(arenaID int64, size int64) int64    { return arena_alloc(arenaID, size) }
func (_NcpuNS) arena_dealloc(arenaID int64, ptrID int64) int64 { return arena_dealloc(arenaID, ptrID) }
func (_NcpuNS) arena_destroy(arenaID int64) int64              { return arena_destroy(arenaID) }
func (_NcpuNS) arena_abort(arenaID int64) int64                { return arena_abort(arenaID) }
func (_NcpuNS) abort()                                         { ncpu_abort() }
func (_NcpuNS) broadcast(msg string)                            { broadcast(msg) }
func (_NcpuNS) recieve(msg string) int64                       { return recieve(msg) }

var ncpu = _NcpuNS{}
