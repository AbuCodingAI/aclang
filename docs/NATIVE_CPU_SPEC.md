# `native-cpu` (`ncpu`) — Library Spec

**Status:** IMPLEMENTED 2026-08-01 (targets AC 1.1.x), except the `atomic` type keyword (still
pending — see "Type keyword" section below, left deferred on purpose). Rename from `pointers` +
the allocator/arena/abort/messaging surface are done, verified end-to-end (real toolchain, not just
compile-checks) on PY/JS/C/CPP/GO/RS/V/JAVA/BNY (13/14 `--all` targets; ASM verified by manual
assemble+link+run since the ASM backend has no automatic runner). Java has one pre-existing,
unchanged gap: the carried-over bare `ptr_*` calls don't resolve (Java-codegen architecture issue,
not specific to this library — see `docs/LIBRARY_GUIDE.md`'s native-cpu section for the full writeup).
See `library/ilib/native-cpu/` and the `docs/LIBRARY_GUIDE.md` entry for the actual current surface.

`native-cpu` is AC's **low-level / bare-metal toolkit** — it replaces the `pointers` library. It
gives programs direct, unmanaged access to memory, allocation, concurrency, and messaging, in the
spirit of "you own the CPU."

## Rename / wiring

- Directory: `library/ilib/pointers/` → `library/ilib/native-cpu/`.
- Token / lowering: the `.acl` prefix `pointers:` → `native-cpu:` (the token) → `ncpu.` call form.
- All existing `pointers` functions carry over (see **Pointers** below).

---

## Allocation

Naming: `dha` = **d**ynamic **h**eap **a**llocation, `z` = zeroed, `bm` = **bare-metal**.

| Function | Meaning |
|---|---|
| `ncpu.dha(size)` | malloc-style raw allocation |
| `ncpu.zdha(size)` | calloc-style zeroed allocation |
| `ncpu.realloc(ptr, size)` | resize an allocation |
| `ncpu.bmdha(size, optional)` | bare-metal alloc — **no OS/libc allocator**, straight to raw pages/hardware, or second arguement |
| `ncpu.bmzdha(size, optional)` | bare-metal zeroed alloc |
| `ncpu.bmrealloc(ptr, size, optional)` | bare-metal resize |

The `bm*` twins bypass the libc allocator entirely (raw mmap/pages), for freestanding / zero-dep use.

## Arena allocator

A bump allocator — fast allocation with bulk free.

| Function | Meaning |
|---|---|
| `ncpu.arena_create(...)` | create an arena |
| `ncpu.arena_alloc(arena, size)` | bump-allocate from the arena |
| `ncpu.arena_dealloc(...)` | free within the arena |
| `ncpu.arena_destroy(arena)` | tear the arena down |
| `ncpu.arena_abort(arena)` | emergency — **nuke the whole arena** on error |

## Control

| Function | Meaning |
|---|---|
| `ncpu.abort()` | hard bare-metal abort — CPU trap/halt, **not** a clean exit |

## Concurrency — `quickthread`

- `ncpu.quickthread` — **not** a "coroutine".
- `quickthread` is an **existing AC core feature** (goroutine-like, a CPU-friendlier name).
- `ncpu.quickthread` is a **more bare-metal, more CPU-friendly variant** of the core `quickthread`.

## Messaging (between quickthreads)

| Function | Meaning |
|---|---|
| `ncpu.broadcast("Shutdown_request")` | send a named message to all quickthreads |
| `ncpu.recieve("Shutdown_request")` | returns true/false **and** consumes the message if present |

> Note: the surface spelling is **`recieve`** (author's choice), not `receive`.

## Pointers (carried over from `pointers`)

The existing pointer functions remain, memory-safety-hardened (size-tracked allocations, bounded
deref, growing update, deep copy, value-comparing eq):

- `ptr_new` — allocate/box a value, return a pointer
- `ptr_deref` — read through a pointer (bounded by tracked size)
- `ptr_copy` — deep copy
- `ptr_update` — write through a pointer (grows via realloc)
- `ptr_eq` — value comparison (memcmp)
- `ptr_null` — the null pointer
- `ptr_is_null` — null check

## Type keyword — `atomic`

- `atomic s = 5` — `atomic` is a **type keyword** declaring an atomic variable, part of native-cpu.
- Rest of the design (operations, memory ordering, per-backend lowering) is **TBD by the author**.

---

## Implementation notes (for whoever builds this)

- **Backends:** like every ilib, it needs a C++ core + `extern "C"` C shim + per-backend FFI
  (`ffi/*.py|js|c|cpp|rs|go|v|java`) + the `.acl` lowering file, following the pattern of the other
  `library/ilib/*` dirs. The `bm*` / `abort` / arena paths are the freestanding-friendly ones and
  should be usable under `--static-link` (they use raw syscalls, no libc), which dovetails with the
  BNY Option-2 splicer (see `HANDOFF.md` §2d and `library/ilib/math/freestanding/`).
- **`quickthread` core vs ncpu variant:** confirm how the existing core `quickthread` is implemented
  before adding the bare-metal variant, so they share semantics where sensible.
- **`atomic` type:** wire it like `short`/`mini` did — add an `IRType`, tokenize the keyword, map it
  in `include/type.hpp` per backend (C++ `std::atomic<…>`, Rust `AtomicI64`, Go `atomic.Int64`, JS
  `Atomics`/`SharedArrayBuffer`, etc.), and decide BNY's `lock`-prefixed instruction lowering.

*Source of record: this file + `HANDOFF.md` §2a. Confirmed surface, 2026-07-16.*
