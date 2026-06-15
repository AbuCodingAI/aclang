# Design Document: High-Performance AC Compiler

## Overview

This design transforms the AC compiler from its current implementation into a high-performance compiler achieving C-level execution speeds. The transformation addresses three critical performance bottlenecks:

1. **Frontend bottleneck**: String-heavy expression handling requiring re-parsing in the IR layer
2. **IR bottleneck**: String-based references with allocation-heavy emission
3. **Backend bottleneck**: External compiler invocation overhead via system() calls

The solution implements a fully integerized IR with arena allocation, SSA-form optimizations, and direct machine code generation. This eliminates "interpretation disguised as compilation" by treating compilation as a proper multi-stage transformation pipeline.

### Current Architecture Problems

**Frontend (Parser)**: Expressions are captured as strings and stored in AST nodes. The parser does minimal work, deferring expression analysis to later stages. This creates a "parse once, re-parse many times" anti-pattern.

**IR Layer**: Uses string-based temporaries ("t0", "t1"), string-based labels ("L0", "L1"), and per-instruction heap allocation via std::unique_ptr. Every IR operation involves string comparisons and allocations.

**Backend**: Invokes external compilers (gcc, clang) via system() calls, adding 50-200ms overhead per compilation. The ASM backend generates text assembly that must be assembled and linked externally.

### Proposed Architecture

**Frontend**: Pratt parser for expressions producing structured AST nodes (BinaryExpr, UnaryExpr, CallExpr) with typed operands. Parsing happens exactly once.

**IR Layer**: Integerized references (temporaries as integers 0,1,2..., labels as integers 0,1,2...), flat instruction arrays with arena allocation, immediate CFG lowering, SSA conversion pass.

**Backend**: Direct machine code generation for x86-64, ARM64, RISC-V. Integrated assembler eliminates external process invocation.

### Performance Targets

- **Compilation speed**: 5x faster than current implementation (target: <100ms for 1000-line programs)
- **Execution speed**: Within 2x of gcc -O2 on benchmarks (fibonacci, factorial, array operations)
- **Memory efficiency**: 50% reduction in IR memory footprint through arena allocation

## Architecture

### Compilation Pipeline

```
Source Code (.ac)
    ↓
Lexer (unchanged)
    ↓
Parser (NEW: Pratt parser for expressions)
    ↓
AST (NEW: structured expression nodes)
    ↓
IR Builder (NEW: integerized IR with arena allocation)
    ↓
CFG Builder (NEW: immediate control flow lowering)
    ↓
SSA Converter (NEW: SSA form transformation)
    ↓
Optimizer (NEW: constant folding, DCE, copy propagation, inlining)
    ↓
Register Allocator (NEW: graph coloring or linear scan)
    ↓
Machine IR (NEW: architecture-specific IR)
    ↓
Code Generator (NEW: direct machine code emission)
    ↓
Binary Output (.acb, ELF/Mach-O/PE)
```

### Component Interactions

```
┌─────────────────────────────────────────────────────────────┐
│                         Frontend                             │
│  ┌──────────┐      ┌──────────────┐      ┌──────────────┐  │
│  │  Lexer   │─────▶│ Pratt Parser │─────▶│ Structured   │  │
│  │          │      │              │      │     AST      │  │
│  └──────────┘      └──────────────┘      └──────────────┘  │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                        IR Layer                              │
│  ┌──────────────┐   ┌──────────────┐   ┌──────────────┐   │
│  │  IR Builder  │──▶│ CFG Builder  │──▶│ SSA Convert  │   │
│  │ (integerized)│   │ (early lower)│   │              │   │
│  └──────────────┘   └──────────────┘   └──────────────┘   │
│         │                                       │           │
│         ▼                                       ▼           │
│  ┌──────────────┐                     ┌──────────────┐    │
│  │Arena Alloc   │                     │  Optimizer   │    │
│  │              │                     │  Passes      │    │
│  └──────────────┘                     └──────────────┘    │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                        Backend                               │
│  ┌──────────────┐   ┌──────────────┐   ┌──────────────┐   │
│  │  Register    │──▶│  Machine IR  │──▶│   Code Gen   │   │
│  │  Allocator   │   │              │   │  (x86-64)    │   │
│  └──────────────┘   └──────────────┘   └──────────────┘   │
│                                                 │           │
│                                                 ▼           │
│                                        ┌──────────────┐    │
│                                        │ Binary Output│    │
│                                        │ (ELF/Mach-O) │    │
│                                        └──────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

### Data Flow

1. **Lexer → Parser**: Token stream
2. **Parser → IR Builder**: Structured AST with typed expression nodes
3. **IR Builder → CFG Builder**: Flat instruction array with integer references
4. **CFG Builder → SSA Converter**: CFG with explicit jumps and labels
5. **SSA Converter → Optimizer**: SSA-form IR
6. **Optimizer → Register Allocator**: Optimized IR
7. **Register Allocator → Machine IR**: IR with register assignments
8. **Machine IR → Code Generator**: Architecture-specific IR
9. **Code Generator → Binary**: Machine code bytes

## Components and Interfaces

### 1. Pratt Parser (Frontend)

**Purpose**: Parse expressions into structured AST nodes in a single pass.

**Interface**:
```cpp
class PrattParser {
public:
    // Parse expression from token stream
    NodePtr parseExpression(int precedence = 0);
    
    // Parse prefix operators (-, NOT, etc.)
    NodePtr parsePrefix();
    
    // Parse infix operators (+, -, *, /, etc.)
    NodePtr parseInfix(NodePtr left, TokenType op);
    
private:
    int getPrecedence(TokenType op);
    bool isRightAssociative(TokenType op);
};
```

**Precedence Table**:
```
Precedence 1: OR
Precedence 2: AND
Precedence 3: ==, !=, <, >, <=, >=
Precedence 4: +, -
Precedence 5: *, /, %, @
Precedence 6: NOT, unary -
Precedence 7: function calls, indexing
```

**Algorithm**: Top-down operator precedence parsing (Pratt parsing)
- Start with precedence 0
- Parse prefix expression
- While current token has higher precedence than minimum:
  - Parse infix expression with left operand
  - Update left operand to result
- Return final expression

### 2. Structured Expression AST Nodes (Frontend)

**Purpose**: Replace string-based expression storage with typed nodes.

**Node Types**:
```cpp
// Binary expression: left op right
struct BinaryExpr : ASTNode {
    NodePtr left;
    TokenType op;  // +, -, *, /, %, @, <, >, etc.
    NodePtr right;
};

