# ADK — Abu Development Kit

The ADK is the unified toolchain for the **A language family**: three languages designed to cover every layer of the software stack, from cloud applications to bare-metal kernels.

```
ADK
├── AC   — compiled, general-purpose, 12+ output targets
├── AI   — interpreted, exact decimal arithmetic, bytecode VM
└── AC+  — systems/hardware, x86-64 assembly, OS/kernel work
```

---

## 1. The Three Languages

| Property        | AC                          | AI                         | AC+                          |
|-----------------|-----------------------------|----------------------------|------------------------------|
| Purpose         | General-purpose apps        | Exact arithmetic / scripts | Systems / hardware / OS      |
| Output          | 12 language targets         | `.aibc` → AIVM             | x86-64 assembly              |
| Level           | High                        | High                       | Low (register, memory)       |
| File extension  | `.ac`                       | `.ai`                      | `.acp`                       |
| Entry point     | `<mainloop>` label          | `<mainloop>` label         | `<mainloop>` label           |
| Functions       | `Make name func(params)`    | `Make name func(params)`   | `Make name func(params)`     |
| Halt            | `/kill`                     | `/kill`                    | `/kill`                      |
| Variables       | Inferred plain vars         | Same as AC + `dec` type    | Same + `reg 64box` / `stack` |
| Target decl     | `AC->PY`, `AC->C`, etc.     | `AI->VM`                   | None (always ASM)            |
| Nil             | Used in conditions          | Used in conditions         | `nil` — explicit hardware null|
| Use case        | Apps, scripts, games        | Finance, science, math     | Kernels, bootloaders, AOS    |

---

## 2. AC — The Compiled Core

AC is the flagship language. Source files compile to one of **12+ output targets** through a unified IR pipeline:

| Target | Keyword  | Output                        |
|--------|----------|-------------------------------|
| Python | `AC->PY` | `.py`                         |
| C      | `AC->C`  | `.c`                          |
| C++    | `AC->CPP`| `.cpp`                        |
| JavaScript | `AC->JS` | `.js`                    |
| HTML   | `AC->HTML`| `.html`                      |
| Java   | `AC->JAVA`| `.java`                      |
| Go     | `AC->GO` | `.go`                         |
| Rust   | `AC->RS` | `.rs`                         |
| V      | `AC->V`  | `.v`                          |
| Assembly | `AC->ASM` | `.asm` (AT&T x86-64)       |

**Key features:**
- Inferred types — no type annotations needed
- `Term.display`, `Term.input`, `Term.halt` — standard I/O keywords
- `dec`/`int`/`string`/`bool` — type coercion (redefines variable type in static backends)
- `func`, `IF`/`ELIF`/`OTHER`, `WHILST`, `FOR` — full control flow
- `ilib` / `elib` / `clib` — library system
- Math library via `use ilib math` — dot-call syntax: `math.sqrt(x)`, `math.pi(16)`
- Bundle/class construct for grouped state
- Compound assignment: `+=`, `-=`, `*=`, `/=`, `%=`, `**=`

**Pipeline:**
```
.ac source
    │
    ▼
Lexer + Parser  (lexer.cpp, parser.cpp)
    │  AST
    ▼
IR Lowering     (ir.cpp)
    │  Unified IR (.lir)
    ▼
UnifiedIRCodeGen (ir_codegen.cpp)  — BackendStrategy pattern
    │
    ├─→ PythonStrategy   → .py
    ├─→ JavaScriptStrategy → .js
    ├─→ HTMLStrategy     → .html
    ├─→ CStrategy        → .c
    ├─→ CppStrategy      → .cpp
    ├─→ JavaStrategy     → .java
    ├─→ GoStrategy       → .go
    ├─→ RustStrategy     → .rs
    ├─→ VStrategy        → .v
    └─→ ASMStrategy      → .asm
```

---

## 3. AI — The Exact Arithmetic Interpreter

AI adds one primitive type — `dec` — that makes exact decimal arithmetic a first-class citizen with no floating-point error. The compilation model mirrors Java: source compiles to platform-independent bytecode, which the AIVM interprets (or JIT-compiles for hot paths).

**The `dec` type:**
- Stored as `std::map<int, int>` — exponent → digit (0–9), sparse
- `0.1 + 0.2 = 0.3` — always exact, no IEEE 754 rounding
- `19.99 * 1.08 = 21.5892` — exact
- Division: long division to configurable precision (default 20 fractional digits); warns on non-terminating results

**Pipeline:**
```
.ai source
    │
    ▼
Shared Lexer + Parser  (AC's lexer.cpp, parser.cpp)
    │  AST
    ▼
AI IR lowering         (ai_codegen.cpp)
    │
    ▼
.aibc bytecode         (binary: magic AIBC, constant pool, function table, opcode stream)
    │
    ▼
AIVM interpreter       (register-based, 4 bytes/instruction)
    │  (hot path)
    └─→ JIT via relic ASM+BNY codegens → mmap → native callable
```

**Usage:**
```sh
ai mycode.ai          # compile + run
ai mycode.ai --no-run # compile only → mycode.aibc
aivm mycode.aibc      # run pre-compiled bytecode
```

