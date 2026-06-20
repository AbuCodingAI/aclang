// AC ilib: pointers — Java FFI
// Pointer storage and operations

import java.util.*;
import java.nio.ByteBuffer;

public class Pointers {

    // ========================================================================
    // POINTER REGISTRY
    // ========================================================================

    static class PtrEntry {
        Object addr;      // actual reference/value
        long typeId;      // type hint (0=generic)
        boolean valid;    // is valid?

        PtrEntry(Object addr, long typeId) {
            this.addr = addr;
            this.typeId = typeId;
            this.valid = true;
        }
    }

    private static Map<Long, PtrEntry> ptrRegistry = new HashMap<>();
    private static long nextPtrId = 1000;
    private static final long NULL_PTR = -1;
    private static final Object registryLock = new Object();

    // ========================================================================
    // POINTER CREATION
    // ========================================================================

    /**
     * ptNew - Create pointer to a value
     */
    public static long ptNew(Object value, long valueSize, long typeId) {
        if (value == null || valueSize <= 0) {
            return NULL_PTR;
        }

        synchronized (registryLock) {
            long ptrId = nextPtrId++;
            ptrRegistry.put(ptrId, new PtrEntry(value, typeId));
            return ptrId;
        }
    }

    /**
     * ptAlloc - Allocate storage of given size
     */
    public static long ptAlloc(long valueSize, long typeId) {
        if (valueSize <= 0) return NULL_PTR;

        synchronized (registryLock) {
            long ptrId = nextPtrId++;
            byte[] data = new byte[(int) valueSize];
            ptrRegistry.put(ptrId, new PtrEntry(data, typeId));
            return ptrId;
        }
    }

    // ========================================================================
    // POINTER DEREFERENCING
    // ========================================================================

    /**
     * ptGet - Dereference pointer - get value
     */
    public static Object ptGet(long ptrId) {
        synchronized (registryLock) {
            PtrEntry entry = ptrRegistry.get(ptrId);
            if (entry != null && entry.valid) {
                return entry.addr;
            }
        }
        return null;
    }

    /**
     * ptSet - Set value at pointer location
     */
    public static int ptSet(long ptrId, Object value, long offset) {
        synchronized (registryLock) {
            PtrEntry entry = ptrRegistry.get(ptrId);
            if (entry == null || !entry.valid) {
                return -1;
            }

            if (entry.addr instanceof byte[]) {
                byte[] data = (byte[]) entry.addr;
                if (value instanceof Integer) {
                    int idx = (int) offset;
                    if (idx >= 0 && idx < data.length) {
                        data[idx] = (byte) (((Integer) value) & 0xFF);
                    }
                } else if (value instanceof byte[]) {
                    byte[] src = (byte[]) value;
                    System.arraycopy(src, 0, data, (int) offset, Math.min(src.length, data.length - (int) offset));
                } else if (value instanceof String) {
                    byte[] src = ((String) value).getBytes();
                    System.arraycopy(src, 0, data, (int) offset, Math.min(src.length, data.length - (int) offset));
                }
                return 0;
            }

            entry.addr = value;
            return 0;
        }
    }

    // ========================================================================
    // POINTER ARITHMETIC
    // ========================================================================

    /**
     * ptAdd - Add offset to pointer
     */
    public static long ptAdd(long ptrId, long offset) {
        synchronized (registryLock) {
            PtrEntry entry = ptrRegistry.get(ptrId);
            if (entry == null || !entry.valid) {
                return NULL_PTR;
            }

            long newId = nextPtrId++;
            ptrRegistry.put(newId, new PtrEntry(entry.addr, entry.typeId));
            return newId;
        }
    }

    /**
     * ptSub - Subtract two pointers (get distance)
     */
    public static long ptSub(long ptrId1, long ptrId2) {
        synchronized (registryLock) {
            PtrEntry entry1 = ptrRegistry.get(ptrId1);
            PtrEntry entry2 = ptrRegistry.get(ptrId2);

            if (entry1 == null || entry2 == null || !entry1.valid || !entry2.valid) {
                return 0;
            }

            return 0;  // Can't compute real offset
        }
    }

    // ========================================================================
    // POINTER COMPARISON
    // ========================================================================

    /**
     * ptEq - Check if two pointers are equal
     */
    public static int ptEq(long ptrId1, long ptrId2) {
        synchronized (registryLock) {
            PtrEntry entry1 = ptrRegistry.get(ptrId1);
            PtrEntry entry2 = ptrRegistry.get(ptrId2);

            if (entry1 == null || entry2 == null || !entry1.valid || !entry2.valid) {
                return 0;
            }

            return entry1.addr == entry2.addr ? 1 : 0;
        }
    }

    /**
     * ptNull - Get NULL pointer value
     */
    public static long ptNull() {
        return NULL_PTR;
    }

    /**
     * ptIsNull - Check if pointer is NULL
     */
    public static int ptIsNull(long ptrId) {
        return ptrId == NULL_PTR ? 1 : 0;
    }

    // ========================================================================
    // POINTER LIFECYCLE
    // ========================================================================

    /**
     * ptFree - Free/invalidate pointer
     */
    public static int ptFree(long ptrId) {
        synchronized (registryLock) {
            PtrEntry entry = ptrRegistry.get(ptrId);
            if (entry != null) {
                entry.valid = false;
                ptrRegistry.remove(ptrId);
                return 0;
            }
        }
        return -1;
    }

    /**
     * ptTypeof - Get type ID of pointer
     */
    public static long ptTypeof(long ptrId) {
        synchronized (registryLock) {
            PtrEntry entry = ptrRegistry.get(ptrId);
            if (entry != null && entry.valid) {
                return entry.typeId;
            }
        }
        return -1;
    }

    /**
     * ptValid - Check if pointer is valid
     */
    public static int ptValid(long ptrId) {
        synchronized (registryLock) {
            PtrEntry entry = ptrRegistry.get(ptrId);
            if (entry != null && entry.valid) {
                return 1;
            }
        }
        return 0;
    }

    // ========================================================================
    // UTILITY
    // ========================================================================

    /**
     * ptSizeof - Get size of allocated memory
     */
    public static long ptSizeof(long ptrId) {
        synchronized (registryLock) {
            PtrEntry entry = ptrRegistry.get(ptrId);
            if (entry != null) {
                if (entry.addr instanceof byte[]) {
                    return ((byte[]) entry.addr).length;
                }
                if (entry.addr instanceof ByteBuffer) {
                    return ((ByteBuffer) entry.addr).capacity();
                }
            }
        }
        return 0;
    }

    /**
     * ptClearAll - Clear entire registry
     */
    public static void ptClearAll() {
        synchronized (registryLock) {
            ptrRegistry.clear();
            nextPtrId = 1000;
        }
    }
}
