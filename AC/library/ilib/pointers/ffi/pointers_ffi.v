module pointers

import sync

// ============================================================================
// POINTER REGISTRY
// ============================================================================

struct PtrEntry {
pub mut:
    addr    voidptr  // actual pointer/address
    type_id i64     // type hint (0=generic)
    valid   bool    // is valid?
    data    []u8    // allocated buffer
}

__global (
    ptr_registry = map[i64]PtrEntry{}
    next_ptr_id = i64(1000)
    registry_mutex = sync.new_mutex()
)

pub const NULL_PTR = i64(-1)

// ============================================================================
// POINTER CREATION
// ============================================================================

// pt_new - Create pointer to a value
pub fn pt_new(value voidptr, value_size i64, type_id i64) i64 {
    if value == unsafe { nil } || value_size <= 0 {
        return NULL_PTR
    }

    registry_mutex.lock()
    defer { registry_mutex.unlock() }

    ptr_id := next_ptr_id
    next_ptr_id += 1

    ptr_registry[ptr_id] = PtrEntry{
        addr: value
        type_id: type_id
        valid: true
    }

    return ptr_id
}

// pt_alloc - Allocate storage of given size
pub fn pt_alloc(value_size i64, type_id i64) i64 {
    if value_size <= 0 {
        return NULL_PTR
    }

    registry_mutex.lock()
    defer { registry_mutex.unlock() }

    ptr_id := next_ptr_id
    next_ptr_id += 1

    data := []u8{len: int(value_size)}

    ptr_registry[ptr_id] = PtrEntry{
        addr: data.data
        type_id: type_id
        valid: true
        data: data
    }

    return ptr_id
}

// ============================================================================
// POINTER DEREFERENCING
// ============================================================================

// pt_get - Dereference pointer - get value
pub fn pt_get(ptr_id i64) voidptr {
    registry_mutex.lock()
    defer { registry_mutex.unlock() }

    if ptr_id in ptr_registry {
        entry := ptr_registry[ptr_id]
        if entry.valid {
            return entry.addr
        }
    }

    return unsafe { nil }
}

// pt_set - Set value at pointer location
pub fn pt_set(ptr_id i64, value voidptr, offset i64) int {
    registry_mutex.lock()
    defer { registry_mutex.unlock() }

    if ptr_id !in ptr_registry {
        return -1
    }

    mut entry := ptr_registry[ptr_id]
    if !entry.valid {
        return -1
    }

    if entry.data.len > 0 && offset >= 0 && offset < entry.data.len {
        unsafe {
            ptr := entry.addr as &u8
            ptr[offset] = value as u8
        }
        return 0
    }

    return -1
}

// ============================================================================
// POINTER ARITHMETIC
// ============================================================================

// pt_add - Add offset to pointer
pub fn pt_add(ptr_id i64, offset i64) i64 {
    registry_mutex.lock()
    defer { registry_mutex.unlock() }

    if ptr_id !in ptr_registry {
        return NULL_PTR
    }

    entry := ptr_registry[ptr_id]
    if !entry.valid {
        return NULL_PTR
    }

    new_id := next_ptr_id
    next_ptr_id += 1

    ptr_registry[new_id] = entry

    return new_id
}

// pt_sub - Subtract two pointers
pub fn pt_sub(ptr_id1 i64, ptr_id2 i64) i64 {
    registry_mutex.lock()
    defer { registry_mutex.unlock() }

    if ptr_id1 !in ptr_registry || ptr_id2 !in ptr_registry {
        return 0
    }

    entry1 := ptr_registry[ptr_id1]
    entry2 := ptr_registry[ptr_id2]

    if !entry1.valid || !entry2.valid {
        return 0
    }

    if entry1.addr == entry2.addr {
        return 0
    }

    return 0
}

// ============================================================================
// POINTER COMPARISON
// ============================================================================

// pt_eq - Check if two pointers are equal
pub fn pt_eq(ptr_id1 i64, ptr_id2 i64) int {
    registry_mutex.lock()
    defer { registry_mutex.unlock() }

    if ptr_id1 !in ptr_registry || ptr_id2 !in ptr_registry {
        return 0
    }

    entry1 := ptr_registry[ptr_id1]
    entry2 := ptr_registry[ptr_id2]

    if !entry1.valid || !entry2.valid {
        return 0
    }

    return if entry1.addr == entry2.addr { 1 } else { 0 }
}

// pt_null - Get NULL pointer value
pub fn pt_null() i64 {
    return NULL_PTR
}

// pt_is_null - Check if pointer is NULL
pub fn pt_is_null(ptr_id i64) int {
    return if ptr_id == NULL_PTR { 1 } else { 0 }
}

// ============================================================================
// POINTER LIFECYCLE
// ============================================================================

// pt_free - Free/invalidate pointer
pub fn pt_free(ptr_id i64) int {
    registry_mutex.lock()
    defer { registry_mutex.unlock() }

    if ptr_id in ptr_registry {
        mut entry := ptr_registry[ptr_id]
        entry.valid = false
        ptr_registry.delete(ptr_id)
        return 0
    }

    return -1
}

// pt_typeof - Get type ID of pointer
pub fn pt_typeof(ptr_id i64) i64 {
    registry_mutex.lock()
    defer { registry_mutex.unlock() }

    if ptr_id !in ptr_registry {
        return -1
    }

    entry := ptr_registry[ptr_id]
    return if entry.valid { entry.type_id } else { -1 }
}

// pt_valid - Check if pointer is valid
pub fn pt_valid(ptr_id i64) int {
    registry_mutex.lock()
    defer { registry_mutex.unlock() }

    if ptr_id !in ptr_registry {
        return 0
    }

    return if ptr_registry[ptr_id].valid { 1 } else { 0 }
}

// ============================================================================
// UTILITY
// ============================================================================

// pt_sizeof - Get size of allocated memory
pub fn pt_sizeof(ptr_id i64) i64 {
    registry_mutex.lock()
    defer { registry_mutex.unlock() }

    if ptr_id !in ptr_registry {
        return 0
    }

    entry := ptr_registry[ptr_id]
    return entry.data.len
}

// pt_clear_all - Clear entire registry
pub fn pt_clear_all() {
    registry_mutex.lock()
    defer { registry_mutex.unlock() }

    ptr_registry.clear()
    next_ptr_id = 1000
}
