// AC ilib: native-cpu (ncpu) — JavaScript (Node) FFI
// Bare-metal / low-level toolkit: pointer registry (carried over from `pointers`),
// heap + bare-metal allocation, arena allocator, hard abort, quickthread messaging.

// ============================================================================
// POINTER REGISTRY (global storage, carried over from `pointers`)
// ============================================================================

class PtrEntry {
    constructor(addr, typeId = 0) {
        this.addr = addr;
        this.typeId = typeId;
        this.valid = true;
    }
}

const ptrRegistry = new Map();
let nextPtrId = 1000;
const NULL_PTR = -1;

// ============================================================================
// POINTER CREATION / ACCESS (carried over from `pointers`, called bare)
// ============================================================================

function ptr_new(value, valueSize, typeId = 0) {
    if (value === null || value === undefined) return NULL_PTR;
    const ptrId = nextPtrId++;
    ptrRegistry.set(ptrId, new PtrEntry(value, typeId));
    return ptrId;
}

function ptr_deref(ptrId) {
    const entry = ptrRegistry.get(ptrId);
    if (!entry || !entry.valid) return null;
    if (Buffer.isBuffer(entry.addr)) {
        const end = entry.addr.indexOf(0);
        return entry.addr.toString('utf8', 0, end === -1 ? entry.addr.length : end);
    }
    return entry.addr;
}

function ptr_null() { return NULL_PTR; }
function ptr_is_null(ptrId) { return ptrId === NULL_PTR ? 1 : 0; }

function ptr_eq(ptrId1, ptrId2) {
    if (ptrId1 === ptrId2) return 1;
    if (ptrId1 === NULL_PTR || ptrId2 === NULL_PTR) return 0;
    const e1 = ptrRegistry.get(ptrId1), e2 = ptrRegistry.get(ptrId2);
    if (!e1 || !e2 || !e1.valid || !e2.valid) return 0;
    if (Buffer.isBuffer(e1.addr) && Buffer.isBuffer(e2.addr)) return e1.addr.equals(e2.addr) ? 1 : 0;
    return e1.addr === e2.addr ? 1 : 0;
}

function ptr_copy(ptrId) {
    const entry = ptrRegistry.get(ptrId);
    if (!entry || !entry.valid) return NULL_PTR;
    const dup = Buffer.isBuffer(entry.addr) ? Buffer.from(entry.addr) : entry.addr;
    const newId = nextPtrId++;
    ptrRegistry.set(newId, new PtrEntry(dup, entry.typeId));
    return newId;
}

function ptr_update(ptrId, value, offset = 0) {
    const entry = ptrRegistry.get(ptrId);
    if (!entry || !entry.valid) return -1;
    try {
        if (Buffer.isBuffer(entry.addr)) {
            const data = Buffer.isBuffer(value) ? value : Buffer.from(String(value));
            const need = offset + data.length;
            if (need > entry.addr.length) {
                const grown = Buffer.alloc(need);
                entry.addr.copy(grown);
                entry.addr = grown;
            }
            data.copy(entry.addr, offset);
        } else {
            entry.addr = value;
        }
        return 0;
    } catch (e) {
        return -1;
    }
}

function ptr_free(ptrId) {
    const entry = ptrRegistry.get(ptrId);
    if (!entry) return -1;
    entry.valid = false;
    ptrRegistry.delete(ptrId);
    return 0;
}

// ============================================================================
// ALLOCATION — dha = dynamic heap allocation, z = zeroed, bm = bare-metal
// ============================================================================
// Node has no accessible raw-mmap primitive without a native addon, so bm* here
// is best-effort (a plain Buffer, same as dha/zdha) — a documented host-language
// limitation, not a silent fake. BNY/C/C++/Go/Rust get the real mmap-backed path.

function dha(size, typeId = 0) {
    if (size <= 0) return NULL_PTR;
    const ptrId = nextPtrId++;
    ptrRegistry.set(ptrId, new PtrEntry(Buffer.allocUnsafe(size), typeId));
    return ptrId;
}

function zdha(size, typeId = 0) {
    if (size <= 0) return NULL_PTR;
    const ptrId = nextPtrId++;
    ptrRegistry.set(ptrId, new PtrEntry(Buffer.alloc(size), typeId));
    return ptrId;
}

function ncpu_realloc(ptrId, size) {
    if (size <= 0) return NULL_PTR;
    const entry = ptrRegistry.get(ptrId);
    if (!entry || !entry.valid || !Buffer.isBuffer(entry.addr)) return NULL_PTR;
    const grown = Buffer.alloc(size);
    entry.addr.copy(grown, 0, 0, Math.min(size, entry.addr.length));
    entry.addr = grown;
    return ptrId;
}

function bmdha(size, typeId = 0) { return zdha(size, typeId); }
function bmzdha(size, typeId = 0) { return zdha(size, typeId); }
function bmrealloc(ptrId, size) { return ncpu_realloc(ptrId, size); }

// ============================================================================
// ARENA ALLOCATOR — bump allocator, bulk free
// ============================================================================

const arenaRegistry = new Map();
let nextArenaId = 1;

function arena_create(size) {
    if (size <= 0) return NULL_PTR;
    const arenaId = nextArenaId++;
    arenaRegistry.set(arenaId, { buffer: Buffer.alloc(size), capacity: size, offset: 0 });
    return arenaId;
}

function arena_alloc(arenaId, size) {
    if (size <= 0) return NULL_PTR;
    const arena = arenaRegistry.get(arenaId);
    if (!arena || arena.offset + size > arena.capacity) return NULL_PTR;
    const view = arena.buffer.subarray(arena.offset, arena.offset + size);
    arena.offset += size;
    const ptrId = nextPtrId++;
    ptrRegistry.set(ptrId, new PtrEntry(view, 0));
    return ptrId;
}

// Bump allocators don't free individual items — reclaim via arena_abort/destroy.
function arena_dealloc(arenaId, ptrId) { return 0; }

function arena_destroy(arenaId) {
    return arenaRegistry.delete(arenaId) ? 0 : -1;
}

function arena_abort(arenaId) {
    const arena = arenaRegistry.get(arenaId);
    if (!arena) return -1;
    arena.offset = 0;
    return 0;
}

// ============================================================================
// CONTROL
// ============================================================================

// Hard bare-metal abort — process.abort() raises SIGABRT immediately (with a
// core dump if enabled), not a clean process.exit().
function ncpu_abort() { process.abort(); }

// ============================================================================
// MESSAGING (between quickthreads) — spelling "recieve" is intentional
// ============================================================================

const _messages = new Set();

function broadcast(msg) { _messages.add(msg); }
function recieve(msg) {
    if (_messages.has(msg)) { _messages.delete(msg); return 1; }
    return 0;
}

// ── AC-facing dotted namespace: `ncpu.dha(...)`, `ncpu.abort()`, etc. ──
const ncpu = {
    dha, zdha, realloc: ncpu_realloc, bmdha, bmzdha, bmrealloc,
    arena_create, arena_alloc, arena_dealloc, arena_destroy, arena_abort,
    abort: ncpu_abort, broadcast, recieve,
};

if (typeof module !== 'undefined' && module.exports) {
    module.exports = {
        ptr_new, ptr_deref, ptr_null, ptr_is_null, ptr_eq, ptr_copy, ptr_update, ptr_free,
        dha, zdha, ncpu_realloc, bmdha, bmzdha, bmrealloc,
        arena_create, arena_alloc, arena_dealloc, arena_destroy, arena_abort,
        ncpu_abort, broadcast, recieve, ncpu,
    };
}
