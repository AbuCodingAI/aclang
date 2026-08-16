// AC ilib: native-cpu (ncpu) — C++ shared library implementation
// Bare-metal / low-level toolkit: pointer registry (carried over from `pointers`),
// heap + bare-metal allocation, arena allocator, hard abort, quickthread messaging.
// Build: g++ -O2 -fPIC -shared native-cpu.cpp native-cpu_c.cpp -o libacncpu.so

#include <unordered_map>
#include <set>
#include <string>
#include <mutex>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <sys/mman.h>
#include <unistd.h>

// ============================================================================
// POINTER REGISTRY (global storage, carried over from `pointers`)
// ============================================================================

struct PtrEntry {
    void* addr;         // actual pointer/address (owned unless owns_memory==false)
    int64_t size;        // bytes allocated at addr — tracked so every copy/deref is bounded
    int64_t type_id;      // type hint (0=generic)
    bool valid;          // is this entry valid?
    bool bare_metal;     // true if addr came from mmap (free via munmap, not free())
    bool owns_memory;    // false for arena sub-allocations — the arena owns the block
};

static std::mutex g_ncpuMutex;
static std::unordered_map<int64_t, PtrEntry> ptr_registry;
static int64_t next_ptr_id = 1000;  // start IDs at 1000 to avoid confusion with addresses
static const int64_t NULL_PTR = -1;

struct ArenaEntry {
    void* buffer;
    int64_t capacity;
    int64_t offset;
};
static std::unordered_map<int64_t, ArenaEntry> arena_registry;
static int64_t next_arena_id = 1;

static std::set<std::string> g_ncpuMessages;

