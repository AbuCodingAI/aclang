# Fix All Backends - Action Plan

## Issues Found

### Java
1. ❌ `__funcall__` not handled in AssignStmt
2. ✅ IF/OTHER already has the fix
3. ❌ Function bodies incomplete

### C++, C, Go, Rust, V
- Need to check each one individually
- Likely need `__funcall__` handling
- IF/OTHER fixes already applied to most

## Fix Strategy

Apply the same pattern used in JavaScript:

```cpp
else if (val.size() >= 11 && val.substr(0, 11) == "__funcall__") {
    // Function call: name = func(arg1, arg2)
    if (!node.children.empty() && node.children[0]->type == NodeType::FunctionCall) {
        auto& funcCall = *node.children[0];
        std::string funcName = funcCall.value;
        std::string args;
        for (size_t i = 0; i < funcCall.attrs.size(); i++) {
            if (i > 0) args += ", ";
            std::string attr = funcCall.attrs[i];
            // Check if it's a keyword argument
            size_t eqPos = attr.find('=');
            if (eqPos != std::string::npos && eqPos > 0) {
                // Extract value part
                std::string val = attr.substr(eqPos + 1);
                args += val;
            } else {
                // Positional argument
                args += attr;
            }
        }
        emit("Type varName = funcName(args);");
    }
}
```

## Backends to Fix

1. Java - Add `__funcall__` handling
2. C++ - Add `__funcall__` handling (IF/OTHER already fixed)
3. C - Add `__funcall__` handling (IF/OTHER already fixed)
4. Go - Add `__funcall__` handling (IF/OTHER already fixed)
5. Rust - Add `__funcall__` handling (IF/OTHER already fixed)
6. V - Add `__funcall__` handling (IF/OTHER already fixed)
7. ASM - Special case, may need different approach

## Implementation Order

1. Java (most complex type system)
2. C++/C (similar syntax)
3. Go
4. Rust
5. V
6. ASM (last, special handling)