// Unary expression: op operand
struct UnaryExpr : ASTNode {
    TokenType op;  // -, NOT
    NodePtr operand;
};

// Function call: func(arg1, arg2, ...)
struct CallExpr : ASTNode {
    std::string funcName;
    std::vector<NodePtr> arguments;
};

// Array indexing: array[index]
struct IndexExpr : ASTNode {
    NodePtr array;
    NodePtr index;
};

// Literal values
struct LiteralExpr : ASTNode {
    enum class Type { INT, FLOAT, STRING, BOOL, NULL_VAL };
    Type type;
    std::variant<int, double, std::string, bool> value;
};
```

**Integration**: Parser creates these nodes directly. IR builder consumes them without string parsing.

### 3. Integerized IR (IR Layer)

**Purpose**: Replace string-based references with integer IDs for optimal performance.

**Data Structures**:
```cpp
// IR Reference - uses integers instead of strings
struct IRRef {
    enum class Kind {
        TEMP,      // Temporary: id = 0, 1, 2, ...
        LABEL,     // Label: id = 0, 1, 2, ...
        VAR,       // Variable: id = symbol table index
        CONST,     // Constant: embedded value
        NONE       // No reference
    };
    
    Kind kind;
    int id;              // For TEMP, LABEL, VAR
    IRValue value;       // For CONST
    
    // String conversion only for debugging
    std::string toString() const;
};

// IR Instruction - flat structure
struct IRInstruction {
    IROpcode opcode;
    IRRef result;                    // Destination (optional)
    std::vector<IRRef> operands;     // Source operands
    int lineNumber;                  // For debug info
};

// IR Function - flat instruction array
struct IRFunction {
    int nameId;                      // String table index
    std::vector<int> parameterIds;   // Symbol table indices
    std::vector<IRInstruction> instructions;  // Flat array
    IRType returnType;
};
```

**Symbol Table**:
```cpp
class SymbolTable {
    std::vector<std::string> names;           // Index → name
    std::unordered_map<std::string, int> ids; // Name → index
    
public:
    int intern(const std::string& name);
    const std::string& getName(int id) const;
};
```

**Benefits**:
- Integer comparisons instead of string comparisons
- Cache-friendly flat arrays
- No per-instruction allocation overhead
- Compact memory representation

### 4. Arena Allocator (IR Layer)

**Purpose**: Eliminate per-instruction heap allocation overhead.

**Interface**:
```cpp
class Arena {
    struct Block {
        std::unique_ptr<char[]> data;
        size_t size;
        size_t used;
    };
    
    std::vector<Block> blocks;
    size_t blockSize;  // Default: 64KB
    
public:
    Arena(size_t blockSize = 65536);
    
    // Allocate memory from arena
    void* allocate(size_t size, size_t alignment = 8);
    
    // Allocate typed object
    template<typename T, typename... Args>
    T* create(Args&&... args);
    
    // Reset arena (keep blocks, reset used counters)
    void reset();
    
    // Free all blocks
    void clear();
};
```

**Usage Pattern**:
```cpp
Arena arena;
IRFunction* func = arena.create<IRFunction>("main");
for (int i = 0; i < 100; i++) {
    IRInstruction* instr = arena.create<IRInstruction>();
    func->instructions.push_back(*instr);
}
// No individual deallocations needed
arena.clear();  // Free everything at once
```

**Memory Layout**:
```
Block 0: [IRFunction][IRInstruction][IRInstruction]...[free space]
Block 1: [IRInstruction][IRInstruction]...[free space]
Block 2: [IRInstruction]...[free space]
```

### 5. CFG Builder (IR Layer)

**Purpose**: Lower high-level control flow to explicit jumps and labels immediately.

**Transformation Examples**:

**IF Statement**:
```
// High-level
IF condition
    body
OTHER
    else_body

// Lowered CFG
    t0 = eval(condition)
    jf t0, L_else
    [body instructions]
    jmp L_end
L_else:
    [else_body instructions]
L_end:
```

**WHILST Loop**:
```
// High-level
WHILST condition
    body

// Lowered CFG
L_start:
    t0 = eval(condition)
    jf t0, L_end
    [body instructions]
    jmp L_start
L_end:
```

**FOR Loop**:
```
// High-level
FOR item in collection
    body

// Lowered CFG
    t0 = iter_init(collection)
L_start:
    t1 = iter_has_next(t0)
    jf t1, L_end
    item = iter_next(t0)
    [body instructions]
    jmp L_start
L_end:
```

**Interface**:
```cpp
class CFGBuilder {
    Arena& arena;
    SymbolTable& symbols;
    int tempCounter;
    int labelCounter;
    
public:
    // Lower AST node to CFG instructions
    void lowerNode(const ASTNode& node, std::vector<IRInstruction>& out);
    
    // Lower expression to temporary
    IRRef lowerExpr(const ASTNode& expr, std::vector<IRInstruction>& out);
    
    // Allocate new temporary
    IRRef newTemp();
    
    // Allocate new label
    IRRef newLabel();
};
```

### 6. SSA Converter (IR Layer)

**Purpose**: Transform IR into Static Single Assignment form for optimization.

**Algorithm**: Standard SSA construction with dominance frontiers
1. Insert PHI nodes at dominance frontiers
2. Rename variables to SSA form
3. Maintain def-use chains

**Example Transformation**:
```
// Before SSA
x = 1
if condition:
    x = 2
print x

// After SSA
x_0 = 1
if condition:
    x_1 = 2
x_2 = PHI(x_0, x_1)
print x_2
```

**Interface**:
```cpp
class SSAConverter {
    struct DominatorTree {
        std::vector<int> idom;  // Immediate dominator
        std::vector<std::vector<int>> children;
        std::vector<std::set<int>> frontiers;
    };
    
public:
    // Convert function to SSA form
    void convertToSSA(IRFunction& func);
    
private:
    // Build dominator tree
    DominatorTree buildDominatorTree(const IRFunction& func);
    
    // Insert PHI nodes
    void insertPhiNodes(IRFunction& func, const DominatorTree& dt);
    