extern "C" {

// ============================================================================
// POINTER CREATION / ACCESS (carried over from `pointers`)
// ============================================================================

int64_t ac_ncpu_ptr_new(const void* value, int64_t value_size, int64_t type_id) {
    if (!value || value_size <= 0) return NULL_PTR;
    std::lock_guard<std::mutex> lock(g_ncpuMutex);
    try {
        void* stored = malloc(value_size);
        if (!stored) return NULL_PTR;
        memcpy(stored, value, value_size);
        int64_t id = next_ptr_id++;
        ptr_registry[id] = {stored, value_size, type_id, true, false, true};
        return id;
    } catch (...) { return NULL_PTR; }
}

int64_t ac_ncpu_ptr_deref(int64_t ptr_id, void* out, int64_t out_size) {
    if (!out || out_size <= 0) return -1;
    std::lock_guard<std::mutex> lock(g_ncpuMutex);
    auto it = ptr_registry.find(ptr_id);
    if (it == ptr_registry.end() || !it->second.valid) return -1;
    int64_t n = out_size < it->second.size ? out_size : it->second.size;
    if (n < 0) n = 0;
    memcpy(out, it->second.addr, (size_t)n);
    return n;
}

int ac_ncpu_ptr_is_null(int64_t ptr_id) { return ptr_id == NULL_PTR ? 1 : 0; }
int64_t ac_ncpu_ptr_null() { return NULL_PTR; }

int ac_ncpu_ptr_eq(int64_t ptr1, int64_t ptr2) {
    if (ptr1 == ptr2) return 1;
    if (ptr1 == NULL_PTR || ptr2 == NULL_PTR) return 0;
    std::lock_guard<std::mutex> lock(g_ncpuMutex);
    auto it1 = ptr_registry.find(ptr1);
    auto it2 = ptr_registry.find(ptr2);
    if (it1 == ptr_registry.end() || it2 == ptr_registry.end()) return 0;
    if (!it1->second.valid || !it2->second.valid) return 0;
    if (it1->second.size != it2->second.size) return 0;
    return memcmp(it1->second.addr, it2->second.addr, (size_t)it1->second.size) == 0 ? 1 : 0;
}

int64_t ac_ncpu_ptr_copy(int64_t ptr_id) {
    if (ptr_id == NULL_PTR) return NULL_PTR;
    std::lock_guard<std::mutex> lock(g_ncpuMutex);
    auto it = ptr_registry.find(ptr_id);
    if (it == ptr_registry.end() || !it->second.valid) return NULL_PTR;
    int64_t sz = it->second.size;
    void* dup = malloc(sz > 0 ? (size_t)sz : 1);
    if (!dup) return NULL_PTR;
    if (sz > 0) memcpy(dup, it->second.addr, (size_t)sz);
    int64_t new_id = next_ptr_id++;
    ptr_registry[new_id] = {dup, sz, it->second.type_id, true, false, true};
    return new_id;
}

int ac_ncpu_ptr_update(int64_t ptr_id, void* new_value, int64_t size) {
    if (ptr_id == NULL_PTR || !new_value || size <= 0) return -1;
    std::lock_guard<std::mutex> lock(g_ncpuMutex);
    auto it = ptr_registry.find(ptr_id);
    if (it == ptr_registry.end() || !it->second.valid) return -1;
    if (size > it->second.size) {
        if (it->second.bare_metal || !it->second.owns_memory) return -1; // can't grow these in place
        void* grown = realloc(it->second.addr, (size_t)size);
        if (!grown) return -1;
        it->second.addr = grown;
        it->second.size = size;
    }
    memcpy(it->second.addr, new_value, (size_t)size);
    return 0;
}

int ac_ncpu_ptr_free(int64_t ptr_id) {
    if (ptr_id == NULL_PTR) return 0;
    std::lock_guard<std::mutex> lock(g_ncpuMutex);
    auto it = ptr_registry.find(ptr_id);
    if (it == ptr_registry.end()) return -1;
    if (it->second.valid && it->second.addr && it->second.owns_memory) {
        if (it->second.bare_metal) munmap(it->second.addr, (size_t)it->second.size);
        else free(it->second.addr);
    }
    ptr_registry.erase(it);
    return 0;
}

// ============================================================================
// ALLOCATION — dha = dynamic heap allocation, z = zeroed, bm = bare-metal (raw mmap)
// ============================================================================

int64_t ac_ncpu_dha(int64_t size, int64_t type_id) {
    if (size <= 0) return NULL_PTR;
    std::lock_guard<std::mutex> lock(g_ncpuMutex);
    void* mem = malloc((size_t)size);
    if (!mem) return NULL_PTR;
    int64_t id = next_ptr_id++;
    ptr_registry[id] = {mem, size, type_id, true, false, true};
    return id;
}

int64_t ac_ncpu_zdha(int64_t size, int64_t type_id) {
    if (size <= 0) return NULL_PTR;
    std::lock_guard<std::mutex> lock(g_ncpuMutex);
    void* mem = calloc(1, (size_t)size);
    if (!mem) return NULL_PTR;
    int64_t id = next_ptr_id++;
    ptr_registry[id] = {mem, size, type_id, true, false, true};
    return id;
}

int64_t ac_ncpu_realloc(int64_t ptr_id, int64_t size) {
    if (size <= 0) return NULL_PTR;
    std::lock_guard<std::mutex> lock(g_ncpuMutex);
    auto it = ptr_registry.find(ptr_id);
    if (it == ptr_registry.end() || !it->second.valid || it->second.bare_metal
        || !it->second.owns_memory)
        return NULL_PTR;
    void* grown = realloc(it->second.addr, (size_t)size);
    if (!grown) return NULL_PTR;
    it->second.addr = grown;
    it->second.size = size;
    return ptr_id;
}

// Bare-metal twins: raw mmap, no libc allocator, straight to pages. Anonymous mmap pages
// are kernel-zeroed already, so bmzdha is bmdha in every way that matters.
static int64_t bmAllocImpl(int64_t size, int64_t type_id) {
    if (size <= 0) return NULL_PTR;
    void* mem = mmap(nullptr, (size_t)size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) return NULL_PTR;
    std::lock_guard<std::mutex> lock(g_ncpuMutex);
    int64_t id = next_ptr_id++;
    ptr_registry[id] = {mem, size, type_id, true, true, true};
    return id;
}
int64_t ac_ncpu_bmdha(int64_t size, int64_t type_id)  { return bmAllocImpl(size, type_id); }
int64_t ac_ncpu_bmzdha(int64_t size, int64_t type_id) { return bmAllocImpl(size, type_id); }

int64_t ac_ncpu_bmrealloc(int64_t ptr_id, int64_t size) {
    if (size <= 0) return NULL_PTR;
    std::lock_guard<std::mutex> lock(g_ncpuMutex);
    auto it = ptr_registry.find(ptr_id);
    if (it == ptr_registry.end() || !it->second.valid || !it->second.bare_metal
        || !it->second.owns_memory)
        return NULL_PTR;
#ifdef __linux__
    void* grown = mremap(it->second.addr, (size_t)it->second.size, (size_t)size, MREMAP_MAYMOVE);
    if (grown == MAP_FAILED) return NULL_PTR;
#else
    void* grown = mmap(nullptr, (size_t)size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (grown == MAP_FAILED) return NULL_PTR;
    memcpy(grown, it->second.addr, (size_t)(size < it->second.size ? size : it->second.size));
    munmap(it->second.addr, (size_t)it->second.size);
#endif
    it->second.addr = grown;
    it->second.size = size;
    return ptr_id;
}

// ============================================================================
// ARENA ALLOCATOR — bump allocator, bulk free
// ============================================================================

int64_t ac_ncpu_arena_create(int64_t size) {
    if (size <= 0) return NULL_PTR;
    void* buf = malloc((size_t)size);
    if (!buf) return NULL_PTR;
    std::lock_guard<std::mutex> lock(g_ncpuMutex);
    int64_t id = next_arena_id++;
    arena_registry[id] = {buf, size, 0};
    return id;
}

int64_t ac_ncpu_arena_alloc(int64_t arena_id, int64_t size) {
    if (size <= 0) return NULL_PTR;
    std::lock_guard<std::mutex> lock(g_ncpuMutex);
    auto ait = arena_registry.find(arena_id);
    if (ait == arena_registry.end()) return NULL_PTR;
    ArenaEntry& a = ait->second;
    if (a.offset + size > a.capacity) return NULL_PTR;  // arena exhausted
    void* slot = (char*)a.buffer + a.offset;
    a.offset += size;
    int64_t id = next_ptr_id++;
    // Non-owning: the arena's buffer owns this memory, so ptr_free/realloc must not touch it.
    ptr_registry[id] = {slot, size, 0, true, false, false};
    return id;
}

// Bump allocators don't free individual items — reclaiming happens in bulk via
// arena_abort (reset) or arena_destroy (tear down). Documented no-op, not a silent fake.
int ac_ncpu_arena_dealloc(int64_t /*arena_id*/, int64_t /*ptr_id*/) { return 0; }

int ac_ncpu_arena_destroy(int64_t arena_id) {
    std::lock_guard<std::mutex> lock(g_ncpuMutex);
    auto ait = arena_registry.find(arena_id);
    if (ait == arena_registry.end()) return -1;
    free(ait->second.buffer);
    arena_registry.erase(ait);
    return 0;
}

// Emergency: nuke the whole arena's allocations (reset the bump cursor) without freeing
// the underlying buffer, so it's immediately reusable — an O(1) bulk free.
int ac_ncpu_arena_abort(int64_t arena_id) {
    std::lock_guard<std::mutex> lock(g_ncpuMutex);
    auto ait = arena_registry.find(arena_id);
    if (ait == arena_registry.end()) return -1;
    ait->second.offset = 0;
    return 0;
}

// ============================================================================
// CONTROL
// ============================================================================

// Hard bare-metal abort — a real, immediate SIGABRT, not a clean/graceful exit.
void ac_ncpu_abort() { std::abort(); }

// ============================================================================
// MESSAGING (between quickthreads) — spelling "recieve" is intentional
// ============================================================================

void ac_ncpu_broadcast(const char* msg) {
    if (!msg) return;
    std::lock_guard<std::mutex> lock(g_ncpuMutex);
    g_ncpuMessages.insert(msg);
}

int ac_ncpu_recieve(const char* msg) {
    if (!msg) return 0;
    std::lock_guard<std::mutex> lock(g_ncpuMutex);
    auto it = g_ncpuMessages.find(msg);
    if (it == g_ncpuMessages.end()) return 0;
    g_ncpuMessages.erase(it);
    return 1;
}

// ============================================================================
// UTILITY
// ============================================================================

int64_t ac_ncpu_count() {
    std::lock_guard<std::mutex> lock(g_ncpuMutex);
    return (int64_t)ptr_registry.size();
}

const char* ac_ncpu_version() { return "libacncpu 1.1.0"; }

// ============================================================================
// BARE-NAME ALIASES — for the ASM backend specifically. Its ilib-injection only
// pulls `extern` lines out of the FFI file (no trampoline/wrapper layer like the
// other backends get), so the call sites' own dot-mangled/bare names (ncpu_dha,
// ptr_new, ...) must be REAL exported .so symbols, not just ac_ncpu_*.
// ============================================================================

int64_t ptr_new(const void* value, int64_t value_size, int64_t type_id) { return ac_ncpu_ptr_new(value, value_size, type_id); }
int64_t ptr_deref(int64_t ptr_id, void* out, int64_t out_size)          { return ac_ncpu_ptr_deref(ptr_id, out, out_size); }
int     ptr_is_null(int64_t ptr_id)                                    { return ac_ncpu_ptr_is_null(ptr_id); }
int64_t ptr_null()                                                     { return ac_ncpu_ptr_null(); }
int     ptr_eq(int64_t a, int64_t b)                                   { return ac_ncpu_ptr_eq(a, b); }
int64_t ptr_copy(int64_t ptr_id)                                       { return ac_ncpu_ptr_copy(ptr_id); }
int     ptr_update(int64_t ptr_id, void* new_value, int64_t size)      { return ac_ncpu_ptr_update(ptr_id, new_value, size); }
int     ptr_free(int64_t ptr_id)                                       { return ac_ncpu_ptr_free(ptr_id); }

int64_t ncpu_dha(int64_t size, int64_t type_id)          { return ac_ncpu_dha(size, type_id); }
int64_t ncpu_zdha(int64_t size, int64_t type_id)         { return ac_ncpu_zdha(size, type_id); }
int64_t ncpu_realloc(int64_t ptr_id, int64_t size)       { return ac_ncpu_realloc(ptr_id, size); }
int64_t ncpu_bmdha(int64_t size, int64_t type_id)        { return ac_ncpu_bmdha(size, type_id); }
int64_t ncpu_bmzdha(int64_t size, int64_t type_id)       { return ac_ncpu_bmzdha(size, type_id); }
int64_t ncpu_bmrealloc(int64_t ptr_id, int64_t size)     { return ac_ncpu_bmrealloc(ptr_id, size); }
int64_t ncpu_arena_create(int64_t size)                  { return ac_ncpu_arena_create(size); }
int64_t ncpu_arena_alloc(int64_t arena_id, int64_t size) { return ac_ncpu_arena_alloc(arena_id, size); }
int     ncpu_arena_dealloc(int64_t arena_id, int64_t ptr_id) { return ac_ncpu_arena_dealloc(arena_id, ptr_id); }
int     ncpu_arena_destroy(int64_t arena_id)             { return ac_ncpu_arena_destroy(arena_id); }
int     ncpu_arena_abort(int64_t arena_id)               { return ac_ncpu_arena_abort(arena_id); }
void    ncpu_abort()                                     { ac_ncpu_abort(); }
void    ncpu_broadcast(const char* msg)                  { ac_ncpu_broadcast(msg); }
int     ncpu_recieve(const char* msg)                    { return ac_ncpu_recieve(msg); }

}  // extern "C"
