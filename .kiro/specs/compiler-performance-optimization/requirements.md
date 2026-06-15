# Requirements Document: High-Performance AC Compiler

## Introduction

This document specifies the requirements for transforming the AC language compiler from its current implementation into a high-performance compiler that achieves C-level execution speed. The current compiler suffers from three major performance bottlenecks: string-heavy expression handling in the frontend, string-based IR references with allocation-heavy emission in the IR layer, and external compiler invocation overhead in the backend. This transformation will eliminate "interpretation disguised as compilation" by implementing a fully integerized IR, arena allocation, SSA-form optimizations, and direct machine code generation.

## Glossary

- **AC_Compiler**: The AC language compiler system
- **Frontend**: The lexer and parser components that produce the AST
- **IR_Layer**: The intermediate representation layer between AST and code generation
- **Backend**: The code generation and assembly/machine code emission components
- **AST**: Abstract Syntax Tree - the parsed representation of source code
- **IR**: Intermediate Representation - the low-level representation between AST and machine code
- **SSA**: Static Single Assignment form - an IR property where each variable is assigned exactly once
- **CFG**: Control Flow Graph - a representation of all paths that might be traversed during program execution
- **IRRef**: IR Reference - a typed reference to values, variables, temporaries, or labels in the IR
- **Arena_Allocator**: A memory allocation strategy that allocates from a contiguous memory region
- **Pratt_Parser**: A top-down operator precedence parser for expressions
- **Register_Allocator**: The component that assigns virtual registers to physical CPU registers
- **Optimization_Pass**: A transformation applied to IR to improve performance
- **Machine_IR**: The final low-level IR before assembly/machine code generation
- **System_Call**: External process invocation (e.g., calling gcc/clang via system())

## Requirements

### Requirement 1: Eliminate String-Based Expression Parsing

**User Story:** As a compiler developer, I want expressions to be parsed once into AST nodes during the frontend phase, so that the IR layer does not need to re-parse string expressions.

#### Acceptance Criteria

1. WHEN the Parser encounters an expression, THE Parser SHALL parse it into a structured AST node using Pratt parsing
2. THE Parser SHALL produce BinaryExpr, UnaryExpr, CallExpr, and IndexExpr AST nodes with typed operands
3. THE IR_Layer SHALL consume structured AST expression nodes without string parsing
4. THE IR_Layer SHALL NOT invoke string parsing functions during IR generation
5. FOR ALL expressions in the source code, parsing SHALL occur exactly once in the Frontend

### Requirement 2: Implement Integerized IR References

**User Story:** As a compiler developer, I want IR references to use integer IDs instead of strings, so that IR operations achieve optimal cache locality and performance.

#### Acceptance Criteria

1. THE IR_Layer SHALL represent temporaries as integer IDs (0, 1, 2, ...) not strings ("t0", "t1", "t2")
2. THE IR_Layer SHALL represent labels as integer IDs (0, 1, 2, ...) not strings ("L0", "L1", "L2")
3. THE IR_Layer SHALL represent variables using a symbol table with integer indices
4. THE IRRef structure SHALL use integer fields for all references
5. WHEN converting IR to text format for debugging, THE IR_Layer SHALL generate string representations only for display purposes

### Requirement 3: Implement Flat IR with Arena Allocation

**User Story:** As a compiler developer, I want IR instructions stored in flat arrays with arena allocation, so that IR generation avoids per-instruction heap allocation overhead.

#### Acceptance Criteria

1. THE IR_Layer SHALL store IRInstruction objects in contiguous flat arrays
2. THE IR_Layer SHALL use an Arena_Allocator for all IR memory allocation
3. THE IR_Layer SHALL NOT use std::unique_ptr or std::shared_ptr for individual IR instructions
4. THE Arena_Allocator SHALL allocate memory in large blocks (minimum 64KB per block)
5. WHEN generating IR for a function, THE IR_Layer SHALL allocate all instructions from the same arena

### Requirement 4: Lower Control Flow Early to CFG

**User Story:** As a compiler developer, I want control flow constructs (IF/WHILST/FOR) lowered to CFG immediately during IR generation, so that optimization passes operate on a uniform jump-based representation.

#### Acceptance Criteria

1. WHEN the IR_Layer encounters an IF statement, THE IR_Layer SHALL emit conditional jumps and labels immediately
2. WHEN the IR_Layer encounters a WHILST loop, THE IR_Layer SHALL emit loop labels and back-edges immediately
3. WHEN the IR_Layer encounters a FOR loop, THE IR_Layer SHALL emit iterator initialization, condition checks, and loop jumps immediately
4. THE IR_Layer SHALL NOT preserve high-level control flow constructs in the IR
5. THE CFG SHALL be explicit in the IR through JUMP, JUMP_IF_TRUE, and JUMP_IF_FALSE instructions

