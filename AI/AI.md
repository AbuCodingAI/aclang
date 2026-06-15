# AI — AC Interpreted

AI is the interpreted sibling of AC. Where AC compiles ahead-of-time to static targets, AI compiles to **AI Bytecode (.aibc)** — a compact, platform-independent binary format executed by the **AIVM** (AI Virtual Machine). The model is intentionally close to Java: `ai` compiles `.ai` source to `.aibc`, and the AIVM loads and runs `.aibc` files.

AI adds one primitive type — `dec` — that makes exact decimal arithmetic a first-class citizen with no floating-point error.

---

## 1. Relationship to AC and Java

| Property          | AC                              | AI                                    | Java                        |
|-------------------|---------------------------------|---------------------------------------|-----------------------------|
| Execution model   | Compiled (AOT)                  | Bytecode → AIVM                       | Bytecode → JVM              |
| Intermediate file | none (direct output)            | `.aibc` (AI Bytecode)                 | `.class`                    |
| Runtime           | OS / language runtime           | AIVM (`aivm` binary)                  | JVM (`java` binary)         |
| Extra types       | —                               | `dec` (exact decimal)                 | `BigDecimal`                |
| Float arithmetic  | IEEE 754                        | `dec` eliminates it                   | IEEE 754 (unless BigDecimal)|
| Syntax base       | AC grammar                      | Same AC grammar                       | different                   |
| Target decl       | `AC->PY`, `AC->C`, etc.         | `AI->VM`                              | —                           |

---

## 2. Compilation Pipeline

```
AI source  (mycode.ai  or  .ac with  AI->VM)
     │
     ▼
AC Lexer + Parser              (shared — ac-compiler/src/lexer.cpp, parser.cpp)
     │  AST
     ▼
AI IR lowering                 (ai-compiler/src/ai_ir.cpp — same structure as AC IR)
     │  AI IR
     ▼
ai_codegen.cpp                 (AI Bytecode emitter)
     │
     ▼
mycode.aibc                    (AI Bytecode file — platform-independent binary)
     │
     ▼
AIVM  (aivm mycode.aibc)       (loads .aibc, interprets or JIT-compiles and executes)
```

### 2.1 Usage (mirrors Java)

```sh
ai   mycode.ai          # compile → mycode.aibc, then run in AIVM (like java MainClass)
ai   mycode.ai --no-run # compile only → mycode.aibc
aivm mycode.aibc        # run an already-compiled .aibc file
```

### 2.2 The `.aibc` Format

`.aibc` is a binary container modelled after `.class` files:

```
Header:
  magic       4 bytes   0x41 0x49 0x42 0x43  ("AIBC")
  version     2 bytes   format version (currently 1)
  flags       2 bytes   reserved

Constant pool:
  count       2 bytes
  entries[]   variable  (string literals, integer constants, symbol names)

Function table:
  count       2 bytes
  entries[]   name_ref(2) + bytecode_offset(4) + bytecode_len(4) + local_count(2)

Bytecode section:
  raw opcode stream (see §2.3)
```

This is compact enough to distribute, language-agnostic, and straightforward to interpret or JIT.

### 2.3 AIVM Bytecode Instruction Set

The bytecode is register-based (like Lua 5), not stack-based. Each instruction is 4 bytes.

