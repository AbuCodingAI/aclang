; AC ilib: native-cpu (ncpu) — x86-64 NASM extern declarations (libacncpu.so)
; Link with: nasm -f elf64 output.asm && gcc output.o -L<libdir> -lacncpu -Wl,-rpath,<libdir> -o output
;
; The AC->ASM backend's ilib injection only pulls lines starting with "extern "
; out of this file (see AsmStrategy::emitHeader) — it does NOT inline the rest of
; the file, so no trampoline/wrapper layer defined here would ever be emitted.
; Call sites mangle dotted calls to underscores (ncpu.dha -> ncpu_dha) and leave
; bare calls (ptr_new) alone, so these `extern` names must be the ACTUAL symbols
; the call sites use — libacncpu.so exports both the ac_ncpu_* core names and
; these bare/mangled aliases (see native-cpu.cpp's "BARE-NAME ALIASES" section).

extern ptr_new
extern ptr_deref
extern ptr_is_null
extern ptr_null
extern ptr_eq
extern ptr_copy
extern ptr_update
extern ptr_free

extern ncpu_dha
extern ncpu_zdha
extern ncpu_realloc
extern ncpu_bmdha
extern ncpu_bmzdha
extern ncpu_bmrealloc
extern ncpu_arena_create
extern ncpu_arena_alloc
extern ncpu_arena_dealloc
extern ncpu_arena_destroy
extern ncpu_arena_abort
extern ncpu_abort
extern ncpu_broadcast
extern ncpu_recieve
