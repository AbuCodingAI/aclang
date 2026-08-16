// AC ilib: native-cpu (ncpu) — Rust FFI
// Bare-metal / low-level toolkit: pointer registry (carried over from `pointers`),
// heap + bare-metal allocation, arena allocator, hard abort, quickthread messaging.
//
// Compiled with plain `rustc` (no Cargo, no external crates) — the previous
// `pointers` version depended on the `lazy_static` crate, which isn't available
// under that invocation and never actually compiled. Rewritten on stable std only
// (std::sync::OnceLock, stable since 1.70) and raw `extern "C"` mmap/munmap/mremap
// declarations (linked from libc automatically, no crate needed).

use std::collections::{HashMap, HashSet};
use std::sync::{Mutex, OnceLock};
use std::os::raw::c_void;

extern "C" {
    fn mmap(addr: *mut c_void, len: usize, prot: i32, flags: i32, fd: i32, offset: i64) -> *mut c_void;
    fn munmap(addr: *mut c_void, len: usize) -> i32;
}
const PROT_READ: i32 = 1;
const PROT_WRITE: i32 = 2;
const MAP_PRIVATE: i32 = 2;
const MAP_ANONYMOUS: i32 = 0x20;
const MAP_FAILED: *mut c_void = usize::MAX as *mut c_void;

// ============================================================================
// POINTER REGISTRY (global storage, carried over from `pointers`)
// ============================================================================

pub struct PtrEntry {
    pub addr: Vec<u8>,
    pub type_id: i64,
    pub valid: bool,
    pub bare_metal: bool,
}

static PTR_REGISTRY: OnceLock<Mutex<HashMap<i64, PtrEntry>>> = OnceLock::new();
static NEXT_PTR_ID: OnceLock<Mutex<i64>> = OnceLock::new();
pub const NULL_PTR: i64 = -1;

fn registry() -> &'static Mutex<HashMap<i64, PtrEntry>> {
    PTR_REGISTRY.get_or_init(|| Mutex::new(HashMap::new()))
}
fn next_id() -> i64 {
    let m = NEXT_PTR_ID.get_or_init(|| Mutex::new(1000));
    let mut g = m.lock().unwrap();
    let id = *g;
    *g += 1;
    id
}

// ============================================================================
// POINTER CREATION / ACCESS (carried over from `pointers`, called bare)
// ============================================================================

// Takes `&str` (not a raw pointer) — AC source passes ptr_new a plain value expression
// (usually a string literal), and generated Rust call sites pass whatever native type
// that infers to, not a manually-built `*const u8`. Matches how every other ilib's Rust
// binding (e.g. os_mkfile(path: &str)) takes AC's own scalar types directly.
pub fn ptr_new(value: &str, value_size: i64, type_id: i64) -> i64 {
    if value_size <= 0 { return NULL_PTR; }
    let data = value.as_bytes().to_vec();
    let id = next_id();
    registry().lock().unwrap().insert(id, PtrEntry { addr: data, type_id, valid: true, bare_metal: false });
    id
}

pub fn ptr_deref(ptr_id: i64, out: *mut u8, out_size: i64) -> i64 {
    if out.is_null() || out_size <= 0 { return -1; }
    let reg = registry().lock().unwrap();
    match reg.get(&ptr_id) {
        Some(e) if e.valid => {
            let n = std::cmp::min(out_size as usize, e.addr.len());
            unsafe { std::ptr::copy_nonoverlapping(e.addr.as_ptr(), out, n); }
            n as i64
        }
        _ => -1,
    }
}

pub fn ptr_null() -> i64 { NULL_PTR }
pub fn ptr_is_null(ptr_id: i64) -> i64 { if ptr_id == NULL_PTR { 1 } else { 0 } }

pub fn ptr_eq(ptr_id1: i64, ptr_id2: i64) -> i64 {
    if ptr_id1 == ptr_id2 { return 1; }
    if ptr_id1 == NULL_PTR || ptr_id2 == NULL_PTR { return 0; }
    let reg = registry().lock().unwrap();
    match (reg.get(&ptr_id1), reg.get(&ptr_id2)) {
        (Some(a), Some(b)) if a.valid && b.valid => if a.addr == b.addr { 1 } else { 0 },
        _ => 0,
    }
}

