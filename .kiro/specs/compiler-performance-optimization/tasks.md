# Implementation Plan: High-Performance AC Compiler

## Overview

This implementation plan transforms the AC compiler from its current string-heavy implementation into a high-performance compiler achieving C-level execution speeds. The plan follows a 10-phase roadmap over 20 weeks, implementing: Pratt parser for expressions, integerized IR with arena allocation, immediate CFG lowering, SSA conversion, optimization passes (constant folding, DCE, copy propagation, inlining), register allocation, direct machine code generation, debug information, error recovery, and comprehensive integration testing.

## Tasks

- [x] 1. Phase 1: Frontend Transformation - Pratt Parser and Structured AST (Weeks 1-2)
  - [x] 1.1 Implement Pratt parser for expressions in parser.cpp
    - Add PrattParser class with parseExpression(), parsePrefix(), parseInfix() methods
    - Implement precedence table: OR(1), AND(2), comparisons(3), +/-(4), */%(5), unary(6), calls/indexing(7)
    - Support all binary operators: +, -, *, /, %, @, <, >, <=, >=, ==, !=
    - Support unary operators: -, NOT
    - Parse in O(n) time using top-down operator precedence algorithm
    - _Requirements: 13.1, 13.2, 13.3, 13.4, 13.5_

  - [ ]* 1.2 Write property test for Pratt parser precedence
    - **Property 18: Pratt Parser Precedence**
    - **Validates: Requirements 13.1, 13.2, 13.3, 13.4**

  - [x] 1.3 Add structured expression AST nodes to ast.hpp
    - Add BinaryExpr node with left, op, right fields
    - Add UnaryExpr node with op, operand fields
    - Add CallExpr node with funcName, arguments fields
    - Add IndexExpr node with array, index fields
    - Add LiteralExpr node with type (INT/FLOAT/STRING/BOOL/NULL) and value fields
    - _Requirements: 14.1, 14.2, 14.3, 14.4, 14.5_

  - [ ]* 1.4 Write property test for structured AST generation
    - **Property 1: Expression Parsing Produces Structured AST**
    - **Validates: Requirements 1.1, 1.2, 14.1, 14.2, 14.3, 14.4, 14.5**

  - [x] 1.5 Update parser to use Pratt parsing for all expressions
    - Replace string-based expression capture with Pratt parser calls
    - Ensure expressions parsed exactly once in frontend
    - Remove string parsing from IR layer
    - _Requirements: 1.1, 1.2, 1.5_

  - [ ]* 1.6 Write property test for single-pass parsing
    - **Property 2: Single-Pass Expression Parsing**
    - **Validates: Requirements 1.5**

  - [ ]* 1.7 Write property test for IR string-free operation
    - **Property 3: IR Layer String-Free Operation**
    - **Validates: Requirements 1.3, 1.4**

  - [x]* 1.8 Write unit tests for expression parsing
    - Test binary operator precedence (2 + 3 * 4 = 14)
    - Test unary operators (-5, NOT true)
    - Test function calls (func(a, b, c))
    - Test array indexing (arr[0], arr[i+1])
    - Test nested expressions ((a + b) * (c - d))

- [x] 2. Checkpoint - Verify frontend transformation
  - Ensure all tests pass, ask the user if questions arise.

