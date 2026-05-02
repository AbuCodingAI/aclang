# AC Language Compiler - Technology Stack

## Build System

**Makefile** (`ac-compiler/Makefile`)
- Compiler: g++ with C++17 standard
- Flags: `-std=c++17 -Wall -O2`
- Output binary: `ac`

### Build Commands

```bash
# Compile the compiler
make

# Install to /usr/local/bin
sudo make install

# Clean build artifacts
make clean
```

## Tech Stack

### Core Language
- **C++17** - Compiler implementation
- **x86-64 Assembly** - ASM and BNY backends

### Target Backends
| Backend | Output | Status |
|---------|--------|--------|
| Python | `.py` | ✅ Complete |
| JavaScript | `.js` | ✅ Complete |
| C | `.c` | ✅ Complete |
| C++ | `.cpp` | ✅ Complete |
| Rust | `.rs` | ✅ Complete |
| Go | `.go` | ✅ Complete |
| Java | `.java` | ✅ Complete |
| HTML | `.html` | ✅ Complete |
| V | `.v` | ⚠️ Needs V compiler |
| Assembly | `.s` | ✅ Complete |
| Native Binary | `.acb` | ✅ Complete |

### Key Libraries/Dependencies
- Standard C++ library only (no external dependencies)
- For native binary compilation: GCC or Clang (system assembler/linker)

## Architecture

```
AC Source (.ac)
    ↓
Lexer (token.cpp, lexer.cpp)
    ↓
Parser (parser.cpp)
    ↓
AST (ast.hpp)
    ↓
IR Generator (ir.cpp) - Optional optimization layer
    ↓
IR JSON (intermediate representation)
    ↓
Backend Registry (backend_registry.hpp)
    ↓
Code Generator (codegen_*.cpp)
    ↓
Target Output
```

### Source Files
- `src/main.cpp` - Entry point, CLI handling, compilation flow
- `src/lexer.cpp` - Tokenization of AC source
- `src/parser.cpp` - AST construction
- `src/ir.cpp` - IR generation (optional)
- `src/backend_registry.cpp` - Backend registration system
- `src/codegen_*.cpp` - 11 backend-specific code generators

### Header Files (`include/`)
- `ast.hpp` - AST node definitions
- `ir.hpp` - IR data structures
- `token.hpp` - Token types
- `backend_registry.hpp` - Backend registration
- `codegen_utils.hpp` - Shared codegen utilities
- `error.hpp` - Error handling
- `tags.hpp` - Tag system definitions
- `type.hpp` - Type inference system

## Compilation Flow

1. **Lexer** tokenizes source into tokens (keywords, identifiers, strings, operators)
2. **Parser** builds AST from tokens
3. **IR Generation** (optional) creates intermediate representation
4. **IR JSON** serialization for backend-agnostic processing
5. **Backend Selection** via `backend_registry`
6. **Code Generation** produces target language code
7. **Execution** runs generated code with appropriate runner

## Backend Detection

Backends are detected from `AC->XX` declarations in source files:
- `AC->PY` → Python
- `AC->BNY` → Native binary
- `AC->ASM` → Assembly
- `AC->JS` → JavaScript
- `AC->C` → C
- `AC->C++` or `AC->CPP` → C++
- `AC->RS` → Rust
- `AC->GO` → Go
- `AC->Java` → Java
- `AC->HTML` → HTML
- `AC->V` → V