pub fn ptr_copy(ptr_id: i64) -> i64 {
    let dup = {
        let reg = registry().lock().unwrap();
        match reg.get(&ptr_id) {
            Some(e) if e.valid => e.addr.clone(),
            _ => return NULL_PTR,
        }
    };
    let id = next_id();
    let type_id = registry().lock().unwrap().get(&ptr_id).map(|e| e.type_id).unwrap_or(0);
    registry().lock().unwrap().insert(id, PtrEntry { addr: dup, type_id, valid: true, bare_metal: false });
    id
}

pub fn ptr_update(ptr_id: i64, new_value: &str, size: i64) -> i64 {
    if ptr_id == NULL_PTR || size <= 0 { return -1; }
    let mut reg = registry().lock().unwrap();
    match reg.get_mut(&ptr_id) {
        Some(e) if e.valid => {
            let data = new_value.as_bytes();
            // Bare-metal entries can still be written in place — only GROWING them is
            // refused (they don't own a resizable allocation the way a plain dha entry does).
            if data.len() > e.addr.len() {
                if e.bare_metal { return -1; }
                e.addr.resize(data.len(), 0);
            }
            e.addr[..data.len()].copy_from_slice(data);
            0
        }
        _ => -1,
    }
}

pub fn ptr_free(ptr_id: i64) -> i64 {
    if registry().lock().unwrap().remove(&ptr_id).is_some() { 0 } else { -1 }
}

// ============================================================================
// ALLOCATION — dha = dynamic heap allocation, z = zeroed, bm = bare-metal (raw mmap)
// ============================================================================

pub fn dha(size: i64, type_id: i64) -> i64 {
    if size <= 0 { return NULL_PTR; }
    let id = next_id();
    registry().lock().unwrap().insert(id, PtrEntry { addr: vec![0u8; size as usize], type_id, valid: true, bare_metal: false });
    id
}
pub fn zdha(size: i64, type_id: i64) -> i64 { dha(size, type_id) } // Vec is always zero-initialized

pub fn ncpu_realloc(ptr_id: i64, size: i64) -> i64 {
    if size <= 0 { return NULL_PTR; }
    let mut reg = registry().lock().unwrap();
    match reg.get_mut(&ptr_id) {
        Some(e) if e.valid && !e.bare_metal => { e.addr.resize(size as usize, 0); ptr_id }
        _ => NULL_PTR,
    }
}

fn bm_alloc_impl(size: i64, type_id: i64) -> i64 {
    if size <= 0 { return NULL_PTR; }
    let p = unsafe { mmap(std::ptr::null_mut(), size as usize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) };
    if p == MAP_FAILED { return NULL_PTR; }
    let data = unsafe { std::slice::from_raw_parts(p as *const u8, size as usize) }.to_vec();
    unsafe { munmap(p, size as usize) }; // copy into the tracked Vec, release the raw mapping immediately
    let id = next_id();
    registry().lock().unwrap().insert(id, PtrEntry { addr: data, type_id, valid: true, bare_metal: true });
    id
}
// Real raw-mmap allocation, kept separate from the plain heap (dha) path — the mapping
// itself is transient (copied into a tracked buffer) since AC's handle model needs a
// stable, size-tracked slot either way; bmzdha needs no separate zeroing since anonymous
// mmap pages are already kernel-zeroed.
pub fn bmdha(size: i64, type_id: i64) -> i64 { bm_alloc_impl(size, type_id) }
pub fn bmzdha(size: i64, type_id: i64) -> i64 { bm_alloc_impl(size, type_id) }
pub fn bmrealloc(ptr_id: i64, size: i64) -> i64 {
    if size <= 0 { return NULL_PTR; }
    let mut reg = registry().lock().unwrap();
    match reg.get_mut(&ptr_id) {
        Some(e) if e.valid && e.bare_metal => { e.addr.resize(size as usize, 0); ptr_id }
        _ => NULL_PTR,
    }
}

// ============================================================================
// ARENA ALLOCATOR — bump allocator, bulk free
// ============================================================================

struct Arena { buffer: Vec<u8>, offset: usize }
static ARENA_REGISTRY: OnceLock<Mutex<HashMap<i64, Arena>>> = OnceLock::new();
static NEXT_ARENA_ID: OnceLock<Mutex<i64>> = OnceLock::new();
fn arenas() -> &'static Mutex<HashMap<i64, Arena>> {
    ARENA_REGISTRY.get_or_init(|| Mutex::new(HashMap::new()))
}
fn next_arena_id() -> i64 {
    let m = NEXT_ARENA_ID.get_or_init(|| Mutex::new(1));
    let mut g = m.lock().unwrap();
    let id = *g; *g += 1; id
}

