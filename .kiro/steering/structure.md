# AC Language Compiler - Project Structure

```
ac-compiler/
├── include/              # Header files
│   ├── ast.hpp          # AST node definitions and types
│   ├── ir.hpp           # Intermediate representation structures
│   ├── token.hpp        # Token type definitions
│   ├── backend_registry.hpp  # Backend registration system
│   ├── codegen_utils.hpp     # Shared codegen utilities
│   ├── error.hpp        # Error handling
│   ├── tags.hpp         # Tag system definitions
│   └── type.hpp         # Type inference system
├── src/                 # Source files
│   ├── main.cpp         # Entry point, CLI, compilation orchestration
│   ├── lexer.cpp        # Tokenization
│   ├── parser.cpp       # AST construction
│   ├── ir.cpp           # IR generation
│   ├── backend_registry.cpp
│   ├── acc_cache.hpp    # Compilation cache system
│   └── codegen_*.cpp    # 11 backend code generators
│       ├── codegen_py.cpp    # Python backend
│       ├── codegen_js.cpp    # JavaScript backend
│       ├── codegen_html.cpp  # HTML backend
│       ├── codegen_java.cpp  # Java backend
│       ├── codegen_cpp.cpp   # C++ backend
│       ├── codegen_c.cpp     # C backend
│       ├── codegen_asm.cpp   # Assembly backend
│       ├── codegen_rs.cpp    # Rust backend
│       ├── codegen_go.cpp    # Go backend
│       ├── codegen_v.cpp     # V backend
│       └── codegen_bny.cpp   # Native binary backend
├── test/                # Test files and expected outputs
│   ├── *.ac            # AC source test files
│   ├── *.py, *.js, etc # Expected output for each backend
│   └── *.acc           # Cached AST files
├── library/             # Built-in libraries (ilib, elib, clib)
├── Makefile            # Build configuration
├── README.md           # Project documentation
└── ac                  # Compiled binary
```

## Key Directories

### `include/`
Contains all header files defining data structures:
- **ast.hpp**: Defines `ASTNode` struct with `NodeType` enum for all AST node types
- **ir.hpp**: Defines IR structures (`IRProgram`, `IRFunction`, `IRInstruction`, `IRRef`)
- **token.hpp**: Token types for lexer output
- **backend_registry.hpp**: Backend registration system

### `src/`
Implementation files:
- **main.cpp**: CLI argument parsing, file I/O, compilation orchestration
- **lexer.cpp**: Tokenizes AC source into tokens
- **parser.cpp**: Builds AST from tokens
- **ir.cpp**: Generates IR from AST (optional optimization layer)
- **codegen_*.cpp**: Backend-specific code generators (one per target language)

### `test/`
Test files with expected outputs:
- `*.ac`: AC source test programs
- `*.py`, `*.js`, `*.c`, etc.: Expected generated output
- `*.acc`: Cached AST for faster recompilation

## File Naming Conventions

- **Source files**: `src/*.cpp`
- **Header files**: `include/*.hpp`
- **Test files**: `test/*.ac` with corresponding expected outputs
- **Cache files**: `*.acc` (same name as source, different extension)
- **Output files**: Backend-specific extensions (`.py`, `.js`, `.c`, `.cpp`, `.rs`, `.go`, `.java`, `.html`, `.v`, `.s`, `.acb`)

## Backend Code Generator Pattern

Each backend follows a consistent pattern:
1. Include `base_codegen.hpp` and backend-specific headers
2. Implement `genNode()` method with switch on `NodeType`
3. Use `emit()` for indented output
4. Handle string escaping, boolean translation, and operator translation
5. Support event listeners conditionally (only when used)

## Compilation Cache

The compiler uses `.acc` files to cache parsed AST:
- Location: Same directory as source, with `.acc` extension
- Purpose: Faster recompilation by skipping lex/parse
- Triggered automatically unless `--force` flag is used
