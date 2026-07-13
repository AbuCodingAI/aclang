// AC ilib: pointers — Rust FFI
// Pointer storage and operations

use std::collections::HashMap;
use std::sync::{Arc, Mutex};

// ============================================================================
// POINTER REGISTRY
// ============================================================================

#[derive(Clone)]
pub struct PtrEntry {
    pub addr: Arc<dyn std::any::Any + Send + Sync>,
    pub type_id: i64,
    pub valid: bool,
}

lazy_static::lazy_static! {
    static ref PTR_REGISTRY: Mutex<HashMap<i64, PtrEntry>> = Mutex::new(HashMap::new());
    static ref NEXT_PTR_ID: Mutex<i64> = Mutex::new(1000);
}

pub const NULL_PTR: i64 = -1;

// ============================================================================
// POINTER CREATION
// ============================================================================

/// pt_new - Create pointer to a value
pub fn pt_new(value: *mut u8, value_size: i64, type_id: i64) -> i64 {
    if value.is_null() || value_size <= 0 {
        return NULL_PTR;
    }

    let mut registry = PTR_REGISTRY.lock().unwrap();
    let mut next_id = NEXT_PTR_ID.lock().unwrap();
    let ptr_id = *next_id;
    *next_id += 1;

    // Store as boxed bytes
    let data = unsafe { Vec::from_raw_parts(value, value_size as usize, value_size as usize) };

    registry.insert(
        ptr_id,
        PtrEntry {
            addr: Arc::new(data),
            type_id,
            valid: true,
        },
    );

    ptr_id
}

/// pt_alloc - Allocate storage of given size
pub fn pt_alloc(value_size: i64, type_id: i64) -> i64 {
    if value_size <= 0 {
        return NULL_PTR;
    }

    let mut registry = PTR_REGISTRY.lock().unwrap();
    let mut next_id = NEXT_PTR_ID.lock().unwrap();
    let ptr_id = *next_id;
    *next_id += 1;

    let data: Vec<u8> = vec![0; value_size as usize];

    registry.insert(
        ptr_id,
        PtrEntry {
            addr: Arc::new(data),
            type_id,
            valid: true,
        },
    );

    ptr_id
}

// ============================================================================
// POINTER DEREFERENCING
// ============================================================================

/// pt_get - Dereference pointer - get value
pub fn pt_get(ptr_id: i64) -> *mut u8 {
    let registry = PTR_REGISTRY.lock().unwrap();

    if let Some(entry) = registry.get(&ptr_id) {
        if entry.valid {
            if let Some(data) = entry.addr.downcast_ref::<Vec<u8>>() {
                return data.as_ptr() as *mut u8;
            }
        }
    }

    std::ptr::null_mut()
}

/// pt_set - Set value at pointer location
pub fn pt_set(ptr_id: i64, value: *mut u8, offset: i64) -> i32 {
    let mut registry = PTR_REGISTRY.lock().unwrap();

    if let Some(entry) = registry.get_mut(&ptr_id) {
        if entry.valid {
            if let Some(data) = entry.addr.downcast_ref::<Vec<u8>>() {
                if offset >= 0 && offset < data.len() as i64 && !value.is_null() {
                    unsafe {
                        *(data.as_ptr() as *mut u8).add(offset as usize) = *value;
                    }
                    return 0;
                }
            }
        }
    }

    -1
}

// ============================================================================
// POINTER ARITHMETIC
// ============================================================================

/// pt_add - Add offset to pointer
pub fn pt_add(ptr_id: i64, offset: i64) -> i64 {
    let registry = PTR_REGISTRY.lock().unwrap();

    if let Some(entry) = registry.get(&ptr_id) {
        if entry.valid {
            let mut next_id = NEXT_PTR_ID.lock().unwrap();
            let new_id = *next_id;
            *next_id += 1;

            drop(next_id);

            let mut new_registry = PTR_REGISTRY.lock().unwrap();
            new_registry.insert(new_id, entry.clone());

            return new_id;
        }
    }

    NULL_PTR
}

/// pt_sub - Subtract two pointers
pub fn pt_sub(ptr_id1: i64, ptr_id2: i64) -> i64 {
    let registry = PTR_REGISTRY.lock().unwrap();

    if let (Some(entry1), Some(entry2)) = (registry.get(&ptr_id1), registry.get(&ptr_id2)) {
        if entry1.valid && entry2.valid {
            // Can't compute real offset between different objects
            return 0;
        }
    }

    0
}

// ============================================================================
// POINTER COMPARISON
// ============================================================================

/// pt_eq - Check if two pointers are equal
pub fn pt_eq(ptr_id1: i64, ptr_id2: i64) -> i32 {
    let registry = PTR_REGISTRY.lock().unwrap();

    if let (Some(entry1), Some(entry2)) = (registry.get(&ptr_id1), registry.get(&ptr_id2)) {
        if entry1.valid && entry2.valid {
            if Arc::ptr_eq(&entry1.addr, &entry2.addr) {
                return 1;
            }
        }
    }

    0
}

/// pt_null - Get NULL pointer
pub fn pt_null() -> i64 {
    NULL_PTR
}

/// pt_is_null - Check if pointer is NULL
pub fn pt_is_null(ptr_id: i64) -> i32 {
    if ptr_id == NULL_PTR {
        1
    } else {
        0
    }
}

// ============================================================================
// POINTER LIFECYCLE
// ============================================================================

/// pt_free - Free/invalidate pointer
pub fn pt_free(ptr_id: i64) -> i32 {
    let mut registry = PTR_REGISTRY.lock().unwrap();

    if let Some(entry) = registry.get_mut(&ptr_id) {
        entry.valid = false;
        registry.remove(&ptr_id);
        return 0;
    }

    -1
}

/// pt_typeof - Get type ID of pointer
pub fn pt_typeof(ptr_id: i64) -> i64 {
    let registry = PTR_REGISTRY.lock().unwrap();

    if let Some(entry) = registry.get(&ptr_id) {
        if entry.valid {
            return entry.type_id;
        }
    }

    -1
}

/// pt_valid - Check if pointer is valid
pub fn pt_valid(ptr_id: i64) -> i32 {
    let registry = PTR_REGISTRY.lock().unwrap();

    if let Some(entry) = registry.get(&ptr_id) {
        if entry.valid {
            return 1;
        }
    }

    0
}

// ============================================================================
// UTILITY
// ============================================================================

/// pt_sizeof - Get size of allocated memory
pub fn pt_sizeof(ptr_id: i64) -> i64 {
    let registry = PTR_REGISTRY.lock().unwrap();

    if let Some(entry) = registry.get(&ptr_id) {
        if let Some(data) = entry.addr.downcast_ref::<Vec<u8>>() {
            return data.len() as i64;
        }
    }

    0
}

/// pt_clear_all - Clear entire registry
pub fn pt_clear_all() {
    let mut registry = PTR_REGISTRY.lock().unwrap();
    registry.clear();

    let mut next_id = NEXT_PTR_ID.lock().unwrap();
    *next_id = 1000;
}