| Opcode | Mnemonic       | Operands      | Effect                                  |
|--------|----------------|---------------|-----------------------------------------|
| 0x01   | `LOAD_CONST`   | dst, cpool_i  | `reg[dst] = constant_pool[cpool_i]`     |
| 0x02   | `LOAD_VAR`     | dst, var_i    | `reg[dst] = locals[var_i]`              |
| 0x03   | `STORE_VAR`    | var_i, src    | `locals[var_i] = reg[src]`              |
| 0x10   | `ADD`          | dst, a, b     | `reg[dst] = reg[a] + reg[b]`            |
| 0x11   | `SUB`          | dst, a, b     | `reg[dst] = reg[a] - reg[b]`            |
| 0x12   | `MUL`          | dst, a, b     | `reg[dst] = reg[a] * reg[b]`            |
| 0x13   | `DIV`          | dst, a, b     | `reg[dst] = reg[a] / reg[b]`            |
| 0x20   | `CMP_EQ`       | dst, a, b     | `reg[dst] = (reg[a] == reg[b]) ? 1 : 0` |
| 0x21   | `CMP_LT`       | dst, a, b     | less-than                               |
| 0x22   | `CMP_GT`       | dst, a, b     | greater-than                            |
| 0x30   | `JMP`          | offset(2)     | `pc += offset`                          |
| 0x31   | `JZ`           | src, offset   | `if reg[src]==0: pc += offset`          |
| 0x40   | `CALL`         | fn_i, argc    | call function table entry fn_i          |
| 0x41   | `RETURN`       | src           | return `reg[src]`                       |
| 0x50   | `DISPLAY`      | src           | print `reg[src]` to stdout              |
| 0x51   | `INPUT`        | dst, prompt   | read line → `reg[dst]`                  |
| 0x60   | `DEC_NEW`      | dst, cpool_i  | parse decimal literal → dec handle      |
| 0x61   | `DEC_ADD`      | dst, a, b     | exact decimal addition                  |
| 0x62   | `DEC_SUB`      | dst, a, b     | exact decimal subtraction               |
| 0x63   | `DEC_MUL`      | dst, a, b     | exact decimal multiplication            |
| 0x64   | `DEC_DIV`      | dst, a, b     | exact decimal division (precision 20)   |
| 0x65   | `DEC_DISPLAY`  | src           | print dec value exactly                 |

### 2.4 AIVM Execution

The AIVM interpreter loop reads one instruction at a time and dispatches on the opcode. For hot functions (called more than a configurable threshold), the AIVM can JIT-compile the bytecode using the **relic ASM and BNY codegens** from `ac-compiler/relics/` — the same native code generators used by AC. The JIT path is:

```
bytecode function
     │
     ▼
Reconstruct mini-AST from bytecode (ai-vm/src/jit.cpp)
     │
     ▼
relic codegen_asm.cpp  →  AT&T x86-64 assembly text
     │
     ▼
relic codegen_bny.cpp  →  gcc -no-pie → ELF bytes
     │
     ▼
mmap(PROT_READ|PROT_EXEC) → native callable
```

The JIT result is cached in the AIVM's function table so subsequent calls go direct to native code.

---

## 3. The `dec` Type

### 3.1 Concept

A `dec` value is a **dictionary from base-10 exponent (integer) to digit (0–9)**. The key `k` represents the 10^k place.

```
42.7   →  { 1:4,  0:2,  -1:7 }
           ^10s  ^1s   ^tenths

0.1    →  { -1:1 }

314    →  { 2:3,  1:1,  0:4 }
```

Keys are always plain integers — no floats anywhere in the representation. The decimal point exists if and only if any key `k < 0` is present. The representation is sparse (trailing zeros omitted). Each digit value is exactly 0–9; carry propagates on mutation.

### 3.2 Syntax

```ac
AI->VM

x = dec 42.7          -- literal → dec type
y = dec 0.1
z = x + y
Term.display z         -- prints 42.8 exactly

price = dec 19.99
tax   = dec 1.08
total = price @ tax
Term.display total     -- 21.5892 exactly
```

`dec` is a type annotation on a literal or assignment. Once a variable is `dec`, all arithmetic on it stays in `dec`. A bare integer mixed with a `dec` operand is promoted automatically.

### 3.3 Internal Representation (C++)

```cpp
// In the AIVM host (ai-vm/src/dec.hpp)
using DecValue = std::map<int, int>;  // exponent → digit (0..9), sparse

DecRef dec_new(int exponent, int digit);
DecRef dec_from_literal(const char* s);   // parses "3.14" → {1:3, 0:1, -1:4}
DecRef dec_add(DecRef a, DecRef b);
DecRef dec_sub(DecRef a, DecRef b);
DecRef dec_mul(DecRef a, DecRef b);
DecRef dec_div(DecRef a, DecRef b, int precision);
void   dec_display(DecRef a);
int    dec_cmp(DecRef a, DecRef b);       // -1 / 0 / 1
```

`DecRef` is an opaque `void*` handle into the AIVM's dec heap.

### 3.4 Arithmetic — How It Stays Exact