### Requirement 5: Implement SSA Form Conversion

**User Story:** As a compiler developer, I want the IR to support SSA form, so that optimization passes can perform dataflow analysis and register allocation efficiently.

#### Acceptance Criteria

1. THE IR_Layer SHALL provide an SSA conversion pass that transforms IR into SSA form
2. WHEN a variable is assigned multiple times, THE SSA conversion pass SHALL create unique SSA variables for each assignment
3. WHEN control flow merges, THE SSA conversion pass SHALL insert PHI nodes
4. THE IR_Layer SHALL maintain a mapping from original variables to SSA variables
5. THE SSA form SHALL enable constant propagation, dead code elimination, and copy propagation optimizations

### Requirement 6: Implement Constant Folding Optimization

**User Story:** As a compiler developer, I want constant expressions evaluated at compile time, so that runtime performance improves by eliminating redundant computations.

#### Acceptance Criteria

1. WHEN the Optimization_Pass encounters an arithmetic operation with constant operands, THE Optimization_Pass SHALL evaluate the operation at compile time
2. THE Optimization_Pass SHALL fold integer arithmetic (+, -, *, /, %)
3. THE Optimization_Pass SHALL fold comparison operations (<, >, <=, >=, ==, !=)
4. THE Optimization_Pass SHALL fold logical operations (AND, OR, NOT)
5. THE Optimization_Pass SHALL replace the operation with a constant load instruction

### Requirement 7: Implement Dead Code Elimination

**User Story:** As a compiler developer, I want unused code removed from the IR, so that generated binaries are smaller and execution is faster.

#### Acceptance Criteria

1. THE Optimization_Pass SHALL identify instructions whose results are never used
2. THE Optimization_Pass SHALL remove instructions with no side effects that produce unused values
3. THE Optimization_Pass SHALL remove unreachable basic blocks
4. THE Optimization_Pass SHALL preserve instructions with side effects (I/O, function calls, memory writes)
5. THE Optimization_Pass SHALL iterate until no more dead code is found

### Requirement 8: Implement Copy Propagation

**User Story:** As a compiler developer, I want redundant copy operations eliminated, so that register pressure is reduced and performance improves.

#### Acceptance Criteria

1. WHEN the Optimization_Pass encounters a copy instruction (x = y), THE Optimization_Pass SHALL replace uses of x with y where valid
2. THE Optimization_Pass SHALL respect SSA form constraints
3. THE Optimization_Pass SHALL NOT propagate copies across basic block boundaries where PHI nodes exist
4. THE Optimization_Pass SHALL remove copy instructions that become dead after propagation
5. THE Optimization_Pass SHALL reduce the total number of IR instructions by at least 10% on typical programs

### Requirement 9: Implement Inline Expansion for Small Functions

**User Story:** As a compiler developer, I want small functions inlined at call sites, so that function call overhead is eliminated.

#### Acceptance Criteria

1. THE Optimization_Pass SHALL identify functions with fewer than 20 IR instructions as inline candidates
2. WHEN a function is called and is an inline candidate, THE Optimization_Pass SHALL replace the call with the function body
3. THE Optimization_Pass SHALL rename variables in the inlined body to avoid conflicts
4. THE Optimization_Pass SHALL NOT inline recursive functions
5. THE Optimization_Pass SHALL inline functions called exactly once regardless of size

### Requirement 10: Implement Register Allocation

**User Story:** As a compiler developer, I want virtual registers mapped to physical CPU registers, so that generated code minimizes memory access.

#### Acceptance Criteria

1. THE Register_Allocator SHALL assign physical registers to frequently-used virtual registers
2. THE Register_Allocator SHALL use graph coloring or linear scan algorithm
3. WHEN physical registers are exhausted, THE Register_Allocator SHALL spill variables to stack
4. THE Register_Allocator SHALL respect calling convention constraints (caller-saved vs callee-saved)
5. THE Register_Allocator SHALL achieve at least 80% register utilization on typical programs

### Requirement 11: Replace External Compiler Invocation

**User Story:** As a compiler developer, I want to eliminate system() calls to external compilers, so that compilation is faster and more portable.

#### Acceptance Criteria

