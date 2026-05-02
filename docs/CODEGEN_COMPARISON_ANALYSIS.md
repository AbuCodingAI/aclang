# Codegen Comparison: Python vs C++ vs V vs ASM

## Summary

After reading all four backends, here's what each has that the others don't:

## Python UNIQUE Features:

1. **Conditional Event Listener Emission** ✓
   - Has `usesEventListener()` helper function
   - Only emits keybind imports and EventListener class when actually used
   - **C++, V, ASM: ALWAYS emit keybind spam**

2. **Enhanced unwrapDollars()** ✓
   - Handles escape sequences: `\"`, `\\`, `\n`, `\t`
   - Properly escapes special characters
   - **C++, V, ASM: NOW HAVE THIS TOO (we just added it)**

3. **Widget Library Support**
   - Checks for `use ilib widgets` and imports widgets library
   - Adds `root.mainloop()` before `/kill` when widgets are used
   - **C++, V, ASM: No widget support**

4. **Case-Insensitive Boolean Translation**
   - Translates `true`/`True`/`TRUE` → `True`
   - Translates `false`/`False`/`FALSE` → `False`
   - **C++, V, ASM: Case-sensitive or no boolean translation**

5. **Keyword Argument Support**
   - Parses `func(key=value, key2=value2)` syntax
   - Properly handles keyword arguments in function calls and method calls
   - **C++, V: Partial support; ASM: No support**

6. **Null → None Translation**
   - Translates `null` to `None` in keyword arguments
   - **C++, V, ASM: No null handling**

## C++ UNIQUE Features:

1. **Keybind Header Includes** (SPAM)
   - `#include "../library/keybinds/keybinds.hpp"`
   - `#include "../library/event_listener/event_listener.hpp"`
   - `using namespace ACKeybinds;`
   - **ALWAYS EMITTED - NEEDS FIX**

2. **Global Variable Support**
   - Emits global variables before main()
   - **Python, V, ASM: No global variable support**

3. **Vector-Based Collections**
   - Uses `vector<string>` for lists
   - Uses `const vector<string>` for tuples
   - **Python: Native lists/tuples; V: Arrays; ASM: Heap-allocated**

4. **Type Inference for Variables**
   - Uses `Type::inferNumeral()` to determine int/float/double
   - Emits proper C++ types: `int`, `double`, `string`
   - **Python: Dynamic typing; V: Manual type annotation; ASM: All numeric**

5. **Index Expression Support**
   - Has `NodeType::IndexExpr` handling
   - Supports `list[index] = value`
   - **Python, V, ASM: No dedicated index expression handling**

6. **Tag System Integration**
   - Uses `Tags::isMainLoop()`, `Tags::isGUIBox()`, etc.
   - Proper tag block handling with GUI/logic scopes
   - **Python, V, ASM: Basic tag handling only**

## V UNIQUE Features:

1. **Keybind Constants Block** (SPAM)
   - Emits 50+ lines of `const ( key_1 = "1" ... )` constants
   - **ALWAYS EMITTED - NEEDS FIX**

2. **EventListener Struct** (SPAM)
   - Emits full EventListener struct with map-based bindings
   - `__global ac_event_listener = new_event_listener()`
   - **ALWAYS EMITTED - NEEDS FIX**

3. **Single Quote Strings**
   - Uses `'...'` for strings instead of `"..."`
   - Escapes `\'` instead of `\"`
   - **Python, C++, ASM: Use double quotes**

4. **For Loop as While**
   - Translates `whilst` loops to `for condition { }`
   - V doesn't have `while`, only `for`
   - **Python, C++, ASM: Use proper while loops**

5. **Function Collection in Separate Stream**
   - Uses `functions` ostringstream to collect functions
   - Emits all functions before main()
   - **Python: Emits functions inline; C++: Uses functions stream; ASM: Uses text_funcs**

6. **Import os for Input**
   - `import os` for `os.input()` function
   - **Python: Uses built-in input(); C++: Uses cin; ASM: Uses scanf**

## ASM UNIQUE Features:

1. **Keybind .equ Directives** (SPAM)
   - Emits 50+ lines of `.equ KEY_1, 0x31` directives
   - **ALWAYS EMITTED - NEEDS FIX**

2. **Multi-Section Output**
   - `.rodata` for strings and constants
   - `.bss` for uninitialized data (input_buffer)
   - `.text` for code
   - **Python, C++, V: Single output stream**

3. **String Table Management**
   - `allocateString()` function to deduplicate strings
   - Generates `.str_0`, `.str_1`, etc. labels
   - **Python, C++, V: Inline strings**

4. **Stack Frame Management**
   - `allocateStackVariable()` tracks stack offsets
   - Manual stack frame setup: `push %rbp`, `mov %rsp, %rbp`, `sub $256, %rsp`
   - **Python, C++, V: Automatic stack management**

5. **Expression Tokenizer**
   - `tokenizeExpr()` parses expressions into tokens
   - Precedence climbing parser: `parseAddSub()`, `parseMulDiv()`, `parsePrimary()`
   - **Python, C++, V: Rely on target language's expression parsing**

6. **Register-Based Code Generation**
   - Uses x86-64 registers: `%rax`, `%rbx`, `%rcx`, `%rdx`, `%rdi`, `%rsi`
   - Manual register allocation and spilling
   - **Python, C++, V: High-level code generation**

7. **Heap Allocation for Lists**
   - Uses `malloc` to allocate lists on heap
   - Stores length in first 8 bytes, elements follow
   - **Python: Native lists; C++: vector; V: arrays**

8. **Printf Format Strings**
   - `fmt_int: .asciz "%d\\n"` for integers
   - `fmt_str: .asciz "%s\\n"` for strings
   - **Python, C++, V: Use language's print functions**

## Common Features (All 4 Have):

1. **@ → * Translation** ✓
2. **#= → != Translation** ✓
3. **is → == Translation** ✓
4. **Function Call Assignment** ✓
5. **Foreign Block Support** ✓
6. **Kill/Raise/Return Statements** ✓
7. **If/ElseIf/Else Handling** ✓
8. **For/Whilst Loops** ✓
9. **Function Definitions** ✓
10. **Display/Ask Methods** ✓

## CRITICAL ISSUES TO FIX:

### 1. Keybind Spam (C++, V, ASM)
- **C++**: Unconditionally includes keybinds.hpp and event_listener.hpp
- **V**: Unconditionally emits EventListener struct and 50+ keybind constants
- **ASM**: Unconditionally emits 50+ .equ keybind directives

**FIX**: Add `usesEventListener()` helper to C++, V, ASM (like Python and JavaScript)

### 2. String Escaping (DONE ✓)
- All backends now have enhanced `unwrapDollars()` with escape sequence handling

### 3. fn "..." String Parsing (NOT STARTED)
- Parser needs to handle `fn "..."` syntax
- Lexer already handles escape sequences in `"..."` strings
- Need to update parser to recognize `fn` prefix for strings

## Next Steps:

1. ✓ Enhanced unwrapDollars() - COMPLETE
2. ✓ Remove keybind spam from Python and JavaScript - COMPLETE
3. **Remove keybind spam from C++, V, ASM** - IN PROGRESS
4. Implement `fn "..."` string parsing
5. Implement `fn *` multiplication
6. Implement `fn &` and `fn &&` chaining