    // Rename variables
    void renameVariables(IRFunction& func, const DominatorTree& dt);
};
```

**Data Structures**:
```cpp
// PHI node instruction
struct PhiInstruction : IRInstruction {
    IRRef result;
    std::vector<std::pair<IRRef, int>> incoming;  // (value, block_id)
};
```

### 7. Optimization Passes (IR Layer)

**Purpose**: Apply standard compiler optimizations to SSA-form IR.

#### 7.1 Constant Folding

**Algorithm**: Single pass over instructions, evaluate constant operations
```cpp
class ConstantFolder {
public:
    bool run(IRFunction& func);
    
private:
    IRValue fold(IROpcode op, const IRValue& left, const IRValue& right);
};
```

**Example**:
```
// Before
t0 = 2
t1 = 3
t2 = add t0, t1
t3 = mul t2, 4

// After
t2 = 5
t3 = 20
```

#### 7.2 Dead Code Elimination

**Algorithm**: Mark-and-sweep
1. Mark all instructions with side effects (I/O, calls, stores)
2. Mark all instructions used by marked instructions
3. Sweep unmarked instructions

```cpp
class DeadCodeEliminator {
public:
    bool run(IRFunction& func);
    
private:
    bool hasSideEffects(const IRInstruction& instr);
    void mark(IRFunction& func, std::vector<bool>& live);
    void sweep(IRFunction& func, const std::vector<bool>& live);
};
```

#### 7.3 Copy Propagation

**Algorithm**: Replace uses of copied values with original
```cpp
class CopyPropagator {
public:
    bool run(IRFunction& func);
    
private:
    std::unordered_map<int, IRRef> copyMap;  // temp_id → source
};
```

**Example**:
```
// Before
t0 = x
t1 = add t0, 1
t2 = mul t0, 2

// After
t1 = add x, 1
t2 = mul x, 2
```

#### 7.4 Function Inlining

**Algorithm**: Replace call sites with function body
```cpp
class Inliner {
    static constexpr int INLINE_THRESHOLD = 20;  // instructions
    
public:
    bool run(IRProgram& program);
    
private:
    bool shouldInline(const IRFunction& func, int callCount);
    void inlineCall(IRFunction& caller, const IRInstruction& call, 
                    const IRFunction& callee);
};
```

**Heuristics**:
- Inline functions with <20 instructions
- Inline functions called exactly once (regardless of size)
- Never inline recursive functions

### 8. Register Allocator (Backend)

**Purpose**: Map virtual registers (temporaries) to physical CPU registers.

**Algorithm**: Linear scan register allocation (simpler than graph coloring, nearly as effective)

**Interface**:
```cpp
class RegisterAllocator {
    struct LiveInterval {
        int tempId;
        int start;  // First use
        int end;    // Last use
    };
    
public:
    // Allocate registers for function
    RegisterAssignment allocate(const IRFunction& func);
    
private:
    std::vector<LiveInterval> computeLiveIntervals(const IRFunction& func);
    void linearScan(std::vector<LiveInterval>& intervals, 
                    RegisterAssignment& assignment);
};

struct RegisterAssignment {
    std::unordered_map<int, int> tempToReg;  // temp_id → physical register
    std::unordered_map<int, int> spillSlots;  // temp_id → stack offset
};
```

**Physical Registers (x86-64)**:
```cpp
enum class X64Register {
    RAX, RBX, RCX, RDX, RSI, RDI, RBP, RSP,
    R8, R9, R10, R11, R12, R13, R14, R15
};

// Calling convention
constexpr X64Register ARG_REGS[] = {RDI, RSI, RDX, RCX, R8, R9};
constexpr X64Register CALLER_SAVED[] = {RAX, RCX, RDX, RSI, RDI, R8, R9, R10, R11};
constexpr X64Register CALLEE_SAVED[] = {RBX, R12, R13, R14, R15};
```

**Spilling**: When physical registers are exhausted, spill to stack
```
// Before allocation
t0 = add t1, t2
t3 = mul t0, t4

// After allocation (t0 spilled)
rax = add rbx, rcx
[rbp-8] = rax      // spill t0
rdx = [rbp-8]      // reload t0
rdx = mul rdx, rsi
```

### 9. Machine IR (Backend)

**Purpose**: Architecture-specific IR before final code generation.

**Data Structures**:
```cpp
// Machine instruction
struct MachineInstr {
    enum class Opcode {
        MOV, ADD, SUB, MUL, DIV,
        CMP, JMP, JE, JNE, JL, JG,
        CALL, RET, PUSH, POP,
        LOAD, STORE
    };
    
    Opcode opcode;
    std::vector<MachineOperand> operands;
};

// Machine operand
struct MachineOperand {
    enum class Kind { REG, IMM, MEM, LABEL };
    Kind kind;
    int value;  // Register number, immediate value, or label ID
    int offset; // For memory operands
};

// Machine function
struct MachineFunction {
    std::string name;
    std::vector<MachineInstr> instructions;
    int stackFrameSize;
    std::vector<int> usedRegisters;
};
```

**Lowering from IR to Machine IR**:
```cpp
class MachineIRBuilder {
public:
    MachineFunction lower(const IRFunction& func, 
                          const RegisterAssignment& regAlloc);
    
private:
    void lowerInstruction(const IRInstruction& ir, 
                          std::vector<MachineInstr>& out);
};
```

### 10. Direct Machine Code Generator (Backend)

**Purpose**: Generate machine code bytes directly without external assembler.

**Interface**:
```cpp
class X64CodeGenerator {
public:
    std::vector<uint8_t> generate(const MachineFunction& func);
    
private:
    void emitInstruction(const MachineInstr& instr);
    void emitModRM(int mod, int reg, int rm);
    void emitSIB(int scale, int index, int base);
    void emitImmediate(int64_t value, int size);
    
    std::vector<uint8_t> code;
    std::unordered_map<int, size_t> labelPositions;
};
```

**x86-64 Instruction Encoding**:
```
Instruction format:
[Prefixes] [REX] [Opcode] [ModR/M] [SIB] [Displacement] [Immediate]

Example: mov rax, rbx
  REX.W = 1 (64-bit operand)
  Opcode = 0x89 (MOV r/m64, r64)
  ModR/M = 11 011 000 (register-direct, rbx, rax)
  Bytes: 48 89 D8
