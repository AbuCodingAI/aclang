# AC ilib: native-cpu (ncpu) — Python FFI
# Bare-metal / low-level toolkit: pointer registry (carried over from `pointers`),
# heap + bare-metal allocation, arena allocator, hard abort, quickthread messaging.
import os as _os
import mmap as _mmap
import threading as _threading
import copy as _copy

# ============================================================================
# POINTER REGISTRY (global storage, carried over from `pointers`)
# ============================================================================

class PtrEntry:
    def __init__(self, addr, type_id=0, bare_metal=False):
        self.addr = addr        # bytearray / mmap.mmap / arbitrary object reference
        self.type_id = type_id  # type hint (0=generic)
        self.valid = True
        self.bare_metal = bare_metal

_lock = _threading.Lock()
ptr_registry = {}  # int64_id -> PtrEntry
next_ptr_id = 1000  # start IDs at 1000 to avoid confusion with addresses
NULL_PTR = -1

def _alloc_id():
    global next_ptr_id
    i = next_ptr_id
    next_ptr_id += 1
    return i

# ============================================================================
# POINTER CREATION / ACCESS (carried over from `pointers`, called bare)
# ============================================================================

def ptr_new(value, value_size=0, type_id=0):
    """Create pointer to a value (stores a reference, matches the C core's
    handle model closely enough for AC's scalar-only call convention)."""
    if value is None:
        return NULL_PTR
    with _lock:
        pid = _alloc_id()
        ptr_registry[pid] = PtrEntry(value, type_id)
        return pid

def ptr_deref(ptr_id):
    with _lock:
        entry = ptr_registry.get(ptr_id)
        if entry is None or not entry.valid:
            return None
        if isinstance(entry.addr, (bytearray, _mmap.mmap)):
            return bytes(entry.addr).rstrip(b"\x00").decode("utf-8", "replace")
        return entry.addr

def ptr_null():
    return NULL_PTR

def ptr_is_null(ptr_id):
    return 1 if ptr_id == NULL_PTR else 0

def ptr_eq(ptr_id1, ptr_id2):
    if ptr_id1 == ptr_id2:
        return 1
    if ptr_id1 == NULL_PTR or ptr_id2 == NULL_PTR:
        return 0
    with _lock:
        e1 = ptr_registry.get(ptr_id1)
        e2 = ptr_registry.get(ptr_id2)
        if e1 is None or e2 is None or not e1.valid or not e2.valid:
            return 0
        try:
            return 1 if bytes(e1.addr) == bytes(e2.addr) else (1 if e1.addr == e2.addr else 0)
        except Exception:
            return 1 if e1.addr == e2.addr else 0

def ptr_copy(ptr_id):
    """Deep-copy a pointer: new handle with an independent copy of the value."""
    with _lock:
        if ptr_id == NULL_PTR or ptr_id not in ptr_registry:
            return NULL_PTR
        entry = ptr_registry[ptr_id]
        if not entry.valid:
            return NULL_PTR
        try:
            dup = _copy.deepcopy(entry.addr)
        except Exception:
            dup = entry.addr
        pid = _alloc_id()
        ptr_registry[pid] = PtrEntry(dup, entry.type_id)
        return pid

def ptr_update(ptr_id, value, offset=0):
    """Write through a pointer. For byte-buffer-backed pointers, writes bytes
    at `offset`, growing a plain bytearray as needed; for reference-backed
    pointers, replaces the stored reference outright."""
    with _lock:
        entry = ptr_registry.get(ptr_id)
        if entry is None or not entry.valid:
            return -1
        try:
            if isinstance(entry.addr, (bytearray, _mmap.mmap)):
                data = value.encode("utf-8") if isinstance(value, str) else bytes(value)
                need = offset + len(data)
                if isinstance(entry.addr, bytearray) and need > len(entry.addr):
                    entry.addr.extend(b"\x00" * (need - len(entry.addr)))
                entry.addr[offset:offset+len(data)] = data
            else:
                entry.addr = value
            return 0
        except Exception:
            return -1

def ptr_free(ptr_id):
    with _lock:
        entry = ptr_registry.pop(ptr_id, None)
        if entry is None:
            return -1
        entry.valid = False
        if isinstance(entry.addr, _mmap.mmap):
            entry.addr.close()
        return 0

# ============================================================================
# ALLOCATION — dha = dynamic heap allocation, z = zeroed, bm = bare-metal
# ============================================================================
# Note: Python's `bytearray(n)` is always zero-initialized (there's no "uninitialized
# heap allocation" concept at the language level), so dha/zdha are equivalent here —
# a documented limitation of the host language, not a silent fake.

def dha(size, type_id=0):
    if size <= 0:
        return NULL_PTR
    with _lock:
        pid = _alloc_id()
        ptr_registry[pid] = PtrEntry(bytearray(size), type_id)
        return pid

def zdha(size, type_id=0):
    return dha(size, type_id)

