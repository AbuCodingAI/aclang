// AC ilib: pointers — Go FFI
// Pointer storage and operations

package pointers

import (
	"sync"
)

// ============================================================================
// POINTER REGISTRY
// ============================================================================

type PtrEntry struct {
	addr    interface{} // actual reference/value
	typeID  int64       // type hint (0=generic)
	valid   bool        // is valid?
	data    []byte      // allocated buffer (if applicable)
}

var (
	ptrRegistry = make(map[int64]*PtrEntry)
	nextPtrID   int64 = 1000
	registryMu  sync.RWMutex
	NULL_PTR    int64 = -1
)

// ============================================================================
// POINTER CREATION
// ============================================================================

// PtNew - Create pointer to a value
func PtNew(value interface{}, valueSize int64, typeID int64) int64 {
	if value == nil || valueSize <= 0 {
		return NULL_PTR
	}

	registryMu.Lock()
	defer registryMu.Unlock()

	ptrID := nextPtrID
	nextPtrID++
	ptrRegistry[ptrID] = &PtrEntry{
		addr:   value,
		typeID: typeID,
		valid:  true,
	}
	return ptrID
}

// PtAlloc - Allocate storage of given size
func PtAlloc(valueSize int64, typeID int64) int64 {
	if valueSize <= 0 {
		return NULL_PTR
	}

	registryMu.Lock()
	defer registryMu.Unlock()

	ptrID := nextPtrID
	nextPtrID++
	ptrRegistry[ptrID] = &PtrEntry{
		addr:   make([]byte, valueSize),
		typeID: typeID,
		valid:  true,
		data:   make([]byte, valueSize),
	}
	return ptrID
}

// ============================================================================
// POINTER DEREFERENCING
// ============================================================================

// PtGet - Dereference pointer - get value
func PtGet(ptrID int64) interface{} {
	registryMu.RLock()
	defer registryMu.RUnlock()

	entry, ok := ptrRegistry[ptrID]
	if !ok || !entry.valid {
		return nil
	}
	return entry.addr
}

// PtSet - Set value at pointer location
func PtSet(ptrID int64, value interface{}, offset int64) int {
	registryMu.Lock()
	defer registryMu.Unlock()

	entry, ok := ptrRegistry[ptrID]
	if !ok || !entry.valid {
		return -1
	}

	if data, isBytes := entry.addr.([]byte); isBytes {
		if num, isInt := value.(int); isInt && offset >= 0 && offset < int64(len(data)) {
			data[offset] = byte(num & 0xFF)
		} else if bytes, isByte := value.([]byte); isByte {
			copy(data[offset:], bytes)
		}
		return 0
	}

	entry.addr = value
	return 0
}

// ============================================================================
// POINTER ARITHMETIC
// ============================================================================

// PtAdd - Add offset to pointer
func PtAdd(ptrID int64, offset int64) int64 {
	registryMu.Lock()
	defer registryMu.Unlock()

	entry, ok := ptrRegistry[ptrID]
	if !ok || !entry.valid {
		return NULL_PTR
	}

	newID := nextPtrID
	nextPtrID++
	ptrRegistry[newID] = &PtrEntry{
		addr:   entry.addr,
		typeID: entry.typeID,
		valid:  true,
	}
	return newID
}

// PtSub - Subtract two pointers (get distance)
func PtSub(ptrID1, ptrID2 int64) int64 {
	registryMu.RLock()
	defer registryMu.RUnlock()

	entry1, ok1 := ptrRegistry[ptrID1]
	entry2, ok2 := ptrRegistry[ptrID2]

	if !ok1 || !ok2 || !entry1.valid || !entry2.valid {
		return 0
	}

	return 0 // Can't compute real offset between different objects
}

// ============================================================================
// POINTER COMPARISON
// ============================================================================

// PtEq - Check if two pointers are equal
func PtEq(ptrID1, ptrID2 int64) int {
	registryMu.RLock()
	defer registryMu.RUnlock()

	entry1, ok1 := ptrRegistry[ptrID1]
	entry2, ok2 := ptrRegistry[ptrID2]

	if !ok1 || !ok2 || !entry1.valid || !entry2.valid {
		return 0
	}

	if entry1.addr == entry2.addr {
		return 1
	}
	return 0
}

// PtNull - Get NULL pointer value
func PtNull() int64 {
	return NULL_PTR
}

// PtIsNull - Check if pointer is NULL
func PtIsNull(ptrID int64) int {
	if ptrID == NULL_PTR {
		return 1
	}
	return 0
}

// ============================================================================
// POINTER LIFECYCLE
// ============================================================================

// PtFree - Free/invalidate pointer
func PtFree(ptrID int64) int {
	registryMu.Lock()
	defer registryMu.Unlock()

	entry, ok := ptrRegistry[ptrID]
	if ok {
		entry.valid = false
		delete(ptrRegistry, ptrID)
		return 0
	}
	return -1
}

// PtTypeof - Get type ID of pointer
func PtTypeof(ptrID int64) int64 {
	registryMu.RLock()
	defer registryMu.RUnlock()

	entry, ok := ptrRegistry[ptrID]
	if !ok {
		return -1
	}
	if !entry.valid {
		return -1
	}
	return entry.typeID
}

// PtValid - Check if pointer is valid
func PtValid(ptrID int64) int {
	registryMu.RLock()
	defer registryMu.RUnlock()

	entry, ok := ptrRegistry[ptrID]
	if !ok {
		return 0
	}
	if entry.valid {
		return 1
	}
	return 0
}

// ============================================================================
// UTILITY
// ============================================================================

// PtSizeof - Get size of allocated memory
func PtSizeof(ptrID int64) int64 {
	registryMu.RLock()
	defer registryMu.RUnlock()

	entry, ok := ptrRegistry[ptrID]
	if !ok {
		return 0
	}

	if data, isBytes := entry.addr.([]byte); isBytes {
		return int64(len(data))
	}
	return 0
}

// PtClearAll - Clear entire registry (for cleanup)
func PtClearAll() {
	registryMu.Lock()
	defer registryMu.Unlock()

	ptrRegistry = make(map[int64]*PtrEntry)
	nextPtrID = 1000
}