**Addition:**
```
carry = 0
for k from min_exponent to max_exponent + 1:
    digit = a[k] + b[k] + carry
    result[k] = digit % 10
    carry     = digit / 10
if carry > 0: result[max_exponent+1] = carry
prune zeros
```
Every step is integer arithmetic. No float register touched.

**Why `0.1 + 0.2 = 0.3`:**
- `dec 0.1` → `{-1:1}`
- `dec 0.2` → `{-1:2}`
- add at exponent -1: `1 + 2 = 3`, no carry
- result: `{-1:3}` → displays as `0.3`

**Division:** long division to configurable precision (default 20 fractional digits). A `Suggestion: non-terminating` warning is issued if truncation occurs.

---

## 4. File Layout

```
ai-compiler/
  src/
    main.cpp          -- `ai` binary: parse → IR → emit .aibc
    ai_ir.cpp         -- AI IR lowering from AST
    ai_codegen.cpp    -- .aibc bytecode emitter
  include/
    ai_ir.hpp
    aibc.hpp          -- .aibc format constants and writer

ai-vm/
  src/
    main.cpp          -- `aivm` binary: load .aibc → interpret or JIT
    interpreter.cpp   -- AIVM bytecode dispatch loop
    jit.cpp           -- hot-path: bytecode → mini-AST → relic ASM+BNY → mmap
    dec.cpp           -- DecValue arithmetic (dec_add, dec_mul, dec_div, ...)
    dec_heap.cpp      -- handle-based allocator for DecRef values
  include/
    dec.hpp           -- DecRef, dec_* extern "C" declarations
    jit.hpp
  tests/
    test_dec.cpp      -- unit tests: 0.1+0.2=0.3, 19.99*1.08=21.5892, etc.
    test_aivm.cpp     -- compile+run small AI programs, check output

relics used by JIT (read-only):
  ac-compiler/relics/codegen_asm.cpp    -- AST → x86-64 assembly
  ac-compiler/relics/codegen_bny.cpp    -- assembly → native binary bytes
  ac-compiler/relics/base_codegen.hpp

shared with AC (symlinked or path-included):
  ac-compiler/src/lexer.cpp
  ac-compiler/src/parser.cpp
  ac-compiler/include/ast.hpp
  ac-compiler/include/token.hpp
  ac-compiler/include/type.hpp
```

---

## 5. `dec` Examples

```ac
AI->VM

-- The classic:
a = dec 0.1
b = dec 0.2
c = a + b
Term.display c         -- 0.3

-- Money:
price = dec 19.99
tax   = dec 1.08
total = price @ tax
Term.display total     -- 21.5892

-- No precision lost on whole-number arithmetic:
big = dec 999999999999999999
one = dec 1
Term.display big + one -- 1000000000000000000

-- Division (terminates):
x = dec 1
y = dec 4
Term.display x / y     -- 0.25

-- Division (non-terminating, truncated at precision 20):
p = dec 1
q = dec 3
Term.display p / q     -- 0.33333333333333333333
                       -- Suggestion: non-terminating decimal, truncated at 20 digits
```

---

## 6. Implementation Plan

### Phase 1 — `dec` runtime in isolation
- `dec.hpp` / `dec.cpp`: `DecValue = std::map<int,int>`, all four operations, display
- Unit test: `0.1 + 0.2 == 0.3`, `19.99 * 1.08 == 21.5892`, `1/3` truncation warning

### Phase 2 — `.aibc` format and emitter
- `aibc.hpp`: magic, version, constant pool, function table, opcode definitions
- `ai_codegen.cpp`: walk AI IR → emit binary `.aibc` stream
- Round-trip test: emit + parse a simple program

### Phase 3 — AIVM interpreter
- `interpreter.cpp`: register-based dispatch loop over AIVM opcodes
- `dec` opcodes dispatch into `dec_*` runtime functions
- End-to-end: `ai mycode.ai` → `.aibc` → AIVM prints correct output

### Phase 4 — JIT tier (optional, for performance)
- `jit.cpp`: hot-call counter per function; above threshold, reconstruct mini-AST from bytecode, run relic ASM+BNY, mmap result
- JIT result replaces interpreter for that function entry
