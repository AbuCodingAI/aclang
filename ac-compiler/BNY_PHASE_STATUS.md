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
- ✅ LIB_CALL (web functions) - web.pdf/help/page_get routed to libacweb.so

In Progress:
- ❌ INPUT (Term.ask) - integer path exists; string path returns stack-backed memory and is unreliable
- ⏸️ TYPE_CAST - syntax in AC not yet identified

## In-Progress Phases

### Phase 3: Memory, Arrays & Pointers 🟡 PARTIAL
Implemented:
- ✅ ALLOC - bump-allocator backed array/list allocation
- ✅ LOAD_INDEX - real indexed loads
- ✅ STORE_INDEX - real indexed stores
- ✅ APPEND helper - array append support
- ✅ FREE - accepted as a no-op; bump allocator does not reclaim individual objects
- ✅ NA→free global slots - promoted free variables can share writable global storage

Still incomplete:
- 🟡 Pointer arithmetic and pointer-library behavior need broader verification
- 🟡 Global slot coverage should keep expanding with scoping work

## Architecture Notes

### Emitted Helper Functions
On Linux, BNY generates inline helper functions:
- `__ac_print_int__()` - formatted integer output via sys_write
- `__ac_print_str__()` - string output via sys_write
- `__ac_print_cstr__()` - C-string output
- `ac_print_double()` - floating-point output
- `__ac_input_int__()` - integer input (IN PROGRESS - syscall issue)
- `__ac_input_str__()` - string input (IN PROGRESS - lifetime bug)
- `__ac_alloc__()` / `__ac_append__()` - array/list heap helpers

### External Links
- Dynamic libc via PLT/GOT: printf, dlopen, dlsym, strlen
- Math library: libacmath.so (via dlopen/dlsym on demand)
- Web library: libacweb.so (via dlopen/dlsym on demand)

## Key Issues to Address

1. **Term.ask Input**: syscall(0, fd=0, buffer, size) returns 0 bytes
   - Current string bug: `__ac_input_str__` returns a pointer into its own stack frame
   - Fix: store string input in stable memory and then re-check syscall parameters

2. **Array Support Verification**: Arrays are implemented, but need more regression coverage
   - Verified path: allocation, indexing, store, append
   - Required: document the layout and cover nested/mixed cases

3. **Pointer Arithmetic**: Not yet tested
   - Phase 3 will expose any gaps in register allocation or memory management

## Binary Characteristics

- **Format**: ELF64 on Linux
- **Linking**: Dynamic (libc.so.6)
- **Code Generation**: Direct x86-64 via X64Emitter
- **Calling Convention**: System V AMD64 ABI
- **Entry Point**: _start label (global init section)
- **ARM**: Not implemented. AC->BNY refuses ARM instead of routing through AC->C or compiler-generated assembly.

## Next Steps (Priority Order)

1. Fix Term.ask input reading (debug syscall parameters)
2. Expand Phase 3 regression tests (arrays, indexing, pointers)
3. Add static linking option for distribution
4. Windows PE support (via ASM backend delegation)
5. Complete Phase 4 (exceptions, eval, events)
