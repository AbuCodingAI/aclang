# BNY Backend Implementation Status

## Completed Phases

### Phase 1: Core IR Opcodes ✅ COMPLETE
All fundamental operations working:
- Variables & Constants (LOAD_CONST, LOAD_VAR, STORE_VAR)
- Arithmetic (ADD, SUB, MUL, DIV, IDIV, MOD, PMUL)
- Logic & Comparisons (AND, OR, NOT, XOR, XNOR, comparisons)
- Control Flow (COND, LABEL, JUMP, JUMP_IF_TRUE/FALSE)
- Loops (FOR, WHILE)
- Functions (FUNC_BEGIN/END, CALL, RETURN)

### Phase 2: I/O & Libraries ⏳ PARTIAL
Completed:
- ✅ PRINT (Term.display) via __ac_print_int__, __ac_print_str__
- ✅ LIB_CALL (math functions) - math.sqrt, math.sin, etc.

In Progress:
- ❌ INPUT (Term.ask) - implemented __ac_input_int__ but syscall read not returning data
- ⏸️ TYPE_CAST - syntax in AC not yet identified

## In-Progress Phases

### Phase 3: Memory & Pointers ⏸️ NOT STARTED
Needs Implementation:
- ALLOC (array/list creation)
- FREE (memory deallocation)
- LOAD_INDEX (array indexing)
- STORE_INDEX (array assignment)

Note: Requires understanding AC's memory layout for composite types

## Architecture Notes

### Emitted Helper Functions
On Linux, BNY generates inline helper functions:
- `__ac_print_int__()` - formatted integer output via sys_write
- `__ac_print_str__()` - string output via sys_write
- `__ac_print_cstr__()` - C-string output
- `ac_print_double()` - floating-point output
- `__ac_input_int__()` - integer input (IN PROGRESS - syscall issue)

### External Links
- Dynamic libc via PLT/GOT: printf, dlopen, dlsym, strlen
- Math library: libacmath.so (via dlopen/dlsym on demand)
- Web library: libacweb.so (via dlopen/dlsym on demand)

## Key Issues to Address

1. **Term.ask Input**: syscall(0, fd=0, buffer, size) returns 0 bytes
   - Possible causes: FD tracking, stack alignment, buffer address calc
   - Fix: Debug syscall parameters in __ac_input_int__

2. **Array Support**: Need to understand AC's array memory layout
   - Current: Arrays are allocated but indexing returns 0
   - Required: Document how ALLOC stores data (pointer? inline? metadata?)

3. **Pointer Arithmetic**: Not yet tested
   - Phase 3 will expose any gaps in register allocation or memory management

## Binary Characteristics

- **Format**: ELF64 on Linux
- **Linking**: Dynamic (libc.so.6)
- **Code Generation**: Direct x86-64 via X64Emitter
- **Calling Convention**: System V AMD64 ABI
- **Entry Point**: _start label (global init section)

## Next Steps (Priority Order)

1. Fix Term.ask input reading (debug syscall parameters)
2. Implement Phase 3 (arrays, indexing, pointers)
3. Add static linking option for distribution
4. Windows PE support (via ASM backend delegation)
5. Complete Phase 4 (exceptions, eval, events)

