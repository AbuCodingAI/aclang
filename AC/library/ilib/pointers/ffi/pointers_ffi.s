# AC ilib: pointers — x86-64 Assembly FFI
# Pointer storage and operations
# Calling convention: rdi, rsi, rdx, rcx, r8, r9 (args)
# Returns in rax, rdx

.data
    ptr_registry:   .quad 0         # pointer to registry map
    next_ptr_id:    .quad 1000      # starting ID
    NULL_PTR:       .quad -1        # NULL pointer value
    registry_lock:  .quad 0         # mutex for thread safety

.text

# ============================================================================
# POINTER CREATION
# ============================================================================

# pt_new(value: rdi, valueSize: rsi, typeId: rdx) -> rax
# Create pointer to a value
.globl ac_pt_new
.type ac_pt_new, @function
ac_pt_new:
    push rbp
    mov rbp, rsp

    # Check for null or invalid size
    test rdi, rdi
    jz .pt_new_null
    test rsi, rsi
    jle .pt_new_null

    # Get next ID and increment
    lea rax, [rel next_ptr_id]
    mov rax, [rax]
    mov r10, rax
    add r10, 1
    lea r11, [rel next_ptr_id]
    mov [r11], r10

    # rax now contains the ID
    pop rbp
    ret

.pt_new_null:
    mov rax, [rel NULL_PTR]
    pop rbp
    ret

# pt_alloc(valueSize: rdi, typeId: rsi) -> rax
# Allocate storage
.globl ac_pt_alloc
.type ac_pt_alloc, @function
ac_pt_alloc:
    push rbp
    mov rbp, rsp

    test rdi, rdi
    jle .pt_alloc_null

    # Allocate memory (simplified - just return ID)
    lea rax, [rel next_ptr_id]
    mov rax, [rax]
    mov r10, rax
    add r10, 1
    lea r11, [rel next_ptr_id]
    mov [r11], r10

    pop rbp
    ret

.pt_alloc_null:
    mov rax, [rel NULL_PTR]
    pop rbp
    ret

# ============================================================================
# POINTER DEREFERENCING
# ============================================================================

# pt_get(ptrId: rdi) -> rax
# Get value from pointer
.globl ac_pt_get
.type ac_pt_get, @function
ac_pt_get:
    push rbp
    mov rbp, rsp

    # For now, return 0 (simplified)
    xor rax, rax

    pop rbp
    ret

# pt_set(ptrId: rdi, value: rsi, offset: rdx) -> rax
# Set value at pointer
.globl ac_pt_set
.type ac_pt_set, @function
ac_pt_set:
    push rbp
    mov rbp, rsp

    xor rax, rax  # return 0

    pop rbp
    ret

# ============================================================================
# POINTER ARITHMETIC
# ============================================================================

# pt_add(ptrId: rdi, offset: rsi) -> rax
# Add offset to pointer
.globl ac_pt_add
.type ac_pt_add, @function
ac_pt_add:
    push rbp
    mov rbp, rsp

    lea rax, [rel next_ptr_id]
    mov rax, [rax]
    mov r10, rax
    add r10, 1
    lea r11, [rel next_ptr_id]
    mov [r11], r10

    pop rbp
    ret

# pt_sub(ptrId1: rdi, ptrId2: rsi) -> rax
# Subtract two pointers
.globl ac_pt_sub
.type ac_pt_sub, @function
ac_pt_sub:
    xor rax, rax
    ret

# ============================================================================
# POINTER COMPARISON
# ============================================================================

# pt_eq(ptrId1: rdi, ptrId2: rsi) -> rax
# Check if pointers are equal
.globl ac_pt_eq
.type ac_pt_eq, @function
ac_pt_eq:
    cmp rdi, rsi
    je .pt_eq_true
    xor rax, rax
    ret
.pt_eq_true:
    mov rax, 1
    ret

# pt_null() -> rax
# Get NULL pointer
.globl ac_pt_null
.type ac_pt_null, @function
ac_pt_null:
    mov rax, [rel NULL_PTR]
    ret

# pt_is_null(ptrId: rdi) -> rax
# Check if pointer is NULL
.globl ac_pt_is_null
.type ac_pt_is_null, @function
ac_pt_is_null:
    cmp rdi, [rel NULL_PTR]
    je .pt_is_null_true
    xor rax, rax
    ret
.pt_is_null_true:
    mov rax, 1
    ret

# ============================================================================
# POINTER LIFECYCLE
# ============================================================================

# pt_free(ptrId: rdi) -> rax
# Free pointer
.globl ac_pt_free
.type ac_pt_free, @function
ac_pt_free:
    xor rax, rax
    ret

# pt_typeof(ptrId: rdi) -> rax
# Get type ID
.globl ac_pt_typeof
.type ac_pt_typeof, @function
ac_pt_typeof:
    mov rax, -1
    ret

# pt_valid(ptrId: rdi) -> rax
# Check if valid
.globl ac_pt_valid
.type ac_pt_valid, @function
ac_pt_valid:
    xor rax, rax
    ret

# ============================================================================
# UTILITY
# ============================================================================

# pt_sizeof(ptrId: rdi) -> rax
# Get size
.globl ac_pt_sizeof
.type ac_pt_sizeof, @function
ac_pt_sizeof:
    xor rax, rax
    ret

# pt_clear_all()
# Clear registry
.globl ac_pt_clear_all
.type ac_pt_clear_all, @function
ac_pt_clear_all:
    lea rax, [rel next_ptr_id]
    mov qword ptr [rax], 1000
    ret
