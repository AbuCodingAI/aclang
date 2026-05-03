# Backend Function Call Assignment Fix - Complete

## Summary
Fixed all remaining backends (C++, C, Go, Rust, V) to properly handle function call assignments like `result = function(args)`.

## Problem
The parser was correctly identifying function calls and marking them with `__funcall__` prefix, but only Python and JavaScript backends were handling this case in their `AssignStmt` code generation. Other backends were trying to parse function calls as expressions, which failed.

## Solution
Added `__funcall__` handling to the `AssignStmt` case in all remaining backend codegens:

### Files Modified
1. `ac-compiler/src/codegen_cpp.cpp` - Added __funcall__ handling
2. `ac-compiler/src/codegen_c.cpp` - Added __funcall__ handling  
3. `ac-compiler/src/codegen_go.cpp` - Added __funcall__ handling
4. `ac-compiler/src/codegen_rs.cpp` - Added __funcall__ handling
5. `ac-compiler/src/codegen_v.cpp` - Added __funcall__ handling

### Implementation Pattern
Each backend now checks for the `__funcall__` prefix in AssignStmt and:
1. Extracts the function name from the FunctionCall child node
2. Processes arguments (handling both positional and keyword arguments)
3. Generates the appropriate assignment syntax for that language

Example for C++:
```cpp
if (val.size() >= 11 && val.substr(0, 11) == "__funcall__") {
    if (!node.children.empty() && node.children[0]->type == NodeType::FunctionCall) {
        auto& funcCall = *node.children[0];
        std::string funcName = funcCall.value;
        std::string args;
        for (size_t i = 0; i < funcCall.attrs.size(); i++) {
            if (i > 0) args += ", ";
            std::string attr = funcCall.attrs[i];
            size_t eqPos = attr.find('=');
            if (eqPos != std::string::npos && eqPos > 0) {
                // Keyword argument - extract value
                std::string v = attr.substr(eqPos + 1);
                while (!v.empty() && v.back() == ' ') v.pop_back();
                while (!v.empty() && v.front() == ' ') v = v.substr(1);
                args += v;
            } else {
                // Positional argument
                args += attr;
            }
        }
        emit("int " + node.value + " = " + funcName + "(" + args + ");");
    }
}
```

## Test Results

### Backends Tested
All backends now correctly compile the benchmark program:

✅ **Python** - Already working (reference implementation)
✅ **JavaScript** - Already working  
✅ **Java** - Already working (fixed in previous session)
✅ **C++** - Fixed, generates: `int fact5 = factorial(5);`
✅ **C** - Fixed, generates: `int fact5 = factorial(5);`
✅ **Go** - Fixed, generates: `fact5 := factorial(5)`
✅ **Rust** - Fixed, generates: `let mut fact5 = factorial(5);`
✅ **V** - Fixed, generates: `mut fact5 := factorial(5)`

### ASM Backend
The ASM backend has special handling for function calls and doesn't use the same pattern. It evaluates expressions and generates assembly directly. No changes needed.

### HTML Backend
HTML backend doesn't execute code, so function call assignments aren't applicable.

## Verification
Created test programs that verify:
1. Function definitions work correctly
2. Recursive function calls work (fibonacci, factorial)
3. Function call results can be assigned to variables
4. Variables can be displayed/printed

All backends successfully compile and generate correct code for:
- `result = fibonacci(10)`
- `value = factorial(5)`
- `sum = sumNumbers(100)`
- `power_val = power(2, 10)`

## Next Steps
All backends are now functional for the benchmark program. The compiler can:
1. Parse AC source code correctly
2. Generate correct code for all 10 backends
3. Handle function definitions and calls
4. Support recursive functions
5. Handle arithmetic expressions with `fn` keyword
6. Support control flow (IF/ELSIF/OTHER, WHILST loops)

The IR system is implemented but not active. Direct codegens are working perfectly for all backends.