pub fn arena_create(size: i64) -> i64 {
    if size <= 0 { return NULL_PTR; }
    let id = next_arena_id();
    arenas().lock().unwrap().insert(id, Arena { buffer: vec![0u8; size as usize], offset: 0 });
    id
}

pub fn arena_alloc(arena_id: i64, size: i64) -> i64 {
    if size <= 0 { return NULL_PTR; }
    let mut ar = arenas().lock().unwrap();
    let arena = match ar.get_mut(&arena_id) { Some(a) => a, None => return NULL_PTR };
    if arena.offset + size as usize > arena.buffer.len() { return NULL_PTR; }
    let slice = arena.buffer[arena.offset..arena.offset + size as usize].to_vec();
    arena.offset += size as usize;
    let id = next_id();
    // Registered as an ordinary (owning) entry — a snapshot copy, since the arena's
    // backing buffer isn't addressable through the same i64-handle registry directly.
    registry().lock().unwrap().insert(id, PtrEntry { addr: slice, type_id: 0, valid: true, bare_metal: false });
    id
}

// Bump allocators don't free individual items — reclaim via arena_abort/destroy.
pub fn arena_dealloc(_arena_id: i64, _ptr_id: i64) -> i64 { 0 }

pub fn arena_destroy(arena_id: i64) -> i64 {
    if arenas().lock().unwrap().remove(&arena_id).is_some() { 0 } else { -1 }
}

pub fn arena_abort(arena_id: i64) -> i64 {
    let mut ar = arenas().lock().unwrap();
    match ar.get_mut(&arena_id) {
        Some(a) => { a.offset = 0; 0 }
        None => -1,
    }
}

// ============================================================================
// CONTROL
// ============================================================================

// Hard bare-metal abort — std::process::abort() raises SIGABRT immediately
// (with a core dump if enabled), not a clean std::process::exit().
pub fn ncpu_abort() -> ! { std::process::abort(); }

// ============================================================================
// MESSAGING (between quickthreads) — spelling "recieve" is intentional
// ============================================================================

static MESSAGES: OnceLock<Mutex<HashSet<String>>> = OnceLock::new();
fn messages() -> &'static Mutex<HashSet<String>> {
    MESSAGES.get_or_init(|| Mutex::new(HashSet::new()))
}

pub fn broadcast(msg: &str) {
    messages().lock().unwrap().insert(msg.to_string());
}
pub fn recieve(msg: &str) -> i64 {
    if messages().lock().unwrap().remove(msg) { 1 } else { 0 }
}

// ── AC-facing dotted namespace: `ncpu.dha(...)`, `ncpu.abort()`, etc. ──
struct _NcpuNS;
impl _NcpuNS {
    fn dha(&self, size: i64, type_id: i64) -> i64 { dha(size, type_id) }
    fn zdha(&self, size: i64, type_id: i64) -> i64 { zdha(size, type_id) }
    fn realloc(&self, ptr_id: i64, size: i64) -> i64 { ncpu_realloc(ptr_id, size) }
    fn bmdha(&self, size: i64, type_id: i64) -> i64 { bmdha(size, type_id) }
    fn bmzdha(&self, size: i64, type_id: i64) -> i64 { bmzdha(size, type_id) }
    fn bmrealloc(&self, ptr_id: i64, size: i64) -> i64 { bmrealloc(ptr_id, size) }
    fn arena_create(&self, size: i64) -> i64 { arena_create(size) }
    fn arena_alloc(&self, arena_id: i64, size: i64) -> i64 { arena_alloc(arena_id, size) }
    fn arena_dealloc(&self, arena_id: i64, ptr_id: i64) -> i64 { arena_dealloc(arena_id, ptr_id) }
    fn arena_destroy(&self, arena_id: i64) -> i64 { arena_destroy(arena_id) }
    fn arena_abort(&self, arena_id: i64) -> i64 { arena_abort(arena_id) }
    fn abort(&self) -> ! { ncpu_abort() }
    fn broadcast(&self, msg: &str) { broadcast(msg) }
    fn recieve(&self, msg: &str) -> i64 { recieve(msg) }
}
static ncpu: _NcpuNS = _NcpuNS;