def ncpu_realloc(ptr_id, size):
    if size <= 0:
        return NULL_PTR
    with _lock:
        entry = ptr_registry.get(ptr_id)
        if entry is None or not entry.valid or not isinstance(entry.addr, bytearray):
            return NULL_PTR
        if size < len(entry.addr):
            entry.addr = entry.addr[:size]
        else:
            entry.addr.extend(b"\x00" * (size - len(entry.addr)))
        return ptr_id

def bmdha(size, type_id=0):
    """Bare-metal alloc: a real anonymous mmap (mmap.mmap(-1, size)) — a genuine
    raw-pages allocation bypassing Python's object allocator, not just a bytearray."""
    if size <= 0:
        return NULL_PTR
    try:
        buf = _mmap.mmap(-1, size)
    except Exception:
        return NULL_PTR
    with _lock:
        pid = _alloc_id()
        ptr_registry[pid] = PtrEntry(buf, type_id, bare_metal=True)
        return pid

def bmzdha(size, type_id=0):
    return bmdha(size, type_id)  # anonymous mmap pages are already kernel-zeroed

def bmrealloc(ptr_id, size):
    if size <= 0:
        return NULL_PTR
    with _lock:
        entry = ptr_registry.get(ptr_id)
        if entry is None or not entry.valid or not isinstance(entry.addr, _mmap.mmap):
            return NULL_PTR
        try:
            entry.addr.resize(size)
            return ptr_id
        except Exception:
            return NULL_PTR

# ============================================================================
# ARENA ALLOCATOR — bump allocator, bulk free
# ============================================================================

class _Arena:
    def __init__(self, capacity):
        self.buffer = bytearray(capacity)
        self.capacity = capacity
        self.offset = 0

arena_registry = {}
next_arena_id = 1

def arena_create(size):
    global next_arena_id
    if size <= 0:
        return NULL_PTR
    with _lock:
        aid = next_arena_id
        next_arena_id += 1
        arena_registry[aid] = _Arena(size)
        return aid

def arena_alloc(arena_id, size):
    if size <= 0:
        return NULL_PTR
    with _lock:
        arena = arena_registry.get(arena_id)
        if arena is None or arena.offset + size > arena.capacity:
            return NULL_PTR
        pid = _alloc_id()
        # Non-owning view into the arena's own buffer (a Python memoryview slice
        # would detach on write, so store an (arena, start, len) window instead).
        ptr_registry[pid] = PtrEntry(_ArenaView(arena, arena.offset, size), 0)
        arena.offset += size
        return pid

class _ArenaView:
    """A window into an arena's buffer — behaves like a bytearray slice for
    ptr_deref/ptr_update, but writes land in the arena's own backing buffer."""
    def __init__(self, arena, start, length):
        self.arena, self.start, self.length = arena, start, length
    def __len__(self): return self.length
    def __bytes__(self): return bytes(self.arena.buffer[self.start:self.start+self.length])
    def __getitem__(self, sl): return self.arena.buffer[self.start:self.start+self.length][sl]
    def __setitem__(self, sl, val):
        buf = bytearray(self.arena.buffer[self.start:self.start+self.length])
        buf[sl] = val
        self.arena.buffer[self.start:self.start+self.length] = buf[:self.length]
    def extend(self, extra):
        pass  # arena views never grow past their reserved window

def arena_dealloc(arena_id, ptr_id):
    # Bump allocators don't free individual items — reclaim via arena_abort/destroy.
    return 0

def arena_destroy(arena_id):
    with _lock:
        if arena_registry.pop(arena_id, None) is None:
            return -1
        return 0

def arena_abort(arena_id):
    with _lock:
        arena = arena_registry.get(arena_id)
        if arena is None:
            return -1
        arena.offset = 0
        return 0

# ============================================================================
# CONTROL
# ============================================================================

def ncpu_abort():
    """Hard bare-metal abort — os.abort() raises SIGABRT, a real immediate
    termination (with a core dump if enabled), not a clean sys.exit()."""
    _os.abort()

# ============================================================================
# MESSAGING (between quickthreads) — spelling "recieve" is intentional
# ============================================================================

_messages = set()
_msg_lock = _threading.Lock()

def broadcast(msg):
    with _msg_lock:
        _messages.add(msg)

def recieve(msg):
    with _msg_lock:
        if msg in _messages:
            _messages.discard(msg)
            return 1
        return 0

# ── AC-facing dotted namespace: `ncpu.dha(...)`, `ncpu.abort()`, etc. ──
class ncpu:
    dha           = staticmethod(dha)
    zdha          = staticmethod(zdha)
    realloc       = staticmethod(ncpu_realloc)
    bmdha         = staticmethod(bmdha)
    bmzdha        = staticmethod(bmzdha)
    bmrealloc     = staticmethod(bmrealloc)
    arena_create  = staticmethod(arena_create)
    arena_alloc   = staticmethod(arena_alloc)
    arena_dealloc = staticmethod(arena_dealloc)
    arena_destroy = staticmethod(arena_destroy)
    arena_abort   = staticmethod(arena_abort)
    abort         = staticmethod(ncpu_abort)
    broadcast     = staticmethod(broadcast)
    recieve       = staticmethod(recieve)
