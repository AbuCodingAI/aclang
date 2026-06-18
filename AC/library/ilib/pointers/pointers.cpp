// AC ilib: pointers — C++ shared library implementation
// Pointer storage and operations for AC/AI
// Build: g++ -O2 -fPIC -shared pointers.cpp -o libacpointers.so

#include <unordered_map>
#include <cstring>
#include <cstdint>
#include <cstdlib>

// ============================================================================
// POINTER REGISTRY (global storage)
// ============================================================================

struct PtrEntry {
    void* addr;      // actual pointer/address
    int64_t type_id; // type hint (0=generic)
    bool valid;      // is this entry valid?
};

static std::unordered_map<int64_t, PtrEntry> ptr_registry;
static int64_t next_ptr_id = 1000;  // start IDs at 1000 to avoid confusion with addresses
static const int64_t NULL_PTR = -1;

extern "C" {

// ============================================================================
// POINTER CREATION
// ============================================================================

/* Create pointer to a value
   Args:
     value: pointer to value to store
     value_size: size of value in bytes
     type_id: optional type identifier (0 = generic)
   Returns:
     pointer ID (handle), or NULL_PTR on error
*/
int64_t pt_new(void* value, int64_t value_size, int64_t type_id) {
    if (!value || value_size <= 0) return NULL_PTR;

    try {
        // Allocate storage for the value
        void* stored = malloc(value_size);
        if (!stored) return NULL_PTR;

        // Copy value into storage
        memcpy(stored, value, value_size);

        // Store in registry
        int64_t id = next_ptr_id++;
        ptr_registry[id] = {stored, type_id, true};
        return id;
    } catch (...) {
        return NULL_PTR;
    }
}

/* Create pointer with automatic storage allocation
   Args:
     value_size: how many bytes to allocate
   Returns:
     pointer ID to newly allocated (zeroed) memory
*/
int64_t pt_new_alloc(int64_t value_size, int64_t type_id) {
    if (value_size <= 0) return NULL_PTR;

    try {
        void* mem = calloc(1, value_size);  // allocate and zero
        if (!mem) return NULL_PTR;

        int64_t id = next_ptr_id++;
        ptr_registry[id] = {mem, type_id, true};
        return id;
    } catch (...) {
        return NULL_PTR;
    }
}

// ============================================================================
// POINTER DEREFERENCE
// ============================================================================

/* Get value from pointer
   Args:
     ptr_id: pointer ID
     out: output buffer
     out_size: size of output buffer
   Returns:
     number of bytes copied, or -1 on error
*/
int64_t pt_deref(int64_t ptr_id, void* out, int64_t out_size) {
    if (!out || out_size <= 0) return -1;

    try {
        auto it = ptr_registry.find(ptr_id);
        if (it == ptr_registry.end() || !it->second.valid) return -1;

        // Copy from storage to output buffer
        // (size unknown at deref time, assume full copy requested)
        memcpy(out, it->second.addr, out_size);
        return out_size;
    } catch (...) {
        return -1;
    }
}

/* Get raw pointer address (for C interop)
   Returns:
     actual void* address (dangerous but sometimes needed)
*/
void* pt_get_addr(int64_t ptr_id) {
    try {
        auto it = ptr_registry.find(ptr_id);
        if (it == ptr_registry.end() || !it->second.valid) return nullptr;
        return it->second.addr;
    } catch (...) {
        return nullptr;
    }
}

// ============================================================================
// POINTER OPERATIONS
// ============================================================================

/* Check if pointer is null
   Returns:
     1 if null, 0 if valid
*/
int pt_is_null(int64_t ptr_id) {
    return ptr_id == NULL_PTR ? 1 : 0;
}

/* Get null pointer
   Returns:
     NULL_PTR (-1)
*/
int64_t pt_null() {
    return NULL_PTR;
}

/* Compare two pointers
   Returns:
     1 if they point to same location, 0 otherwise
*/
int pt_eq(int64_t ptr1, int64_t ptr2) {
    if (ptr1 == NULL_PTR || ptr2 == NULL_PTR) return ptr1 == ptr2 ? 1 : 0;

    try {
        auto it1 = ptr_registry.find(ptr1);
        auto it2 = ptr_registry.find(ptr2);

        if (it1 == ptr_registry.end() || it2 == ptr_registry.end()) return 0;
        return it1->second.addr == it2->second.addr ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

// ============================================================================
// POINTER COPY
// ============================================================================

/* Copy pointer (creates new handle to same data)
   Returns:
     new pointer ID pointing to same data
*/
int64_t pt_copy(int64_t ptr_id) {
    if (ptr_id == NULL_PTR) return NULL_PTR;

    try {
        auto it = ptr_registry.find(ptr_id);
        if (it == ptr_registry.end() || !it->second.valid) return NULL_PTR;

        // Create new ID pointing to same address
        int64_t new_id = next_ptr_id++;
        ptr_registry[new_id] = it->second;  // copy the entry
        return new_id;
    } catch (...) {
        return NULL_PTR;
    }
}

// ============================================================================
// POINTER UPDATE
// ============================================================================

/* Update value at pointer location
   Args:
     ptr_id: pointer ID
     new_value: new data to write
     size: size of new_value
   Returns:
     0 on success, -1 on error
*/
int pt_update(int64_t ptr_id, void* new_value, int64_t size) {
    if (ptr_id == NULL_PTR || !new_value || size <= 0) return -1;

    try {
        auto it = ptr_registry.find(ptr_id);
        if (it == ptr_registry.end() || !it->second.valid) return -1;

        memcpy(it->second.addr, new_value, size);
        return 0;
    } catch (...) {
        return -1;
    }
}

// ============================================================================
// POINTER CLEANUP
// ============================================================================

/* Free pointer and release memory
   Returns:
     0 on success, -1 on error
*/
int pt_free(int64_t ptr_id) {
    if (ptr_id == NULL_PTR) return 0;  // freeing null is ok

    try {
        auto it = ptr_registry.find(ptr_id);
        if (it == ptr_registry.end()) return -1;

        if (it->second.valid && it->second.addr) {
            free(it->second.addr);
            it->second.valid = false;
        }
        ptr_registry.erase(it);
        return 0;
    } catch (...) {
        return -1;
    }
}

/* Free all pointers (cleanup on exit)
   Returns:
     number of pointers freed
*/
int pt_cleanup_all() {
    int count = 0;
    for (auto& entry : ptr_registry) {
        if (entry.second.valid && entry.second.addr) {
            free(entry.second.addr);
            entry.second.valid = false;
            count++;
        }
    }
    ptr_registry.clear();
    return count;
}

// ============================================================================
// UTILITY
// ============================================================================

/* Get number of active pointers
   Returns:
     count of pointers in registry
*/
int64_t pt_count() {
    return ptr_registry.size();
}

/* Library version
   Returns:
     version string
*/
const char* pt_version() {
    return "libacpointers 0.1.0";
}

}  // extern "C"
