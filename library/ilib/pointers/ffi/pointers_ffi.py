# AC ilib: pointers — Python FFI
# Pointer storage and operations
import ctypes
import sys

# ============================================================================
# POINTER REGISTRY (global storage)
# ============================================================================

class PtrEntry:
    def __init__(self, addr, type_id=0):
        self.addr = addr  # actual memory address or object reference
        self.type_id = type_id  # type hint (0=generic)
        self.valid = True

ptr_registry = {}  # int64_id -> PtrEntry
next_ptr_id = 1000  # start IDs at 1000 to avoid confusion with addresses
NULL_PTR = -1

# ============================================================================
# POINTER CREATION
# ============================================================================

def pt_new(value, value_size, type_id=0):
    """Create pointer to a value
    Args:
        value: pointer/reference to value
        value_size: size in bytes (for type hinting)
        type_id: optional type identifier (0=generic)
    Returns:
        pointer ID (handle), or NULL_PTR on error
    """
    global next_ptr_id

    if value is None or value_size <= 0:
        return NULL_PTR

    try:
        # Store reference
        ptr_id = next_ptr_id
        next_ptr_id += 1
        ptr_registry[ptr_id] = PtrEntry(value, type_id)
        return ptr_id
    except:
        return NULL_PTR

def pt_alloc(value_size, type_id=0):
    """Allocate storage of given size
    Args:
        value_size: size in bytes
        type_id: optional type identifier
    Returns:
        pointer ID, or NULL_PTR on error
    """
    global next_ptr_id

    if value_size <= 0:
        return NULL_PTR

    try:
        # Allocate bytearray
        allocated = bytearray(value_size)
        ptr_id = next_ptr_id
        next_ptr_id += 1
        ptr_registry[ptr_id] = PtrEntry(allocated, type_id)
        return ptr_id
    except:
        return NULL_PTR

# ============================================================================
# POINTER DEREFERENCING
# ============================================================================

def pt_get(ptr_id):
    """Dereference pointer - get value
    Args:
        ptr_id: pointer ID
    Returns:
        stored value or None
    """
    if ptr_id not in ptr_registry:
        return None
    entry = ptr_registry[ptr_id]
    if not entry.valid:
        return None
    return entry.addr

def pt_set(ptr_id, value, offset=0):
    """Set value at pointer location
    Args:
        ptr_id: pointer ID
        value: new value
        offset: offset in bytes (for arrays)
    Returns:
        0 on success, -1 on error
    """
    if ptr_id not in ptr_registry:
        return -1
    entry = ptr_registry[ptr_id]
    if not entry.valid:
        return -1

    try:
        if isinstance(entry.addr, bytearray):
            if isinstance(value, int):
                if 0 <= offset < len(entry.addr):
                    entry.addr[offset] = value & 0xFF
            elif isinstance(value, bytes):
                entry.addr[offset:offset+len(value)] = value
            return 0
        else:
            # Direct reference assignment
            ptr_registry[ptr_id].addr = value
            return 0
    except:
        return -1

# ============================================================================
# POINTER ARITHMETIC
# ============================================================================

def pt_add(ptr_id, offset):
    """Add offset to pointer (array indexing)
    Args:
        ptr_id: pointer ID
        offset: bytes to add
    Returns:
        new pointer ID, or NULL_PTR
    """
    global next_ptr_id

    if ptr_id not in ptr_registry:
        return NULL_PTR
    entry = ptr_registry[ptr_id]
    if not entry.valid:
        return NULL_PTR

    try:
        if isinstance(entry.addr, bytearray):
            new_id = next_ptr_id
            next_ptr_id += 1
            # Create "view" at offset
            ptr_registry[new_id] = PtrEntry(entry.addr, entry.type_id)
            return new_id
        return NULL_PTR
    except:
        return NULL_PTR

def pt_sub(ptr_id1, ptr_id2):
    """Subtract two pointers (get distance)
    Args:
        ptr_id1, ptr_id2: pointer IDs
    Returns:
        difference in bytes
    """
    if ptr_id1 not in ptr_registry or ptr_id2 not in ptr_registry:
        return 0

    entry1 = ptr_registry[ptr_id1]
    entry2 = ptr_registry[ptr_id2]

    if not entry1.valid or not entry2.valid:
        return 0

    # If same object, offset is 0
    if entry1.addr is entry2.addr:
        return 0

    return 0  # Can't compute real offset between different objects

# ============================================================================
# POINTER COMPARISON
# ============================================================================

def pt_eq(ptr_id1, ptr_id2):
    """Check if two pointers are equal
    Returns: 1 if equal, 0 otherwise
    """
    if ptr_id1 not in ptr_registry or ptr_id2 not in ptr_registry:
        return 0

    entry1 = ptr_registry[ptr_id1]
    entry2 = ptr_registry[ptr_id2]

    if not entry1.valid or not entry2.valid:
        return 0

    return 1 if entry1.addr is entry2.addr else 0

def pt_null():
    """Get NULL pointer value"""
    return NULL_PTR

def pt_is_null(ptr_id):
    """Check if pointer is NULL
    Returns: 1 if NULL, 0 otherwise
    """
    return 1 if ptr_id == NULL_PTR else 0

# ============================================================================
# POINTER LIFECYCLE
# ============================================================================

def pt_free(ptr_id):
    """Free/invalidate pointer
    Args:
        ptr_id: pointer ID to free
    Returns:
        0 on success
    """
    if ptr_id in ptr_registry:
        ptr_registry[ptr_id].valid = False
        del ptr_registry[ptr_id]
        return 0
    return -1

def pt_typeof(ptr_id):
    """Get type ID of pointer
    Returns: type_id, or -1 if invalid
    """
    if ptr_id not in ptr_registry:
        return -1
    entry = ptr_registry[ptr_id]
    if not entry.valid:
        return -1
    return entry.type_id

def pt_valid(ptr_id):
    """Check if pointer is valid
    Returns: 1 if valid, 0 otherwise
    """
    if ptr_id not in ptr_registry:
        return 0
    return 1 if ptr_registry[ptr_id].valid else 0

# ============================================================================
# UTILITY
# ============================================================================

def pt_sizeof(ptr_id):
    """Get size of allocated memory
    Returns: size in bytes, or 0 if unknown
    """
    if ptr_id not in ptr_registry:
        return 0
    entry = ptr_registry[ptr_id]
    if isinstance(entry.addr, bytearray):
        return len(entry.addr)
    return 0

def pt_clear_all():
    """Clear entire registry (for cleanup)"""
    global ptr_registry, next_ptr_id
    ptr_registry.clear()
    next_ptr_id = 1000