1. THE Backend SHALL NOT invoke external compilers via system() or exec() calls
2. THE Backend SHALL generate assembly code directly to files
3. THE Backend SHALL invoke the system assembler and linker through library APIs (not shell commands)
4. WHERE native binary output is required, THE Backend SHALL use an integrated assembler library
5. THE Backend SHALL reduce compilation time by at least 50% compared to external compiler invocation

### Requirement 12: Implement Direct Machine Code Generation

**User Story:** As a compiler developer, I want the backend to generate machine code directly, so that compilation achieves maximum speed without external dependencies.

#### Acceptance Criteria

1. THE Backend SHALL provide a Machine_Code_Generator that emits x86-64 machine code directly
2. THE Machine_Code_Generator SHALL encode instructions according to the x86-64 instruction set architecture
3. THE Machine_Code_Generator SHALL generate ELF binary format on Linux
4. THE Machine_Code_Generator SHALL generate Mach-O binary format on macOS
5. THE Machine_Code_Generator SHALL generate PE binary format on Windows

### Requirement 13: Implement Pratt Parser for Expressions

**User Story:** As a compiler developer, I want expression parsing to use Pratt parsing, so that operator precedence is handled correctly and efficiently.

#### Acceptance Criteria

1. THE Parser SHALL implement a Pratt_Parser for all expression types
2. THE Pratt_Parser SHALL handle operator precedence without recursive descent
3. THE Pratt_Parser SHALL support binary operators (+, -, *, /, %, @, <, >, <=, >=, ==, !=)
4. THE Pratt_Parser SHALL support unary operators (-, NOT)
5. THE Pratt_Parser SHALL parse expressions in O(n) time where n is the number of tokens

### Requirement 14: Implement Expression AST Nodes

**User Story:** As a compiler developer, I want structured AST nodes for expressions, so that the IR layer receives typed expression trees instead of strings.

#### Acceptance Criteria

1. THE AST SHALL include BinaryExpr nodes with left operand, operator, and right operand fields
2. THE AST SHALL include UnaryExpr nodes with operator and operand fields
3. THE AST SHALL include CallExpr nodes with function name and argument list fields
4. THE AST SHALL include IndexExpr nodes with array expression and index expression fields
5. THE AST SHALL include LiteralExpr nodes with typed values (int, float, string, bool)

### Requirement 15: Implement Symbol Table with Integer Indices

**User Story:** As a compiler developer, I want variables referenced by integer indices into a symbol table, so that variable lookups are O(1) instead of O(log n) string comparisons.

#### Acceptance Criteria

1. THE IR_Layer SHALL maintain a symbol table mapping variable names to integer indices
2. THE IR_Layer SHALL use integer indices for all variable references in IR instructions
3. THE Symbol_Table SHALL support nested scopes with efficient lookup
4. THE Symbol_Table SHALL assign indices sequentially starting from 0
5. WHEN looking up a variable, THE IR_Layer SHALL perform at most one hash table lookup followed by integer indexing

### Requirement 16: Implement Machine IR Layer

**User Story:** As a compiler developer, I want a Machine_IR layer between high-level IR and assembly, so that architecture-specific optimizations can be applied.

#### Acceptance Criteria

1. THE Backend SHALL include a Machine_IR layer that represents architecture-specific operations
2. THE Machine_IR SHALL include register allocation information
3. THE Machine_IR SHALL include stack frame layout information
4. THE Machine_IR SHALL support x86-64, ARM64, and RISC-V architectures
5. THE Machine_IR SHALL enable instruction selection and scheduling optimizations

### Requirement 17: Implement Benchmark Suite

**User Story:** As a compiler developer, I want a comprehensive benchmark suite, so that performance improvements can be measured objectively.

#### Acceptance Criteria

1. THE AC_Compiler SHALL include a benchmark suite with at least 10 programs
2. THE Benchmark_Suite SHALL include recursive algorithms (fibonacci, factorial)
3. THE Benchmark_Suite SHALL include iterative algorithms (loops, array operations)
4. THE Benchmark_Suite SHALL include real-world programs (parsers, compilers, games)
5. THE Benchmark_Suite SHALL measure compilation time and execution time separately

### Requirement 18: Achieve C-Level Performance

**User Story:** As a compiler user, I want AC programs to execute at speeds comparable to C, so that AC is viable for performance-critical applications.

#### Acceptance Criteria

1. WHEN running the fibonacci benchmark, THE AC_Compiler SHALL produce code that executes within 2x of equivalent C code
2. WHEN running the factorial benchmark, THE AC_Compiler SHALL produce code that executes within 2x of equivalent C code
3. WHEN running array iteration benchmarks, THE AC_Compiler SHALL produce code that executes within 1.5x of equivalent C code
4. WHEN running the full benchmark suite, THE AC_Compiler SHALL achieve geometric mean performance within 2x of gcc -O2
5. THE AC_Compiler SHALL compile programs at least 5x faster than the current implementation

