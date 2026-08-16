// AC ilib: native-cpu (ncpu) — V C interop (libacncpu.so)
// Inlined by AC->V compiler when "use ilib native-cpu" is declared.
#flag -L @AC_LIBDIR@ -lacncpu
#flag -Wl,-rpath,@AC_LIBDIR@
#include "@AC_LIBDIR@/native-cpu_c.h"

fn C.ac_ncpu_ptr_new(value voidptr, value_size i64, type_id i64) i64
fn C.ac_ncpu_ptr_deref(ptr_id i64, out voidptr, out_size i64) i64
fn C.ac_ncpu_ptr_is_null(ptr_id i64) int
fn C.ac_ncpu_ptr_null() i64
fn C.ac_ncpu_ptr_eq(ptr1 i64, ptr2 i64) int
fn C.ac_ncpu_ptr_copy(ptr_id i64) i64
fn C.ac_ncpu_ptr_update(ptr_id i64, new_value voidptr, size i64) int
fn C.ac_ncpu_ptr_free(ptr_id i64) int

fn C.ac_ncpu_dha(size i64, type_id i64) i64
fn C.ac_ncpu_zdha(size i64, type_id i64) i64
fn C.ac_ncpu_realloc(ptr_id i64, size i64) i64
fn C.ac_ncpu_bmdha(size i64, type_id i64) i64
fn C.ac_ncpu_bmzdha(size i64, type_id i64) i64
fn C.ac_ncpu_bmrealloc(ptr_id i64, size i64) i64

fn C.ac_ncpu_arena_create(size i64) i64
fn C.ac_ncpu_arena_alloc(arena_id i64, size i64) i64
fn C.ac_ncpu_arena_dealloc(arena_id i64, ptr_id i64) int
fn C.ac_ncpu_arena_destroy(arena_id i64) int
fn C.ac_ncpu_arena_abort(arena_id i64) int

fn C.ac_ncpu_abort()
fn C.ac_ncpu_broadcast(msg &char)
fn C.ac_ncpu_recieve(msg &char) int

// Pointer registry (carried over from `pointers`, called bare). ptr_new/ptr_update
// take V `string` (not voidptr) — AC source passes them a plain value expression
// (usually a string literal), and V doesn't auto-coerce string to voidptr; `.str`
// gives the underlying byte pointer, matching os_ffi.v's string-arg convention.
pub fn ptr_new(value string, value_size i64, type_id i64) i64 { return C.ac_ncpu_ptr_new(value.str, value_size, type_id) }
pub fn ptr_deref(ptr_id i64, out voidptr, out_size i64) i64    { return C.ac_ncpu_ptr_deref(ptr_id, out, out_size) }
pub fn ptr_null() i64                                          { return C.ac_ncpu_ptr_null() }
pub fn ptr_is_null(ptr_id i64) i64                              { return C.ac_ncpu_ptr_is_null(ptr_id) }
pub fn ptr_eq(ptr1 i64, ptr2 i64) i64                            { return C.ac_ncpu_ptr_eq(ptr1, ptr2) }
pub fn ptr_copy(ptr_id i64) i64                                 { return C.ac_ncpu_ptr_copy(ptr_id) }
pub fn ptr_update(ptr_id i64, new_value string, size i64) i64  { return C.ac_ncpu_ptr_update(ptr_id, new_value.str, size) }
pub fn ptr_free(ptr_id i64) i64                                 { return C.ac_ncpu_ptr_free(ptr_id) }

struct AcNcpuNS {}
fn (n AcNcpuNS) dha(size i64, type_id i64) i64                 { return C.ac_ncpu_dha(size, type_id) }
fn (n AcNcpuNS) zdha(size i64, type_id i64) i64                { return C.ac_ncpu_zdha(size, type_id) }
fn (n AcNcpuNS) realloc(ptr_id i64, size i64) i64              { return C.ac_ncpu_realloc(ptr_id, size) }
fn (n AcNcpuNS) bmdha(size i64, type_id i64) i64               { return C.ac_ncpu_bmdha(size, type_id) }
fn (n AcNcpuNS) bmzdha(size i64, type_id i64) i64              { return C.ac_ncpu_bmzdha(size, type_id) }
fn (n AcNcpuNS) bmrealloc(ptr_id i64, size i64) i64            { return C.ac_ncpu_bmrealloc(ptr_id, size) }
fn (n AcNcpuNS) arena_create(size i64) i64                     { return C.ac_ncpu_arena_create(size) }
fn (n AcNcpuNS) arena_alloc(arena_id i64, size i64) i64        { return C.ac_ncpu_arena_alloc(arena_id, size) }
fn (n AcNcpuNS) arena_dealloc(arena_id i64, ptr_id i64) int    { return C.ac_ncpu_arena_dealloc(arena_id, ptr_id) }
fn (n AcNcpuNS) arena_destroy(arena_id i64) int                { return C.ac_ncpu_arena_destroy(arena_id) }
fn (n AcNcpuNS) arena_abort(arena_id i64) int                  { return C.ac_ncpu_arena_abort(arena_id) }
fn (n AcNcpuNS) abort()                                        { C.ac_ncpu_abort() }
fn (n AcNcpuNS) broadcast(msg string)                          { C.ac_ncpu_broadcast(msg.str) }
fn (n AcNcpuNS) recieve(msg string) int                        { return C.ac_ncpu_recieve(msg.str) }

const ncpu = AcNcpuNS{}