**Why AI vs AC for decimals:**
- AC uses IEEE 754 floats — fast, but `0.1 + 0.2 ≠ 0.3`
- AI uses `dec` — slower, but perfectly exact
- Use AC for speed; use AI where correctness of decimal math is mandatory (finance, science)

---

## 4. AC+ — The Systems Layer

AC+ targets **bare metal** — no OS, no runtime, just x86-64 assembly. It is the foundation for **AOS** (Abu Operating System).

**Key syntax additions over AC:**
- `reg 64box1 = 0xB8000` — typed register variable (`{bits}{name}`)
- `64box1 << value` — memory-write primitive (equivalent to C's `*ptr = val`)
- `stack varname type` — stack allocation
- `locate varname` — address-of (like `&` in C)
- `/kill` — hardware halt (`HLT` instruction)
- `Make funcname func(params)` — function definition
- `<mainloop> ... <mainloop>` — entry point / re-entry loop label
- `nil` — hardware null/zero, used in nil checks
- `use flib file.acp` — file library import (like a C++ `#include`)

**AOS — The Abu Operating System:**

The `AC+/` directory contains the kernel seed for AOS:

| File         | Role                                                   |
|--------------|--------------------------------------------------------|
| `skeleton.s` | 16-bit real-mode BIOS stub; prints "AOS ", sets stack, jumps to `__ai_main__` |
| `logic.acp`  | Hardware logic library — `boot_print(char_code, offset)` writes to VGA VRAM at `0xB8000` |
| `kernel.acp` | Boot sequence — nil check, writes `L O A D` to screen, infinite idle loop |

**Boot sequence:**
1. BIOS loads MBR, jumps to `skeleton.s`
2. `skeleton.s` prints "AOS " via BIOS INT 10h, sets up stack at `0x7C00`, jumps to `__ai_main__`
3. `kernel.acp` allocates a stack buffer, nil-checks the pointer, writes `L O A D` to VRAM offsets 0, 2, 4, 6
4. Enters `WHILST status #> 5` infinite loop to keep the display alive

---

## 5. Language Interoperability

The three languages share a common foundation:

- **Same grammar**: AC's lexer and parser are shared across AC and AI; AC+ is a hardware-facing superset of AC syntax
- **Same IR format**: AC and AI both lower to a unified IR; AC+ lowers to assembly directly
- **Library system**:
  - AC uses `ilib` (internal), `elib` (external), `clib` (C FFI)
  - AI uses the same `ilib`/`elib` system
  - AC+ uses `flib` (file library — direct include of `.acp` files)
- **Math library**: AC's `use ilib math` dot-call syntax is designed to be consistent across all three

**Planned cross-language features:**
- AC programs calling AI functions for exact-decimal sections
- AC+ providing hardware services that AC programs can invoke via FFI
- Shared standard library surface (Term, math, files) with consistent naming across all three

---

## 6. Tooling

### Current
| Tool         | Description                                        |
|--------------|----------------------------------------------------|
| `ac`         | AC compiler binary — compiles `.ac` to any target  |
| `ai`         | AI compiler — compiles `.ai` to `.aibc`, then runs |
| `aivm`       | AIVM — loads and executes `.aibc` bytecode files   |
| `ac-ide`     | IDE extension (VS Code) — syntax highlighting, run |
| Math library | `libacmath.so` + `ac_math_all.hpp` — trig, stats, calculus |

### Planned
| Tool            | Description                                                     |
|-----------------|-----------------------------------------------------------------|
| `adk` CLI       | Unified entry point: `adk run myfile.ac`, `adk build`, `adk new` |
| `adk new`       | Project scaffolding — creates project structure with correct headers |
| AC+ compiler    | `acp` binary — compiles `.acp` → x86-64 ASM → links with `skeleton.s` |
| AOS image build | `adk build-os` — assembles + links `skeleton.s` + compiled kernel into bootable `.img` |
| Package registry| `adk pkg install math` — central library registry for all three languages |
| REPL            | `adk repl` — interactive AC/AI prompt with live evaluation      |
| `adk test`      | Unified test runner across AC, AI, and AC+ projects             |
| LSP server      | Language server for full IDE support (completion, diagnostics)  |

---

## 7. Distribution

| Channel   | Status      | Notes                                           |
|-----------|-------------|-------------------------------------------------|
| npm       | Published   | `ac-compiler` package — `ac` binary via npx    |
| VS Code   | In progress | `ac-ide` extension for syntax + run support    |
| Windows   | Prebuilt    | `ac.exe` (MinGW cross-compile, static binary)  |
| Linux     | Built from source | `make` in `ac-compiler/`               |
| macOS     | Planned     | Cross-compile or CI build                      |

---

## 8. Design Philosophy

- **One syntax, three depths**: AC for apps, AI for exact math, AC+ for hardware — same grammar, different power
- **No runtime dependencies for AC**: compiled output is self-contained in the target language
- **Exact by default in AI**: `dec` is the default numeric type for AI programs; IEEE 754 is opt-in
- **Bare metal first for AC+**: no OS assumed, no libc, no runtime — just the hardware
- **Error messages with personality**: `Preposterous:` prefix on compiler errors — intentional branding
- **Fast iteration**: `.lir` IR cache, unified pipeline, single binary workflow
