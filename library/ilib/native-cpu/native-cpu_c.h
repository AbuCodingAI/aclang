#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Pointer registry (carried over from `pointers`, called bare: ptr_new(...), not ncpu.ptr_new(...))
int64_t     ac_ncpu_ptr_new(const void* value, int64_t value_size, int64_t type_id);
int64_t     ac_ncpu_ptr_deref(int64_t ptr_id, void* out, int64_t out_size);
int         ac_ncpu_ptr_is_null(int64_t ptr_id);
int64_t     ac_ncpu_ptr_null(void);
int         ac_ncpu_ptr_eq(int64_t ptr1, int64_t ptr2);
int64_t     ac_ncpu_ptr_copy(int64_t ptr_id);
int         ac_ncpu_ptr_update(int64_t ptr_id, void* new_value, int64_t size);
int         ac_ncpu_ptr_free(int64_t ptr_id);

// Allocation (called as ncpu.dha(...) etc)
int64_t     ac_ncpu_dha(int64_t size, int64_t type_id);
int64_t     ac_ncpu_zdha(int64_t size, int64_t type_id);
int64_t     ac_ncpu_realloc(int64_t ptr_id, int64_t size);
int64_t     ac_ncpu_bmdha(int64_t size, int64_t type_id);
int64_t     ac_ncpu_bmzdha(int64_t size, int64_t type_id);
int64_t     ac_ncpu_bmrealloc(int64_t ptr_id, int64_t size);

// Arena allocator
int64_t     ac_ncpu_arena_create(int64_t size);
int64_t     ac_ncpu_arena_alloc(int64_t arena_id, int64_t size);
int         ac_ncpu_arena_dealloc(int64_t arena_id, int64_t ptr_id);
int         ac_ncpu_arena_destroy(int64_t arena_id);
int         ac_ncpu_arena_abort(int64_t arena_id);

// Control
void        ac_ncpu_abort(void);

// Messaging (between quickthreads)
void        ac_ncpu_broadcast(const char* msg);
int         ac_ncpu_recieve(const char* msg);

// Utility
int64_t     ac_ncpu_count(void);
const char* ac_ncpu_version(void);

#ifdef __cplusplus
}
#endif

/* Call-form shims: the AC compiler emits ncpu_x(...) on C and ncpu.x(...) on C++
 * (matching os_c.h's pattern) — but the carried-over ptr_* functions are called
 * BARE on every backend (ptr_new, not ncpu.ptr_new/ncpu_ptr_new), matching the
 * original `pointers` library convention and existing user code. */
#define ptr_new    ac_ncpu_ptr_new
#define ptr_deref  ac_ncpu_ptr_deref
#define ptr_null   ac_ncpu_ptr_null
#define ptr_is_null ac_ncpu_ptr_is_null
#define ptr_eq     ac_ncpu_ptr_eq
#define ptr_copy   ac_ncpu_ptr_copy
#define ptr_update ac_ncpu_ptr_update

#ifdef __cplusplus
struct _ac_ncpu_ns {
    int64_t     (*dha)(int64_t, int64_t) = ac_ncpu_dha;
    int64_t     (*zdha)(int64_t, int64_t) = ac_ncpu_zdha;
    int64_t     (*realloc)(int64_t, int64_t) = ac_ncpu_realloc;
    int64_t     (*bmdha)(int64_t, int64_t) = ac_ncpu_bmdha;
    int64_t     (*bmzdha)(int64_t, int64_t) = ac_ncpu_bmzdha;
    int64_t     (*bmrealloc)(int64_t, int64_t) = ac_ncpu_bmrealloc;
    int64_t     (*arena_create)(int64_t) = ac_ncpu_arena_create;
    int64_t     (*arena_alloc)(int64_t, int64_t) = ac_ncpu_arena_alloc;
    int         (*arena_dealloc)(int64_t, int64_t) = ac_ncpu_arena_dealloc;
    int         (*arena_destroy)(int64_t) = ac_ncpu_arena_destroy;
    int         (*arena_abort)(int64_t) = ac_ncpu_arena_abort;
    void        (*abort)() = ac_ncpu_abort;
    void        (*broadcast)(const char*) = ac_ncpu_broadcast;
    int         (*recieve)(const char*) = ac_ncpu_recieve;
};
static _ac_ncpu_ns ncpu;
#else
#define ncpu_dha ac_ncpu_dha
#define ncpu_zdha ac_ncpu_zdha
#define ncpu_realloc ac_ncpu_realloc
#define ncpu_bmdha ac_ncpu_bmdha
#define ncpu_bmzdha ac_ncpu_bmzdha
#define ncpu_bmrealloc ac_ncpu_bmrealloc
#define ncpu_arena_create ac_ncpu_arena_create
#define ncpu_arena_alloc ac_ncpu_arena_alloc
#define ncpu_arena_dealloc ac_ncpu_arena_dealloc
#define ncpu_arena_destroy ac_ncpu_arena_destroy
#define ncpu_arena_abort ac_ncpu_arena_abort
#define ncpu_abort ac_ncpu_abort
#define ncpu_broadcast ac_ncpu_broadcast
#define ncpu_recieve ac_ncpu_recieve
#endif