- [-] 3. Phase 2: Integerized IR with Arena Allocation (Weeks 3-4)
  - [x] 3.1 Implement integerized IRRef structure in ir.hpp
    - Update IRRef to use integer IDs for TEMP (0,1,2...) and LABEL (0,1,2...)
    - Add integer id field for temporaries and labels
    - Keep string conversion only in toString() for debugging
    - _Requirements: 2.1, 2.2, 2.3, 2.4, 2.5_

  - [ ]* 3.2 Write property test for integerized IR references
    - **Property 4: Integerized IR References**
    - **Validates: Requirements 2.1, 2.2, 2.3, 2.4, 2.5**

  - [ ] 3.3 Implement SymbolTable with integer indexing
    - Create SymbolTable class with names vector (index→name) and ids map (name→index)
    - Implement intern() method for adding variables
    - Implement getName() method for lookups
    - Support nested scopes with efficient lookup
    - Assign indices sequentially starting from 0
    - _Requirements: 15.1, 15.2, 15.3, 15.4, 15.5_

  - [ ]* 3.4 Write property test for symbol table integer indexing
    - **Property 19: Symbol Table Integer Indexing**
    - **Validates: Requirements 15.1, 15.2, 15.3, 15.4**

  - [ ] 3.5 Implement Arena allocator
    - Create Arena class with Block structure (data, size, used)
    - Implement allocate() method with 64KB default block size
    - Implement create<T>() template for typed allocation
    - Implement reset() and clear() methods
    - Store blocks in vector for contiguous allocation
    - _Requirements: 3.2, 3.4, 3.5_

  - [ ]* 3.6 Write property test for arena allocation
    - **Property 6: Arena Allocation for IR**
    - **Validates: Requirements 3.2, 3.4, 3.5**

  - [ ] 3.7 Update IR storage to use flat arrays
    - Change IRFunction::instructions to std::vector<IRInstruction>
    - Remove std::unique_ptr and std::shared_ptr for individual instructions
    - Store instructions in contiguous flat arrays
    - Allocate all function instructions from same arena
    - _Requirements: 3.1, 3.3_

  - [ ]* 3.8 Write property test for flat array IR storage
    - **Property 5: Flat Array IR Storage**
    - **Validates: Requirements 3.1, 3.3**

  - [ ] 3.9 Update IR generation to use integer references
    - Modify IRGenerator in ir.cpp to use integer IDs for temps and labels
    - Use SymbolTable for variable references
    - Remove string-based temporary and label generation
    - _Requirements: 2.1, 2.2, 2.3_

  - [ ]* 3.10 Write unit tests for IR generation
    - Test integer reference generation
    - Test symbol table construction
    - Test arena allocation usage
    - Test flat array storage

- [ ] 4. Checkpoint - Verify integerized IR
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 5. Phase 3: CFG Lowering (Weeks 5-6)
  - [ ] 5.1 Add jump and label IR instructions
    - Add JUMP, JUMP_IF_TRUE, JUMP_IF_FALSE opcodes to IROpcode enum
    - Ensure LABEL opcode exists for label definitions
    - Update IR instruction handling for control flow
    - _Requirements: 4.5_

  - [ ] 5.2 Implement CFG builder for IF statements
    - Lower IF condition to: eval(cond), jf cond L_else, [body], jmp L_end, L_else:, [else_body], L_end:
    - Emit conditional jumps immediately during IR generation
    - Do not preserve high-level IF nodes in IR
    - _Requirements: 4.1_

  - [ ] 5.3 Implement CFG builder for WHILST loops
    - Lower WHILST to: L_start:, eval(cond), jf cond L_end, [body], jmp L_start, L_end:
    - Emit loop labels and back-edges immediately
    - _Requirements: 4.2_

  - [ ] 5.4 Implement CFG builder for FOR loops
    - Lower FOR to: iter_init, L_start:, iter_has_next, jf L_end, iter_next, [body], jmp L_start, L_end:
    - Emit iterator initialization, condition checks, and loop jumps immediately
    - _Requirements: 4.3_

  - [ ] 5.5 Verify no high-level control flow in IR
    - Ensure all IF/WHILST/FOR lowered to jumps and labels
    - CFG explicit through JUMP, JUMP_IF_TRUE, JUMP_IF_FALSE instructions
    - _Requirements: 4.4, 4.5_

  - [ ]* 5.6 Write property test for immediate CFG lowering
    - **Property 7: Immediate CFG Lowering**
    - **Validates: Requirements 4.1, 4.2, 4.3, 4.4, 4.5**

  - [ ]* 5.7 Write unit tests for CFG lowering
    - Test IF statement lowering
    - Test WHILST loop lowering
    - Test FOR loop lowering
    - Test nested control flow

