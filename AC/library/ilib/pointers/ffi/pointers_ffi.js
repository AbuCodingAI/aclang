// AC ilib: pointers — JavaScript FFI
// Pointer storage and operations

// ============================================================================
// POINTER REGISTRY (global storage)
// ============================================================================

class PtrEntry {
    constructor(addr, typeId = 0) {
        this.addr = addr;      // actual reference/value
        this.typeId = typeId;  // type hint (0=generic)
        this.valid = true;
    }
}

const ptrRegistry = new Map();  // Map<int, PtrEntry>
let nextPtrId = 1000;            // start IDs at 1000
const NULL_PTR = -1;

// ============================================================================
// POINTER CREATION
// ============================================================================

/**
 * pt_new - Create pointer to a value
 * @param {*} value - value to store
 * @param {number} valueSize - size in bytes (for type hinting)
 * @param {number} typeId - optional type identifier (0=generic)
 * @returns {number} pointer ID, or NULL_PTR on error
 */
function ptNew(value, valueSize, typeId = 0) {
    if (value === null || value === undefined || valueSize <= 0) {
        return NULL_PTR;
    }

    try {
        const ptrId = nextPtrId++;
        ptrRegistry.set(ptrId, new PtrEntry(value, typeId));
        return ptrId;
    } catch (e) {
        return NULL_PTR;
    }
}

/**
 * pt_alloc - Allocate storage of given size
 * @param {number} valueSize - size in bytes
 * @param {number} typeId - optional type identifier
 * @returns {number} pointer ID, or NULL_PTR
 */
function ptAlloc(valueSize, typeId = 0) {
    if (valueSize <= 0) return NULL_PTR;

    try {
        const ptrId = nextPtrId++;
        const buffer = Buffer.alloc(valueSize);
        ptrRegistry.set(ptrId, new PtrEntry(buffer, typeId));
        return ptrId;
    } catch (e) {
        return NULL_PTR;
    }
}

// ============================================================================
// POINTER DEREFERENCING
// ============================================================================

/**
 * pt_get - Dereference pointer - get value
 * @param {number} ptrId - pointer ID
 * @returns {*} stored value or null
 */
function ptGet(ptrId) {
    if (!ptrRegistry.has(ptrId)) return null;
    const entry = ptrRegistry.get(ptrId);
    if (!entry.valid) return null;
    return entry.addr;
}

/**
 * pt_set - Set value at pointer location
 * @param {number} ptrId - pointer ID
 * @param {*} value - new value
 * @param {number} offset - offset in bytes
 * @returns {number} 0 on success, -1 on error
 */
function ptSet(ptrId, value, offset = 0) {
    if (!ptrRegistry.has(ptrId)) return -1;
    const entry = ptrRegistry.get(ptrId);
    if (!entry.valid) return -1;

    try {
        if (Buffer.isBuffer(entry.addr)) {
            if (typeof value === 'number') {
                entry.addr[offset] = value & 0xFF;
            } else if (Buffer.isBuffer(value)) {
                value.copy(entry.addr, offset);
            } else if (typeof value === 'string') {
                entry.addr.write(value, offset);
            }
            return 0;
        } else {
            ptrRegistry.get(ptrId).addr = value;
            return 0;
        }
    } catch (e) {
        return -1;
    }
}

// ============================================================================
// POINTER ARITHMETIC
// ============================================================================

/**
 * pt_add - Add offset to pointer
 * @param {number} ptrId - pointer ID
 * @param {number} offset - bytes to add
 * @returns {number} new pointer ID, or NULL_PTR
 */
function ptAdd(ptrId, offset) {
    if (!ptrRegistry.has(ptrId)) return NULL_PTR;
    const entry = ptrRegistry.get(ptrId);
    if (!entry.valid) return NULL_PTR;

    try {
        const newId = nextPtrId++;
        ptrRegistry.set(newId, new PtrEntry(entry.addr, entry.typeId));
        return newId;
    } catch (e) {
        return NULL_PTR;
    }
}

/**
 * pt_sub - Subtract two pointers (get distance)
 * @param {number} ptrId1 - first pointer ID
 * @param {number} ptrId2 - second pointer ID
 * @returns {number} difference in bytes
 */
function ptSub(ptrId1, ptrId2) {
    if (!ptrRegistry.has(ptrId1) || !ptrRegistry.has(ptrId2)) return 0;

    const entry1 = ptrRegistry.get(ptrId1);
    const entry2 = ptrRegistry.get(ptrId2);

    if (!entry1.valid || !entry2.valid) return 0;

    if (entry1.addr === entry2.addr) return 0;
    return 0;  // Can't compute real offset between different objects
}

// ============================================================================
// POINTER COMPARISON
// ============================================================================

/**
 * pt_eq - Check if two pointers are equal
 * @returns {number} 1 if equal, 0 otherwise
 */
function ptEq(ptrId1, ptrId2) {
    if (!ptrRegistry.has(ptrId1) || !ptrRegistry.has(ptrId2)) return 0;

    const entry1 = ptrRegistry.get(ptrId1);
    const entry2 = ptrRegistry.get(ptrId2);

    if (!entry1.valid || !entry2.valid) return 0;

    return entry1.addr === entry2.addr ? 1 : 0;
}

/**
 * pt_null - Get NULL pointer value
 */
function ptNull() {
    return NULL_PTR;
}

/**
 * pt_is_null - Check if pointer is NULL
 * @returns {number} 1 if NULL, 0 otherwise
 */
function ptIsNull(ptrId) {
    return ptrId === NULL_PTR ? 1 : 0;
}

// ============================================================================
// POINTER LIFECYCLE
// ============================================================================

/**
 * pt_free - Free/invalidate pointer
 * @param {number} ptrId - pointer ID to free
 * @returns {number} 0 on success
 */
function ptFree(ptrId) {
    if (ptrRegistry.has(ptrId)) {
        ptrRegistry.get(ptrId).valid = false;
        ptrRegistry.delete(ptrId);
        return 0;
    }
    return -1;
}

/**
 * pt_typeof - Get type ID of pointer
 * @returns {number} type_id, or -1 if invalid
 */
function ptTypeof(ptrId) {
    if (!ptrRegistry.has(ptrId)) return -1;
    const entry = ptrRegistry.get(ptrId);
    return entry.valid ? entry.typeId : -1;
}

/**
 * pt_valid - Check if pointer is valid
 * @returns {number} 1 if valid, 0 otherwise
 */
function ptValid(ptrId) {
    if (!ptrRegistry.has(ptrId)) return 0;
    return ptrRegistry.get(ptrId).valid ? 1 : 0;
}

// ============================================================================
// UTILITY
// ============================================================================

/**
 * pt_sizeof - Get size of allocated memory
 * @returns {number} size in bytes, or 0 if unknown
 */
function ptSizeof(ptrId) {
    if (!ptrRegistry.has(ptrId)) return 0;
    const entry = ptrRegistry.get(ptrId);
    if (Buffer.isBuffer(entry.addr)) {
        return entry.addr.length;
    }
    return 0;
}

/**
 * pt_clear_all - Clear entire registry (for cleanup)
 */
function ptClearAll() {
    ptrRegistry.clear();
    nextPtrId = 1000;
}

// Export for Node.js
module.exports = {
    ptNew, ptAlloc,
    ptGet, ptSet,
    ptAdd, ptSub,
    ptEq, ptNull, ptIsNull,
    ptFree, ptTypeof, ptValid,
    ptSizeof, ptClearAll
};
