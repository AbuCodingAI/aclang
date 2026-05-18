# IR Redesign Plan - Fixing Core Issues

## Current Problems Identified

### 1. ❗ String-based operands (CRITICAL)
**Problem**: `std::vector<std::string> operands;`
- No type safety
- Can't distinguish variables from temps from constants
- Backend ambiguity
- Defeats the purpose of structured IR

**Solution**: Replace with typed references
```cpp
struct IRRef {
    enum class Kind { 
        TEMP,      // Temporary value (t0, t1, etc.)
        VAR,       // Named variable
        CONST,     // Constant value
        LABEL,     // Jump label
        FUNCTION   // Function reference
    };
    Kind kind;
    int id;              // For temps, labels
    std::string name;    // For vars, functions
    IRValue value;       // For constants
};

struct IRInstruction {
    IROpcode opcode;
    IRRef result;              // Where result goes (for expressions)
    std::vector<IRRef> operands;  // Typed operands
};
```

### 2. ❗ IRValue incomplete for type system
**Problem**: IRValue supports `int, double, string, bool` but IRType claims `LIST, TUPLE, OBJECT, FUNCTION`

**Solution**: Expand IRValue or restrict IRType
```cpp
// Option A: Expand IRValue
struct IRValue {
    std::variant<
        int,
        double,
        std::string,
        bool,
        std::vector<IRValue>,      // For LIST
        std::shared_ptr<IRObject>, // For OBJECT
        int                        // Function ID reference
    > data;
    IRType type;
};

// Option B: Restrict IRType (simpler, recommended)
enum class IRType {
    INT,
    FLOAT,
    STRING,
    BOOL,
    VOID,
    POINTER  // For complex types, use pointers
};
```

### 3. ❗ Mixed abstraction levels
**Problem**: Mix of low-level (JUMP, LABEL) and high-level (PRINT, INPUT, EVENT_BIND)

**Solution**: Choose one level - RECOMMENDED: Lower to runtime calls
```cpp
// BEFORE (mixed):
PRINT "hello"
INPUT x

// AFTER (consistent low-level):
t0 = LOAD_CONST "hello"
CALL __runtime_print, t0
t1 = CALL __runtime_input
STORE_VAR x, t1
```

### 4. ❗ No explicit result targets / SSA
**Problem**: Operations like ADD, MUL have no explicit result destination

**Solution**: SSA-style with explicit results
```cpp
// BEFORE (implicit):
ADD x, y

// AFTER (explicit):
t0 = ADD x, y
STORE_VAR result, t0
```

### 5. ❗ Undefined memory model
**Problem**: ALLOC/FREE exist but no clear ownership model for different backends

**Solution**: Abstract memory operations
```cpp
// Instead of raw ALLOC/FREE:
enum class IROpcode {
    // Memory operations (backend-specific implementation)
    ALLOC_ARRAY,    // Backend decides: malloc, new[], Vec::new()
    ALLOC_OBJECT,   // Backend decides: malloc, new, {}
    FREE_MEMORY,    // Backend decides: free, delete, drop
    
    // Reference operations
    LOAD_FIELD,     // obj.field
    STORE_FIELD,    // obj.field = val
    LOAD_INDEX,     // arr[i]
    STORE_INDEX,    // arr[i] = val
};
```

### 6. ⚠️ FUNC_BEGIN/FUNC_END redundant
**Problem**: Already have IRFunction structure

**Solution**: Remove opcodes, use structure
```cpp
// IRFunction already contains:
struct IRFunction {
    std::string name;
    std::vector<std::string> params;
    std::vector<IRInstruction> instructions;
    // No need for FUNC_BEGIN/FUNC_END opcodes
};
```

### 7. ⚠️ Backend field in IRProgram
**Problem**: `std::string backend;` breaks IR reusability

**Solution**: Remove from IR, handle in compiler driver
```cpp
// IR should be backend-agnostic
struct IRProgram {
    std::vector<IRFunction> functions;
    std::vector<IRInstruction> globalInit;
    // NO backend field
};

// Backend selection in main.cpp instead
```

## Proposed New IR Structure