- [ ] 6. Checkpoint - Verify CFG lowering
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 7. Phase 4: SSA Conversion (Weeks 7-8)
  - [ ] 7.1 Implement dominator tree construction
    - Create DominatorTree structure with idom, children, frontiers fields
    - Implement buildDominatorTree() algorithm
    - Compute immediate dominators for all basic blocks
    - Compute dominance frontiers
    - _Requirements: 5.1, 5.4_

  - [ ] 7.2 Implement PHI node insertion
    - Add PHI instruction type to IR
    - Create PhiInstruction with result and incoming (value, block_id) pairs
    - Insert PHI nodes at dominance frontiers
    - _Requirements: 5.2, 5.3_

  - [ ] 7.3 Implement variable renaming for SSA
    - Create unique SSA variables for each assignment
    - Maintain mapping from original variables to SSA variables
    - Rename variables throughout function
    - _Requirements: 5.1, 5.4_

  - [ ] 7.4 Create SSA converter pass
    - Implement SSAConverter class with convertToSSA() method
    - Integrate dominator tree, PHI insertion, and variable renaming
    - Enable constant propagation, DCE, and copy propagation optimizations
    - _Requirements: 5.5_

  - [ ]* 7.5 Write property test for SSA form conversion
    - **Property 8: SSA Form Conversion**
    - **Validates: Requirements 5.1, 5.2, 5.3, 5.4**

  - [ ]* 7.6 Write unit tests for SSA conversion
    - Test multiple assignment handling
    - Test PHI node insertion at merge points
    - Test variable renaming
    - Test dominator tree construction

- [ ] 8. Checkpoint - Verify SSA conversion
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 9. Phase 5: Optimization Passes (Weeks 9-11)
  - [ ] 9.1 Implement constant folding optimization
    - Create ConstantFolder class with run() method
    - Implement fold() for arithmetic operations (+, -, *, /, %)
    - Implement fold() for comparison operations (<, >, <=, >=, ==, !=)
    - Implement fold() for logical operations (AND, OR, NOT)
    - Replace operations with constant load instructions
    - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.5_

  - [ ]* 9.2 Write property test for constant folding completeness
    - **Property 9: Constant Folding Completeness**
    - **Validates: Requirements 6.1, 6.2, 6.3, 6.4, 6.5**

  - [ ] 9.3 Implement dead code elimination optimization
    - Create DeadCodeEliminator class with run() method
    - Implement mark-and-sweep algorithm
    - Mark instructions with side effects (I/O, calls, stores)
    - Mark instructions used by marked instructions
    - Sweep unmarked instructions
    - Remove unreachable basic blocks
    - Iterate until fixed-point convergence
    - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5_

  - [ ]* 9.4 Write property test for dead code elimination correctness
    - **Property 10: Dead Code Elimination Correctness**
    - **Validates: Requirements 7.1, 7.2, 7.3, 7.4**

  - [ ]* 9.5 Write property test for DCE convergence
    - **Property 11: Dead Code Elimination Convergence**
    - **Validates: Requirements 7.5**

  - [ ] 9.6 Implement copy propagation optimization
    - Create CopyPropagator class with run() method
    - Replace uses of copied values with original (x=y; z=x → z=y)
    - Respect SSA form constraints
    - Do not propagate across PHI nodes
    - Remove dead copy instructions
    - Reduce instruction count by at least 10%
    - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5_

  - [ ]* 9.7 Write property test for copy propagation correctness
    - **Property 12: Copy Propagation Correctness**
    - **Validates: Requirements 8.1, 8.2, 8.3, 8.4**

  - [ ] 9.8 Implement function inlining optimization
    - Create Inliner class with run() method
    - Inline functions with <20 instructions
    - Inline functions called exactly once (any size)
    - Never inline recursive functions
    - Rename variables in inlined body to avoid conflicts
    - _Requirements: 9.1, 9.2, 9.3, 9.4, 9.5_

  - [ ]* 9.9 Write property test for function inlining heuristics
    - **Property 13: Function Inlining Heuristics**
    - **Validates: Requirements 9.1, 9.2, 9.3, 9.4, 9.5**

  - [ ] 9.10 Implement pass manager with optimization levels
    - Create PassManager class with addPass() and run() methods
    - Implement createO0() - no optimization
    - Implement createO1() - basic (constant fold, DCE)
    - Implement createO2() - aggressive (+ copy prop, inline)
    - Implement createO3() - maximum (+ loop unroll, vectorize)
    - Add -O0/-O1/-O2/-O3 command-line flags
    - _Requirements: 23.1, 23.2, 23.3, 23.4, 23.5_

  - [ ]* 9.11 Write unit tests for optimization passes
    - Test constant folding (2+3→5)
    - Test dead code elimination
    - Test copy propagation
    - Test function inlining
    - Test optimization levels