```

**Binary Format Generation**:
```cpp
class ELFGenerator {
public:
    std::vector<uint8_t> generate(const std::vector<MachineFunction>& funcs);
    
private:
    void emitELFHeader();
    void emitProgramHeaders();
    void emitSectionHeaders();
    void emitTextSection(const std::vector<MachineFunction>& funcs);
    void emitDataSection();
    void emitSymbolTable();
};
```

## Data Models

### IR Program Structure

```cpp
struct IRProgram {
    SymbolTable symbols;                    // Shared symbol table
    std::vector<IRFunction> functions;      // All functions
    std::vector<IRInstruction> globalInit;  // Global initialization
    std::string backend;                    // Target backend
    Arena arena;                            // Memory allocator
};
```

### Symbol Table

```cpp
class SymbolTable {
    struct Symbol {
        std::string name;
        IRType type;
        int scope;  // Scope depth
    };
    
    std::vector<Symbol> symbols;
    std::unordered_map<std::string, std::vector<int>> nameToIds;  // Name → [ids]
    int currentScope;
    
public:
    int intern(const std::string& name, IRType type);
    int lookup(const std::string& name) const;
    void enterScope();
    void exitScope();
};
```

### Type System

```cpp
enum class IRType {
    VOID, INT, FLOAT, STRING, BOOL,
    LIST, TUPLE, DICT,
    FUNCTION, POINTER
};

struct TypeInfo {
    IRType base;
    std::vector<TypeInfo> params;  // For generic types (List<Int>, etc.)
};
```

### Optimization Pass Manager

```cpp
class PassManager {
    std::vector<std::unique_ptr<Pass>> passes;
    
public:
    void addPass(std::unique_ptr<Pass> pass);
    bool run(IRProgram& program);
    
    // Standard optimization levels
    static PassManager createO0();  // No optimization
    static PassManager createO1();  // Basic (constant fold, DCE)
    static PassManager createO2();  // Aggressive (+ copy prop, inline)
    static PassManager createO3();  // Maximum (+ loop unroll, vectorize)
};

class Pass {
public:
    virtual bool run(IRProgram& program) = 0;
    virtual const char* name() const = 0;
};
```

## Error Handling

### Error Recovery in Parser

**Strategy**: Synchronization tokens for error recovery

```cpp
class Parser {
    std::vector<SyntaxError> errors;
    static constexpr int MAX_ERRORS = 10;
    
    void reportError(const std::string& msg, int line, int col);
    void synchronize();  // Skip to next synchronization point
    
    // Synchronization tokens
    bool isSyncToken(TokenType type) {
        return type == TokenType::NEWLINE || 
               type == TokenType::DEDENT ||
               type == TokenType::KW_IF ||
               type == TokenType::KW_FOR ||
               type == TokenType::KW_WHILST;
    }
};
```

**Error Types**:
```cpp
struct SyntaxError {
    std::string message;
    int line;
    int column;
    std::string context;  // Surrounding source code
};

struct SemanticError {
    std::string message;
    int line;
    std::string symbol;
};

struct CodeGenError {
    std::string message;
    std::string function;
    int instruction;
};
```

### Debug Information Generation

**DWARF Format**:
```cpp
class DWARFGenerator {
public:
    void addLineInfo(int address, int line, int column);
    void addVariable(const std::string& name, int address, IRType type);
    void addFunction(const std::string& name, int startAddr, int endAddr);
    
    std::vector<uint8_t> generateDebugInfo();
    
private:
    struct LineEntry {
        int address;
        int line;
        int column;
    };
    