```cpp
// ir.hpp - Redesigned

#pragma once
#include <string>
#include <vector>
#include <variant>
#include <memory>

// Basic types
enum class IRType {
    INT,
    FLOAT,
    STRING,
    BOOL,
    VOID,
    POINTER
};

// Value representation
struct IRValue {
    std::variant<int, double, std::string, bool> data;
    IRType type;
};

// Reference to values/variables/temps
struct IRRef {
    enum class Kind { TEMP, VAR, CONST, LABEL, FUNCTION };
    Kind kind;
    int id;              // For TEMP (t0, t1), LABEL (L0, L1)
    std::string name;    // For VAR, FUNCTION
    IRValue value;       // For CONST
    
    // Constructors for convenience
    static IRRef temp(int id) { return {Kind::TEMP, id, "", {}}; }
    static IRRef var(const std::string& n) { return {Kind::VAR, 0, n, {}}; }
    static IRRef constant(const IRValue& v) { return {Kind::CONST, 0, "", v}; }
    static IRRef label(int id) { return {Kind::LABEL, id, "", {}}; }
    static IRRef func(const std::string& n) { return {Kind::FUNCTION, 0, n, {}}; }
};

// Opcodes - LOW LEVEL ONLY
enum class IROpcode {
    // Arithmetic
    ADD, SUB, MUL, DIV, MOD,
    
    // Comparison
    EQ, NE, LT, LE, GT, GE,
    
    // Logical
    AND, OR, NOT,
    
    // Memory
    LOAD_CONST,      // Load constant into temp
    LOAD_VAR,        // Load variable into temp
    STORE_VAR,       // Store temp into variable
    LOAD_INDEX,      // Load array[index]
    STORE_INDEX,     // Store to array[index]
    
    // Control flow
    JUMP,            // Unconditional jump
    JUMP_IF_TRUE,    // Conditional jump
    JUMP_IF_FALSE,   // Conditional jump
    LABEL,           // Jump target
    
    // Functions
    CALL,            // Function call
    RETURN,          // Return from function
    
    // Memory management (abstract)
    ALLOC,           // Allocate memory
    FREE,            // Free memory
    
    // Special
    NOP,             // No operation
    PHI              // SSA phi node (for optimization)
};

// Instruction with explicit result
struct IRInstruction {
    IROpcode opcode;
    IRRef result;                  // Where result goes (optional)
    std::vector<IRRef> operands;   // Typed operands
    
    // Metadata
    int lineNumber = 0;
    std::string comment;
};

// Function representation
struct IRFunction {
    std::string name;
    std::vector<std::string> params;
    std::vector<IRInstruction> instructions;
    int tempCount = 0;  // For generating unique temp IDs
    int labelCount = 0; // For generating unique label IDs
};

// Program representation (backend-agnostic)
struct IRProgram {
    std::vector<IRFunction> functions;
    std::vector<IRInstruction> globalInit;
    
    // Helper to find function
    IRFunction* findFunction(const std::string& name);
};
```

## Example IR Generation

### Before (current):
```
ADD x y
STORE result
```

### After (redesigned):
```
t0 = LOAD_VAR x
t1 = LOAD_VAR y
t2 = ADD t0, t1
STORE_VAR result, t2
```

### In code:
```cpp
// Generate: result = x + y
IRRef t0 = IRRef::temp(func.tempCount++);
IRRef t1 = IRRef::temp(func.tempCount++);
IRRef t2 = IRRef::temp(func.tempCount++);

func.instructions.push_back({
    IROpcode::LOAD_VAR, t0, {IRRef::var("x")}
});
func.instructions.push_back({
    IROpcode::LOAD_VAR, t1, {IRRef::var("y")}
});
func.instructions.push_back({
    IROpcode::ADD, t2, {t0, t1}
});
func.instructions.push_back({
    IROpcode::STORE_VAR, {}, {IRRef::var("result"), t2}
});
```

## Migration Strategy

### Phase 1: Add new structures (non-breaking)
1. Add IRRef struct to ir.hpp
2. Add result field to IRInstruction
3. Keep old string operands temporarily

### Phase 2: Update IR generator
1. Modify ir_generator.cpp to use IRRef
2. Generate explicit temps
3. Add result targets

### Phase 3: Update backends
1. Update ir_codegen.cpp to read IRRef
2. Test with existing backends
3. Remove string operands

### Phase 4: Cleanup
1. Remove high-level opcodes (PRINT, INPUT)
2. Remove FUNC_BEGIN/FUNC_END
3. Remove backend field from IRProgram

## Benefits After Redesign

1. **Type Safety**: Can't confuse variables with temps
2. **Optimization**: Can track value flow, do DCE, constant folding
3. **Backend Clarity**: Each backend knows exactly what to generate
4. **Debugging**: Can print IR in readable SSA form
5. **Portability**: Same IR works for all backends

## Timeline

- Phase 1: 1-2 hours (add structures)
- Phase 2: 2-3 hours (update generator)
- Phase 3: 3-4 hours (update backends)
- Phase 4: 1 hour (cleanup)

**Total: ~8-10 hours of focused work**

## Priority

This is HIGH PRIORITY because:
- Current IR limits optimization potential
- String-based operands cause bugs
- Mixed abstraction makes backends inconsistent
- Will only get harder to fix as codebase grows

## Next Steps

1. Review this plan
2. Implement Phase 1 (add IRRef)
3. Test with simple example
4. Continue to Phase 2