- [ ] 10. Checkpoint - Verify optimization passes
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 11. Phase 6: Register Allocation (Weeks 12-13)
  - [ ] 11.1 Implement live interval computation
    - Create LiveInterval structure with tempId, start, end fields
    - Implement computeLiveIntervals() for function analysis
    - Track first use and last use for each temporary
    - _Requirements: 10.1_

  - [ ] 11.2 Implement linear scan register allocation
    - Create RegisterAllocator class with allocate() method
    - Implement linearScan() algorithm
    - Assign physical registers to frequently-used temporaries
    - Create RegisterAssignment with tempToReg and spillSlots maps
    - _Requirements: 10.1, 10.2_

  - [ ] 11.3 Implement register spilling to stack
    - Spill variables to stack when physical registers exhausted
    - Track spill slots in RegisterAssignment
    - Generate load/store instructions for spilled variables
    - _Requirements: 10.3_

  - [ ] 11.4 Add calling convention support
    - Define x86-64 register sets: ARG_REGS, CALLER_SAVED, CALLEE_SAVED
    - Respect calling convention constraints
    - Handle caller-saved vs callee-saved registers
    - _Requirements: 10.4_

  - [ ] 11.5 Optimize for register utilization
    - Achieve at least 80% register utilization on typical programs
    - Minimize spilling through better allocation
    - _Requirements: 10.5_

  - [ ]* 11.6 Write property test for register allocation completeness
    - **Property 14: Register Allocation Completeness**
    - **Validates: Requirements 10.1, 10.3, 10.4**

  - [ ]* 11.7 Write unit tests for register allocation
    - Test live interval computation
    - Test linear scan algorithm
    - Test spilling behavior
    - Test calling convention

- [ ] 12. Checkpoint - Verify register allocation
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 13. Phase 7: Machine Code Generation (Weeks 14-16)
  - [ ] 13.1 Implement Machine IR layer
    - Create MachineInstr structure with Opcode and MachineOperand fields
    - Create MachineOperand with Kind (REG/IMM/MEM/LABEL) and value fields
    - Create MachineFunction with instructions, stackFrameSize, usedRegisters
    - Support x86-64, ARM64, RISC-V architectures
    - _Requirements: 16.1, 16.2, 16.3, 16.4, 16.5_

  - [ ] 13.2 Implement Machine IR builder
    - Create MachineIRBuilder class with lower() method
    - Lower IR instructions to machine instructions
    - Implement lowerInstruction() for each IR opcode
    - _Requirements: 16.1_

  - [ ] 13.3 Implement x86-64 instruction encoder
    - Create X64CodeGenerator class with generate() method
    - Implement emitInstruction() for x86-64 encoding
    - Implement emitModRM(), emitSIB(), emitImmediate() helpers
    - Encode instructions according to x86-64 ISA
    - Track label positions for jump resolution
    - _Requirements: 12.1, 12.2_

  - [ ]* 13.4 Write property test for machine code generation correctness
    - **Property 16: Machine Code Generation Correctness**
    - **Validates: Requirements 12.1, 12.2**

  - [ ] 13.5 Implement ELF binary generator for Linux
    - Create ELFGenerator class with generate() method
    - Implement emitELFHeader(), emitProgramHeaders(), emitSectionHeaders()
    - Implement emitTextSection(), emitDataSection(), emitSymbolTable()
    - Generate valid ELF format binaries
    - _Requirements: 12.3_

  - [ ] 13.6 Implement Mach-O binary generator for macOS
    - Create MachOGenerator class with generate() method
    - Generate valid Mach-O format binaries
    - _Requirements: 12.4_

  - [ ] 13.7 Implement PE binary generator for Windows
    - Create PEGenerator class with generate() method
    - Generate valid PE format binaries
    - _Requirements: 12.5_

  - [ ]* 13.8 Write property test for binary format generation
    - **Property 17: Binary Format Generation**
    - **Validates: Requirements 12.3, 12.4, 12.5**

  - [ ] 13.9 Remove external compiler invocation
    - Remove all system() and exec() calls to external compilers
    - Generate assembly/machine code directly
    - Use library APIs for assembler/linker where needed
    - Reduce compilation time by at least 50%
    - _Requirements: 11.1, 11.2, 11.3, 11.4, 11.5_

  - [ ]* 13.10 Write property test for no external compiler invocation
    - **Property 15: No External Compiler Invocation**
    - **Validates: Requirements 11.1, 11.2, 11.3, 11.4**

  - [ ]* 13.11 Write unit tests for code generation
    - Test x86-64 instruction encoding
    - Test ELF binary generation
    - Test Mach-O binary generation
    - Test PE binary generation
    - Test binary execution

