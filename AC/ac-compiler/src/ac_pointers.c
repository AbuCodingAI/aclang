/*
 * AC Pointer Runtime Helpers
 * Provides pointer operations for BNY backend
 *
 * Strategy: Global registry with dynamic array for pointer storage
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAX_POINTERS 10000

/* Global pointer registry */
static void* __ac_ptr_registry[MAX_POINTERS];
static int64_t __ac_ptr_next_id = 0;

/*
 * __ac_ptr_new__(rdi = value to point to)
 * Returns pointer ID in RAX
 *
 * Note: In BNY, values are passed by value (64-bit integers or addresses)
 * We store the value directly in the registry
 */
int64_t __ac_ptr_new__(int64_t value) {
    if (__ac_ptr_next_id >= MAX_POINTERS) {
        return -1;  /* Error: pointer registry full */
    }

    int64_t ptr_id = __ac_ptr_next_id++;
    __ac_ptr_registry[ptr_id] = (void*)value;
    return ptr_id;
}

/*
 * __ac_ptr_deref__(rdi = pointer ID)
 * Returns dereferenced value in RAX
 */
int64_t __ac_ptr_deref__(int64_t ptr_id) {
    if (ptr_id < 0 || ptr_id >= __ac_ptr_next_id) {
        return 0;  /* Error: invalid pointer */
    }

    return (int64_t)__ac_ptr_registry[ptr_id];
}

/*
 * __ac_ptr_update__(rdi = pointer ID, rsi = new value)
 * Updates value at pointer
 * Returns 1 if successful, 0 if invalid pointer
 */
int64_t __ac_ptr_update__(int64_t ptr_id, int64_t new_value) {
    if (ptr_id < 0 || ptr_id >= __ac_ptr_next_id) {
        return 0;  /* Error: invalid pointer */
    }

    __ac_ptr_registry[ptr_id] = (void*)new_value;
    return 1;
}