    std::vector<LineEntry> lineTable;
    std::vector<VariableInfo> variables;
    std::vector<FunctionInfo> functions;
};
```

## Testing Strategy

### Unit Testing

**Test Categories**:
1. **Parser Tests**: Verify expression parsing correctness
   - Binary operators with correct precedence
   - Unary operators
   - Function calls and indexing
   - Nested expressions

2. **IR Generation Tests**: Verify IR correctness
   - Integer reference generation
   - CFG lowering correctness
   - SSA conversion correctness

3. **Optimization Tests**: Verify optimization correctness
   - Constant folding produces correct results
   - DCE removes only dead code
   - Copy propagation preserves semantics
   - Inlining produces equivalent code

4. **Register Allocation Tests**: Verify register assignment
   - All temporaries assigned
   - Calling convention respected
   - Spilling works correctly

5. **Code Generation Tests**: Verify machine code correctness
   - Instructions encoded correctly
   - Binary format valid
   - Executables run correctly

**Test Framework**: Use existing AC test suite with new performance benchmarks

### Integration Testing

**End-to-End Tests**:
- Compile and run all existing AC test programs
- Verify output matches expected results
- Measure compilation time and execution time
- Compare against current implementation

**Benchmark Suite**:
```
benchmarks/
├── fibonacci.ac       # Recursive algorithm
├── factorial.ac       # Recursive algorithm
├── array_sum.ac       # Iterative array operations
├── matrix_mult.ac     # Nested loops
├── quicksort.ac       # Complex algorithm
├── parser.ac          # Real-world program
├── compiler.ac        # Self-hosting test
└── pong.ac            # Game logic
```

**Performance Metrics**:
- Compilation time (ms)
- Execution time (ms)
- Binary size (bytes)
- Memory usage (MB)

**Acceptance Criteria**:
- All tests pass with identical output
- Compilation 5x faster than current
- Execution within 2x of gcc -O2
- Binary size reasonable (<2x of C equivalent)

### Property-Based Testing Assessment

This feature involves compiler implementation with complex transformations (parsing, IR generation, optimization, code generation). The core operations are:

1. **Parsing**: Transforming token streams to AST
2. **IR Generation**: Transforming AST to IR
3. **Optimization**: Transforming IR to optimized IR
4. **Code Generation**: Transforming IR to machine code

These transformations have **universal properties** that should hold across all valid inputs:
- **Round-trip properties**: Parse → Pretty-print → Parse should preserve semantics
- **Semantic preservation**: All transformations must preserve program semantics
- **Invariant preservation**: Type safety, SSA form properties, register allocation constraints

Property-based testing **IS appropriate** for this feature because:
- We're testing pure transformations with clear input/output behavior
- There are universal properties (semantic preservation, round-trips)
- The input space is large (all valid AC programs)
- We're testing algorithms and data transformations, not infrastructure

Therefore, we will include Correctness Properties and use property-based testing.



## Correctness Properties

*A property is a characteristic or behavior that should hold true across all valid executions of a system—essentially, a formal statement about what the system should do. Properties serve as the bridge between human-readable specifications and machine-verifiable correctness guarantees.*

### Property Reflection

After analyzing all 125 acceptance criteria, I identified the following redundancies:

**Redundancy Group 1**: Requirements 1.3 and 1.4 both test that the IR layer doesn't parse strings. These can be combined into a single property.

**Redundancy Group 2**: Requirements 2.1, 2.2, 2.3, and 2.4 all test that IR uses integer references. These can be combined into a comprehensive property about integerized IR.

**Redundancy Group 3**: Requirements 4.1, 4.2, 4.3, and 4.4 all test that control flow is lowered to CFG. These can be combined into a single property about CFG lowering.

**Redundancy Group 4**: Requirements 6.2, 6.3, 6.4, and 6.5 all test constant folding for different operators. These can be combined into a comprehensive constant folding property.

**Redundancy Group 5**: Requirements 14.1, 14.2, 14.3, 14.4, and 14.5 all test AST node structure. These can be combined into a property about structured AST nodes.

After eliminating redundancies, we have the following unique correctness properties:

### Property 1: Expression Parsing Produces Structured AST

*For any* valid AC expression, parsing SHALL produce a structured AST node (BinaryExpr, UnaryExpr, CallExpr, IndexExpr, or LiteralExpr) with typed operands, not a string representation.

**Validates: Requirements 1.1, 1.2, 14.1, 14.2, 14.3, 14.4, 14.5**

### Property 2: Single-Pass Expression Parsing

*For any* AC program, each expression SHALL be parsed exactly once during the frontend phase, with no re-parsing in subsequent compilation stages.

**Validates: Requirements 1.5**

### Property 3: IR Layer String-Free Operation

*For any* AST, IR generation SHALL consume structured expression nodes without invoking string parsing functions.

**Validates: Requirements 1.3, 1.4**

### Property 4: Integerized IR References

*For any* generated IR, all references (temporaries, labels, variables) SHALL use integer IDs, not string representations, with string conversion occurring only in toString() methods for debugging.

**Validates: Requirements 2.1, 2.2, 2.3, 2.4, 2.5**

### Property 5: Flat Array IR Storage

*For any* IR function, instructions SHALL be stored in contiguous flat arrays (std::vector) without using std::unique_ptr or std::shared_ptr for individual instructions.

**Validates: Requirements 3.1, 3.3**

### Property 6: Arena Allocation for IR

*For any* IR generation, all IR memory allocations SHALL use an Arena allocator with block sizes >= 64KB, and all instructions in a function SHALL be allocated from the same arena.

**Validates: Requirements 3.2, 3.4, 3.5**

### Property 7: Immediate CFG Lowering

*For any* high-level control flow construct (IF, WHILST, FOR), the IR layer SHALL emit explicit jump instructions and labels immediately, with no high-level control flow nodes preserved in the IR.

**Validates: Requirements 4.1, 4.2, 4.3, 4.4, 4.5**

### Property 8: SSA Form Conversion

*For any* IR function, SSA conversion SHALL create unique SSA variables for each assignment, insert PHI nodes at control flow merge points, and maintain a valid mapping from original variables to SSA variables.

**Validates: Requirements 5.1, 5.2, 5.3, 5.4**

### Property 9: Constant Folding Completeness

*For any* arithmetic, comparison, or logical operation with constant operands, the optimization pass SHALL evaluate the operation at compile time and replace it with a constant load instruction.

**Validates: Requirements 6.1, 6.2, 6.3, 6.4, 6.5**

### Property 10: Dead Code Elimination Correctness

*For any* IR function, dead code elimination SHALL remove instructions whose results are never used and have no side effects, remove unreachable basic blocks, and preserve all instructions with side effects (I/O, calls, memory writes).

**Validates: Requirements 7.1, 7.2, 7.3, 7.4**

### Property 11: Dead Code Elimination Convergence

*For any* IR function, dead code elimination SHALL iterate until no more dead code is found (fixed-point convergence).

**Validates: Requirements 7.5**

### Property 12: Copy Propagation Correctness

*For any* copy instruction (x = y) in SSA form, copy propagation SHALL replace uses of x with y where valid, respect SSA form constraints, not propagate across PHI nodes, and remove dead copies.

**Validates: Requirements 8.1, 8.2, 8.3, 8.4**

### Property 13: Function Inlining Heuristics

*For any* function, inlining SHALL occur if the function has <20 instructions OR is called exactly once, and SHALL NOT occur for recursive functions, with variable renaming to avoid conflicts.

**Validates: Requirements 9.1, 9.2, 9.3, 9.4, 9.5**

### Property 14: Register Allocation Completeness

*For any* IR function, register allocation SHALL assign physical registers to frequently-used temporaries, spill to stack when registers are exhausted, and respect calling convention constraints.

**Validates: Requirements 10.1, 10.3, 10.4**

### Property 15: No External Compiler Invocation

*For any* compilation, the backend SHALL NOT invoke external compilers via system() or exec() calls, and SHALL generate assembly or machine code directly.

**Validates: Requirements 11.1, 11.2, 11.3, 11.4**

### Property 16: Machine Code Generation Correctness

*For any* IR function, the machine code generator SHALL emit valid x86-64 instructions encoded according to the ISA specification.

**Validates: Requirements 12.1, 12.2**

### Property 17: Binary Format Generation

*For any* compiled program, the backend SHALL generate valid binary format (ELF on Linux, Mach-O on macOS, PE on Windows) that can be executed by the operating system.

**Validates: Requirements 12.3, 12.4, 12.5**

### Property 18: Pratt Parser Precedence

*For any* expression with mixed operator precedence, the Pratt parser SHALL parse it correctly according to the precedence table, supporting all binary operators (+, -, *, /, %, @, <, >, <=, >=, ==, !=) and unary operators (-, NOT).

**Validates: Requirements 13.1, 13.2, 13.3, 13.4**

### Property 19: Symbol Table Integer Indexing

*For any* program with variables, the symbol table SHALL map variable names to sequential integer indices starting from 0, support nested scopes, and use integer indices for all variable references in IR.

**Validates: Requirements 15.1, 15.2, 15.3, 15.4**

### Property 20: Incremental Compilation Caching

*For any* program compiled twice without source changes, the second compilation SHALL use cached intermediate results and complete faster than the first compilation.

**Validates: Requirements 20.4**

### Property 21: Parser Round-Trip Preservation

*For any* valid AC program, parsing then pretty-printing then parsing SHALL produce an equivalent AST that preserves program semantics.

**Validates: Requirements 21.1, 21.2, 21.3, 21.4**

### Property 22: IR Serialization Round-Trip

*For any* IR program, serializing to binary format then deserializing SHALL produce an equivalent IR that preserves all information including debug metadata, with architecture-independent portability.

**Validates: Requirements 22.1, 22.2, 22.3, 22.4, 22.5**

### Property 23: Debug Information Completeness

*For any* program compiled with -g flag, the generated binary SHALL include DWARF debug information with source file names, line numbers, variable names and types, and function names and boundaries.

**Validates: Requirements 24.1, 24.2, 24.3, 24.4**

### Property 24: Parser Error Recovery

*For any* program with syntax errors, the parser SHALL report at least 10 errors with line numbers, column numbers, and context, attempt recovery at synchronization tokens (newlines, dedents), and produce a partial AST.

**Validates: Requirements 25.1, 25.2, 25.3, 25.4, 25.5**

## Testing Strategy

### Dual Testing Approach

This feature requires both **unit tests** and **property-based tests** for comprehensive coverage:

**Unit Tests** focus on:
- Specific examples of each transformation (parsing, IR generation, optimization)
- Edge cases (empty programs, single statements, deeply nested structures)
- Error conditions (invalid syntax, type errors, resource exhaustion)
- Integration points between pipeline stages
- Platform-specific behavior (ELF vs Mach-O vs PE generation)

**Property-Based Tests** focus on:
- Universal properties that hold across all valid inputs
- Semantic preservation through all transformations
- Round-trip properties (parse→print→parse, serialize→deserialize)
- Optimization correctness (constant folding, DCE, copy propagation)
- Invariant preservation (SSA form, type safety, calling conventions)

### Property-Based Testing Configuration

**Library**: Use **fast-check** (JavaScript/TypeScript) or **Hypothesis** (Python) or **QuickCheck** (Haskell) depending on test implementation language. For C++ tests, use **RapidCheck**.

**Configuration**:
- Minimum 100 iterations per property test
- Each property test references its design document property number
- Tag format: `Feature: compiler-performance-optimization, Property {number}: {property_text}`

**Example Property Test** (using RapidCheck for C++):
```cpp
// Property 1: Expression Parsing Produces Structured AST
RC_GTEST_PROP(ParserProperties, ExpressionParsingProducesStructuredAST,
              (const std::string& expr)) {
    // Feature: compiler-performance-optimization
    // Property 1: Expression Parsing Produces Structured AST
    
    // Generate valid AC expression
    auto tokens = lex(expr);
    auto ast = parse(tokens);
    
    // Verify: AST node is structured (not string-based)
    RC_ASSERT(ast->type == NodeType::BinaryExpr ||
              ast->type == NodeType::UnaryExpr ||
              ast->type == NodeType::CallExpr ||
              ast->type == NodeType::IndexExpr ||
              ast->type == NodeType::LiteralExpr);
    
    // Verify: Operands are typed nodes, not strings
    if (ast->type == NodeType::BinaryExpr) {
        RC_ASSERT(ast->children.size() == 2);
        RC_ASSERT(ast->children[0] != nullptr);
        RC_ASSERT(ast->children[1] != nullptr);
    }
}
```

### Unit Test Categories

**1. Parser Tests**
- Binary operator precedence (2 + 3 * 4 = 14, not 20)
- Unary operator parsing (-5, NOT true)
- Function call parsing (func(a, b, c))
- Array indexing (arr[0], arr[i+1])
- Nested expressions ((a + b) * (c - d))
- Edge cases (empty expressions, single tokens)

**2. IR Generation Tests**
- Integer reference generation (temporaries 0,1,2..., labels 0,1,2...)
- CFG lowering (IF→jumps, WHILST→loops, FOR→iterators)
- Symbol table construction (variable name→index mapping)
- Arena allocation (all instructions from same arena)
- Expression lowering (AST→IR without string parsing)

**3. SSA Conversion Tests**
- Multiple assignment handling (x=1; x=2 → x_0=1; x_1=2)
- PHI node insertion (merge points get PHI nodes)
- Variable renaming (unique SSA variables)
- Dominator tree construction
- Def-use chain maintenance

**4. Optimization Tests**
- Constant folding (2+3→5, true AND false→false)
- Dead code elimination (unused results removed)
- Copy propagation (x=y; z=x → z=y)
- Function inlining (small functions inlined)
- Fixed-point convergence (optimizations iterate to convergence)

**5. Register Allocation Tests**
- Physical register assignment (temps→registers)
- Spilling (excess temps→stack)
- Calling convention (caller-saved vs callee-saved)
- Live interval computation
- Register pressure handling

**6. Code Generation Tests**
- x86-64 instruction encoding (MOV, ADD, SUB, etc.)
- Binary format generation (ELF, Mach-O, PE)
- Calling convention implementation
- Stack frame layout
- Debug information generation

**7. End-to-End Tests**
- Compile and run all existing AC test programs
- Verify output matches expected results
- Measure compilation time and execution time
- Compare against current implementation
- Test all backends (PY, JS, C, C++, RS, GO, Java, HTML, V, ASM, BNY)

### Benchmark Suite

**Location**: `benchmarks/` directory

**Programs**:
1. `fibonacci.ac` - Recursive fibonacci (tests function calls, recursion)
2. `factorial.ac` - Recursive factorial (tests recursion, integer arithmetic)
3. `array_sum.ac` - Iterative array summation (tests loops, array access)
4. `matrix_mult.ac` - Matrix multiplication (tests nested loops, 2D arrays)
5. `quicksort.ac` - Quicksort algorithm (tests recursion, array manipulation)
6. `bubble_sort.ac` - Bubble sort (tests nested loops, comparisons)
7. `string_concat.ac` - String concatenation (tests string operations)
8. `hash_table.ac` - Hash table operations (tests dictionaries, hashing)
9. `parser.ac` - Simple expression parser (tests real-world logic)
10. `pong.ac` - Pong game logic (tests game loop, collision detection)

**Metrics Collected**:
- Compilation time (ms)
- Execution time (ms)
- Binary size (bytes)
- Memory usage during compilation (MB)
- Number of IR instructions generated
- Number of optimizations applied
- Register allocation statistics

**Performance Targets**:
- Compilation: 5x faster than current implementation
- Execution: Within 2x of gcc -O2 (geometric mean across all benchmarks)
- Specific targets:
  - Fibonacci: Within 2x of C
  - Factorial: Within 2x of C
  - Array operations: Within 1.5x of C

### Integration Testing

**Backward Compatibility Tests**:
- All existing test programs compile without errors
- All existing test programs produce identical output
- All language features work (IF/WHILST/FOR, functions, lists, tuples, dicts)
- All backends work (PY, JS, C, C++, RS, GO, Java, HTML, V, ASM, BNY)
- Command-line interface unchanged

**Pipeline Tests**:
- Each pipeline stage can be stopped for debugging (--stop-after-parse, --stop-after-ir, etc.)
- Intermediate representations can be saved to files (--save-ast, --save-ir, etc.)
- Pipeline completes in <100ms for 1000-line programs

**Cross-Platform Tests**:
- Linux: ELF binary generation and execution
- macOS: Mach-O binary generation and execution
- Windows: PE binary generation and execution
- Architecture portability: x86-64, ARM64 (future)

### Test Execution

**Continuous Integration**:
- Run all unit tests on every commit
- Run property-based tests (100 iterations) on every commit
- Run benchmark suite on every pull request
- Run full integration tests before release

**Performance Regression Detection**:
- Track compilation time over commits
- Track execution time over commits
- Alert on >10% regression
- Require justification for performance regressions

**Test Coverage Target**: 90% code coverage for all new components (parser, IR builder, optimizer, code generator)



## Implementation Roadmap

### Phase 1: Frontend Transformation (Weeks 1-2)

**Goal**: Replace string-based expression handling with structured AST nodes.

**Tasks**:
1. Implement Pratt parser for expressions
2. Add BinaryExpr, UnaryExpr, CallExpr, IndexExpr, LiteralExpr AST nodes
3. Update parser to use Pratt parsing for all expressions
4. Add precedence table and operator handling
5. Write unit tests for expression parsing
6. Write property tests for parsing correctness

**Deliverables**:
- Pratt parser implementation
- Structured expression AST nodes
- Parser tests (unit + property-based)
- Documentation of precedence rules

**Success Criteria**:
- All existing tests pass with new parser
- Property tests verify structured AST generation
- No string-based expressions in AST

### Phase 2: Integerized IR (Weeks 3-4)

**Goal**: Replace string-based IR with integer-based references.

**Tasks**:
1. Implement IRRef with integer IDs
2. Implement SymbolTable with integer indexing
3. Update IR generation to use integer references
4. Implement Arena allocator
5. Update IR storage to use flat arrays with arena allocation
6. Write unit tests for IR generation
7. Write property tests for integerized IR

**Deliverables**:
- IRRef implementation with integer IDs
- SymbolTable with integer indexing
- Arena allocator
- Updated IR generation
- IR tests (unit + property-based)

**Success Criteria**:
- All IR references use integers
- Arena allocation for all IR memory
- Property tests verify integer-only references
- Memory usage reduced by 30%

### Phase 3: CFG Lowering (Weeks 5-6)

**Goal**: Lower control flow to explicit jumps and labels immediately.

**Tasks**:
1. Implement CFG builder
2. Add jump and label instructions to IR
3. Lower IF statements to conditional jumps
4. Lower WHILST loops to loop labels and back-edges
5. Lower FOR loops to iterator operations and jumps
6. Write unit tests for CFG lowering
7. Write property tests for CFG correctness

**Deliverables**:
- CFG builder implementation
- Jump/label IR instructions
- Control flow lowering for IF/WHILST/FOR
- CFG tests (unit + property-based)

**Success Criteria**:
- No high-level control flow in IR
- All control flow explicit via jumps
- Property tests verify CFG correctness
- Optimization passes can operate on uniform IR

### Phase 4: SSA Conversion (Weeks 7-8)

**Goal**: Implement SSA form conversion for optimization.

**Tasks**:
1. Implement dominator tree construction
2. Implement PHI node insertion
3. Implement variable renaming
4. Add SSA conversion pass
5. Write unit tests for SSA conversion
6. Write property tests for SSA correctness

**Deliverables**:
- SSA converter implementation
- Dominator tree builder
- PHI node support
- SSA tests (unit + property-based)

**Success Criteria**:
- Valid SSA form generated
- PHI nodes at merge points
- Unique SSA variables for each assignment
- Property tests verify SSA properties

### Phase 5: Optimization Passes (Weeks 9-11)

**Goal**: Implement standard compiler optimizations.

**Tasks**:
1. Implement constant folding pass
2. Implement dead code elimination pass
3. Implement copy propagation pass
4. Implement function inlining pass
5. Implement pass manager with optimization levels
6. Write unit tests for each optimization
7. Write property tests for optimization correctness

**Deliverables**:
- Constant folding implementation
- Dead code elimination implementation
- Copy propagation implementation
- Function inlining implementation
- Pass manager with -O0/-O1/-O2/-O3
- Optimization tests (unit + property-based)

**Success Criteria**:
- All optimizations preserve semantics
- Property tests verify correctness
- Benchmark performance improves by 50%
- Optimization levels work correctly

### Phase 6: Register Allocation (Weeks 12-13)

**Goal**: Implement register allocation for backend.

**Tasks**:
1. Implement live interval computation
2. Implement linear scan register allocation
3. Implement spilling to stack
4. Add calling convention support
5. Write unit tests for register allocation
6. Write property tests for allocation correctness

**Deliverables**:
- Register allocator implementation
- Live interval analysis
- Spilling support
- Calling convention handling
- Register allocation tests (unit + property-based)

**Success Criteria**:
- All temporaries assigned (register or stack)
- Calling convention respected
- Property tests verify allocation correctness
- 80% register utilization achieved

### Phase 7: Machine Code Generation (Weeks 14-16)

**Goal**: Implement direct machine code generation.

**Tasks**:
1. Implement Machine IR layer
2. Implement x86-64 instruction encoder
3. Implement ELF binary generator (Linux)
4. Implement Mach-O binary generator (macOS)
5. Implement PE binary generator (Windows)
6. Write unit tests for code generation
7. Write property tests for encoding correctness

**Deliverables**:
- Machine IR implementation
- x86-64 code generator
- Binary format generators (ELF/Mach-O/PE)
- Code generation tests (unit + property-based)

**Success Criteria**:
- Valid machine code generated
- Binaries execute correctly on all platforms
- Property tests verify encoding correctness
- No external compiler invocation

### Phase 8: Debug Information (Week 17)

**Goal**: Add debug information generation.

**Tasks**:
1. Implement DWARF generator
2. Add line number tracking
3. Add variable information
4. Add function boundaries
5. Write tests for debug info

**Deliverables**:
- DWARF generator implementation
- Debug information in binaries
- Debug info tests

**Success Criteria**:
- gdb/lldb can debug compiled programs
- Source-level debugging works
- Variable inspection works

### Phase 9: Error Handling (Week 18)

**Goal**: Improve error reporting and recovery.

**Tasks**:
1. Implement error recovery in parser
2. Add synchronization token handling
3. Improve error messages
4. Add partial AST generation
5. Write tests for error handling

**Deliverables**:
- Error recovery implementation
- Improved error messages
- Partial AST generation
- Error handling tests

**Success Criteria**:
- Parser reports multiple errors
- Error messages include context
- Partial AST generated on errors

### Phase 10: Integration and Testing (Weeks 19-20)

**Goal**: Integrate all components and validate performance.

**Tasks**:
1. Integrate all pipeline stages
2. Run full test suite
3. Run benchmark suite
4. Measure performance against targets
5. Fix any integration issues
6. Write end-to-end tests

**Deliverables**:
- Fully integrated compiler
- Complete test suite passing
- Benchmark results
- Performance analysis
- Integration tests

**Success Criteria**:
- All existing tests pass
- Compilation 5x faster
- Execution within 2x of gcc -O2
- All backends work
- Backward compatibility maintained

## Risk Analysis

### Technical Risks

**Risk 1: SSA Conversion Complexity**
- **Impact**: High - SSA is critical for optimizations
- **Probability**: Medium - SSA algorithms are well-known but complex
- **Mitigation**: Use proven algorithms (Cytron et al.), extensive testing, property-based tests
- **Contingency**: Fall back to non-SSA optimizations if SSA proves too complex

**Risk 2: Register Allocation Performance**
- **Impact**: Medium - Poor allocation reduces execution speed
- **Probability**: Low - Linear scan is proven effective
- **Mitigation**: Use linear scan (simpler than graph coloring), benchmark against gcc
- **Contingency**: Use simpler allocation strategy if performance inadequate

**Risk 3: Machine Code Generation Correctness**
- **Impact**: High - Incorrect code generation breaks programs
- **Probability**: Medium - x86-64 encoding is complex
- **Mitigation**: Extensive testing, property-based tests, compare against assembler output
- **Contingency**: Fall back to assembly generation + external assembler

**Risk 4: Binary Format Compatibility**
- **Impact**: High - Invalid binaries won't execute
- **Probability**: Low - Binary formats are well-documented
- **Mitigation**: Use existing libraries (libelf, etc.), test on all platforms
- **Contingency**: Use external linker if binary generation fails

**Risk 5: Performance Target Not Met**
- **Impact**: Medium - Feature still useful but less impactful
- **Probability**: Low - Optimizations are proven effective
- **Mitigation**: Benchmark early and often, profile bottlenecks, iterate on optimizations
- **Contingency**: Adjust targets based on realistic measurements

### Schedule Risks

**Risk 1: Underestimated Complexity**
- **Impact**: High - Delays entire project
- **Probability**: Medium - Compiler work is complex
- **Mitigation**: Build in 20% schedule buffer, prioritize critical path items
- **Contingency**: Reduce scope (e.g., skip ARM64 support initially)

**Risk 2: Integration Issues**
- **Impact**: Medium - Delays final delivery
- **Probability**: Low - Incremental integration reduces risk
- **Mitigation**: Integrate continuously, test at each phase
- **Contingency**: Allocate extra time for integration phase

### Dependency Risks

**Risk 1: Platform-Specific Issues**
- **Impact**: Medium - Limits platform support
- **Probability**: Medium - Binary formats differ across platforms
- **Mitigation**: Test on all platforms early, use platform-specific libraries
- **Contingency**: Support fewer platforms initially (Linux only)

**Risk 2: Backward Compatibility Breakage**
- **Impact**: High - Breaks existing code
- **Probability**: Low - Extensive testing prevents this
- **Mitigation**: Run all existing tests continuously, maintain compatibility layer
- **Contingency**: Provide migration guide if breaking changes necessary

## Success Metrics

### Performance Metrics

**Compilation Speed**:
- Target: 5x faster than current implementation
- Measurement: Time from source to binary for benchmark suite
- Baseline: Current implementation average compilation time
- Success: Geometric mean 5x improvement

**Execution Speed**:
- Target: Within 2x of gcc -O2
- Measurement: Execution time for benchmark suite
- Baseline: gcc -O2 compiled equivalent C programs
- Success: Geometric mean within 2x

**Memory Efficiency**:
- Target: 50% reduction in IR memory footprint
- Measurement: Peak memory usage during compilation
- Baseline: Current implementation memory usage
- Success: 50% reduction

### Quality Metrics

**Test Coverage**:
- Target: 90% code coverage
- Measurement: Line coverage for new components
- Success: 90% coverage achieved

**Bug Rate**:
- Target: <5 bugs per 1000 lines of code
- Measurement: Bugs found in testing and production
- Success: Bug rate below target

**Backward Compatibility**:
- Target: 100% of existing tests pass
- Measurement: Existing test suite pass rate
- Success: All tests pass with identical output

### Adoption Metrics

**User Satisfaction**:
- Target: Positive feedback from users
- Measurement: User surveys, issue reports
- Success: >80% positive feedback

**Performance Improvement Reports**:
- Target: Users report faster compilation
- Measurement: User testimonials, benchmarks
- Success: Measurable improvements in real-world usage

## Conclusion

This design transforms the AC compiler from a string-heavy, allocation-intensive implementation into a high-performance compiler achieving C-level execution speeds. The key innovations are:

1. **Pratt parser** for single-pass expression parsing
2. **Integerized IR** with arena allocation for optimal cache locality
3. **Immediate CFG lowering** for uniform optimization substrate
4. **SSA form** enabling powerful optimizations
5. **Direct machine code generation** eliminating external compiler overhead

The implementation follows a phased approach over 20 weeks, with continuous testing and integration. Property-based testing ensures correctness of all transformations, while benchmarking validates performance targets.

The result is a compiler that:
- Compiles 5x faster than the current implementation
- Generates code executing within 2x of gcc -O2
- Maintains 100% backward compatibility
- Supports all existing backends
- Provides a solid foundation for future optimizations

This design addresses all 25 requirements and provides concrete implementation guidance for achieving C-level performance while maintaining the AC language's beginner-friendly syntax and multi-target compilation capabilities.