- [ ] 14. Checkpoint - Verify machine code generation
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 15. Phase 8: Debug Information (Week 17)
  - [ ] 15.1 Implement DWARF debug information generator
    - Create DWARFGenerator class with generateDebugInfo() method
    - Implement addLineInfo() for line number tracking
    - Implement addVariable() for variable information
    - Implement addFunction() for function boundaries
    - Generate DWARF format debug sections
    - _Requirements: 24.1, 24.2, 24.3, 24.4_

  - [ ] 15.2 Add -g flag for debug information
    - Add command-line flag parsing for -g
    - Generate DWARF debug info when -g is provided
    - Include source file names and line numbers
    - Include variable names and types
    - Include function names and boundaries
    - Enable source-level debugging with gdb and lldb
    - _Requirements: 24.1, 24.5_

  - [ ]* 15.3 Write property test for debug information completeness
    - **Property 23: Debug Information Completeness**
    - **Validates: Requirements 24.1, 24.2, 24.3, 24.4**

  - [ ]* 15.4 Write unit tests for debug information
    - Test line number tracking
    - Test variable information
    - Test function boundaries
    - Test gdb/lldb debugging

- [ ] 16. Checkpoint - Verify debug information
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 17. Phase 9: Error Handling (Week 18)
  - [ ] 17.1 Implement error recovery in parser
    - Add errors vector to Parser class
    - Implement reportError() method with line, column, context
    - Implement synchronize() method for error recovery
    - Use synchronization tokens (NEWLINE, DEDENT, IF, FOR, WHILST)
    - Report at least 10 errors before stopping
    - _Requirements: 25.1, 25.2, 25.3_

  - [ ] 17.2 Implement partial AST generation on errors
    - Generate partial AST even when errors present
    - Continue parsing after error recovery
    - _Requirements: 25.4_

  - [ ] 17.3 Improve error messages
    - Include line numbers, column numbers, and context
    - Provide helpful error messages
    - _Requirements: 25.5_

  - [ ]* 17.4 Write property test for parser error recovery
    - **Property 24: Parser Error Recovery**
    - **Validates: Requirements 25.1, 25.2, 25.3, 25.4, 25.5**

  - [ ]* 17.5 Write unit tests for error handling
    - Test error recovery at synchronization points
    - Test multiple error reporting
    - Test partial AST generation
    - Test error message quality