### Requirement 19: Maintain Backward Compatibility

**User Story:** As an AC language user, I want existing AC programs to compile without modification, so that the performance improvements do not break existing code.

#### Acceptance Criteria

1. THE AC_Compiler SHALL compile all existing test programs without errors
2. THE AC_Compiler SHALL produce identical output for all test programs
3. THE AC_Compiler SHALL support all existing language features (IF/WHILST/FOR, functions, lists, tuples, dictionaries)
4. THE AC_Compiler SHALL support all existing backends (PY, JS, C, C++, RS, GO, Java, HTML, V, ASM, BNY)
5. THE AC_Compiler SHALL maintain the same command-line interface

### Requirement 20: Implement Compilation Pipeline

**User Story:** As a compiler developer, I want a clear compilation pipeline from source to machine code, so that each phase can be tested and optimized independently.

#### Acceptance Criteria

1. THE AC_Compiler SHALL implement the pipeline: Source → Lexer → Parser → AST → IR Builder → CFG Builder → Optimizer → Machine_IR → Assembly/Machine Code
2. THE AC_Compiler SHALL provide command-line flags to stop at each pipeline stage for debugging
3. THE AC_Compiler SHALL save intermediate representations to files when requested
4. THE AC_Compiler SHALL support incremental compilation by caching intermediate results
5. THE AC_Compiler SHALL complete the full pipeline in under 100ms for programs under 1000 lines

### Requirement 21: Implement Parser Round-Trip Property

**User Story:** As a compiler developer, I want to verify parser correctness through round-trip testing, so that parsing bugs are caught early.

#### Acceptance Criteria

1. THE AC_Compiler SHALL include a pretty printer that converts AST back to source code
2. FOR ALL valid AC programs, parsing then pretty-printing then parsing SHALL produce an equivalent AST
3. THE Pretty_Printer SHALL preserve semantics (not necessarily formatting)
4. THE Pretty_Printer SHALL handle all AST node types
5. THE Round_Trip_Test SHALL be included in the test suite

### Requirement 22: Implement IR Serialization

**User Story:** As a compiler developer, I want IR serialized to a binary format, so that compilation can be split across multiple processes or machines.

#### Acceptance Criteria

1. THE IR_Layer SHALL serialize IR to a compact binary format
2. THE IR_Layer SHALL deserialize binary IR back to in-memory representation
3. THE Binary_Format SHALL include version information for compatibility checking
4. THE Binary_Format SHALL be architecture-independent (portable across x86-64, ARM64, etc.)
5. THE Serialization SHALL preserve all IR information including debug metadata

### Requirement 23: Implement Optimization Level Flags

**User Story:** As a compiler user, I want to control optimization levels, so that I can trade compilation time for execution speed.

#### Acceptance Criteria

1. THE AC_Compiler SHALL support -O0 flag for no optimizations (fast compilation)
2. THE AC_Compiler SHALL support -O1 flag for basic optimizations (constant folding, DCE)
3. THE AC_Compiler SHALL support -O2 flag for aggressive optimizations (inlining, copy propagation, SSA)
4. THE AC_Compiler SHALL support -O3 flag for maximum optimizations (loop unrolling, vectorization)
5. THE AC_Compiler SHALL default to -O1 when no flag is specified

### Requirement 24: Implement Debug Information Generation

**User Story:** As a compiler user, I want debug information in compiled binaries, so that I can use debuggers like gdb and lldb.

#### Acceptance Criteria

1. WHEN the -g flag is provided, THE AC_Compiler SHALL generate DWARF debug information
2. THE Debug_Information SHALL include source file names and line numbers
3. THE Debug_Information SHALL include variable names and types
4. THE Debug_Information SHALL include function names and boundaries
5. THE Debug_Information SHALL enable source-level debugging with gdb and lldb

### Requirement 25: Implement Error Recovery in Parser

**User Story:** As a compiler user, I want the compiler to report multiple errors in one compilation, so that I can fix multiple issues at once.

#### Acceptance Criteria

1. WHEN the Parser encounters a syntax error, THE Parser SHALL report the error and attempt to recover
2. THE Parser SHALL report at least 10 errors before stopping
3. THE Parser SHALL use synchronization tokens (newlines, dedents) for error recovery
4. THE Parser SHALL produce a partial AST even when errors are present
5. THE Parser SHALL report errors with line numbers, column numbers, and context