- [ ] 18. Checkpoint - Verify error handling
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 19. Phase 10: Integration and Testing (Weeks 19-20)
  - [ ] 19.1 Integrate all pipeline stages
    - Connect Lexer → Parser → AST → IR Builder → CFG Builder → SSA Converter → Optimizer → Register Allocator → Machine IR → Code Generator
    - Add command-line flags to stop at each stage (--stop-after-parse, --stop-after-ir, etc.)
    - Add flags to save intermediate representations (--save-ast, --save-ir, etc.)
    - Ensure pipeline completes in <100ms for 1000-line programs
    - _Requirements: 20.1, 20.2, 20.3, 20.5_

  - [ ] 19.2 Implement incremental compilation caching
    - Cache intermediate results (AST, IR, optimized IR)
    - Reuse cached results when source unchanged
    - Achieve faster second compilation
    - _Requirements: 20.4_

  - [ ]* 19.3 Write property test for incremental compilation caching
    - **Property 20: Incremental Compilation Caching**
    - **Validates: Requirements 20.4**

  - [ ] 19.4 Create benchmark suite
    - Create benchmarks/ directory
    - Add fibonacci.ac (recursive algorithm)
    - Add factorial.ac (recursive algorithm)
    - Add array_sum.ac (iterative array operations)
    - Add matrix_mult.ac (nested loops)
    - Add quicksort.ac (complex algorithm)
    - Add bubble_sort.ac (nested loops)
    - Add string_concat.ac (string operations)
    - Add hash_table.ac (dictionary operations)
    - Add parser.ac (real-world program)
    - Add pong.ac (game logic)
    - _Requirements: 17.1, 17.2, 17.3, 17.4, 17.5_

  - [ ] 19.5 Run benchmark suite and measure performance
    - Measure compilation time (ms)
    - Measure execution time (ms)
    - Measure binary size (bytes)
    - Measure memory usage (MB)
    - Compare against current implementation
    - Compare against gcc -O2
    - _Requirements: 18.1, 18.2, 18.3, 18.4, 18.5_

  - [ ] 19.6 Verify backward compatibility
    - Run all existing test programs
    - Verify identical output
    - Test all language features (IF/WHILST/FOR, functions, lists, tuples, dicts)
    - Test all backends (PY, JS, C, C++, RS, GO, Java, HTML, V, ASM, BNY)
    - Verify command-line interface unchanged
    - _Requirements: 19.1, 19.2, 19.3, 19.4, 19.5_

  - [ ] 19.7 Implement parser round-trip testing
    - Create pretty printer that converts AST back to source
    - Verify parse → pretty-print → parse produces equivalent AST
    - Preserve semantics (not formatting)
    - Handle all AST node types
    - _Requirements: 21.1, 21.2, 21.3, 21.4_

  - [ ]* 19.8 Write property test for parser round-trip preservation
    - **Property 21: Parser Round-Trip Preservation**
    - **Validates: Requirements 21.1, 21.2, 21.3, 21.4**

  - [ ] 19.9 Implement IR serialization
    - Serialize IR to compact binary format
    - Deserialize binary IR back to in-memory representation
    - Include version information for compatibility
    - Ensure architecture-independent portability
    - Preserve all IR information including debug metadata
    - _Requirements: 22.1, 22.2, 22.3, 22.4, 22.5_

  - [ ]* 19.10 Write property test for IR serialization round-trip
    - **Property 22: IR Serialization Round-Trip**
    - **Validates: Requirements 22.1, 22.2, 22.3, 22.4, 22.5**

  - [ ]* 19.11 Write end-to-end integration tests
    - Test full compilation pipeline
    - Test all optimization levels (-O0/-O1/-O2/-O3)
    - Test all backends
    - Test cross-platform binary generation
    - Test debug information generation

- [ ] 20. Final checkpoint - Verify complete integration
  - Ensure all tests pass, ask the user if questions arise.

## Notes

- Tasks marked with `*` are optional and can be skipped for faster MVP
- Each task references specific requirements for traceability
- Checkpoints ensure incremental validation
- Property tests validate universal correctness properties
- Unit tests validate specific examples and edge cases
- The implementation follows a 10-phase roadmap over 20 weeks
- Performance targets: 5x faster compilation, execution within 2x of gcc -O2
- All existing AC language features and backends must remain functional

