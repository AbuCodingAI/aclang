# AC Language — 0.2 → 0.3 Changelog

Full accounting of every change from the published npm package `aclang@0.2` to the current `0.3` compiler source.

---

## Language — New Syntax

### `alias` — Bidirectional Live Binding

```ac
alias a = x
a = 99      /* x becomes 99 */
x = 42      /* a becomes 42 */
```

`alias` registers a bidirectional write-through binding between two names. Assignments to either name propagate to all names in the group. Alias groups are **transitive**: `alias b = a` after `alias a = x` creates a 3-way group where writing any one name writes all three.

This is assignment-level aliasing — it does not apply to subfields or list elements.

---

### Error/Warning/Clause System

```ac
raise ERR                       /* Preposterous: Fatality occurred */
raise ERR($fatal message$)      /* Preposterous: fatal message */
raise hint($try this$)          /* Suggestion: try this */
raise toxic($warning$)          /* Toxic: warning */
raise Praise($Nice code$)       /* Praise: Nice code */
```

Rules:
- `raise ERR` (bare) → `Preposterous: Fatality occurred` + abort
- `raise hint(...)` → `Suggestion: ...` to stderr (non-fatal)
- `raise toxic(...)` → `Toxic: ...` to stderr (non-fatal)
- Any other name → `Name: message` verbatim (user-defined clause)
- Reserved: `hint`, `toxic`, `ERR` have fixed prefixes; everything else is user-definable
- Users may raise `Hint`, `Err`, `Toxic` (capitalized) as custom verbatim clauses

**Compiler-side Toxic warnings** (emitted automatically, not from source):

```
Toxic: Compiler is slacking off — 'x' assigned but never read
Toxic: Compiler is slacking off — 'f' defined but never called
```

Suppressed when the backend is `AC LIB` or `AC->LIB`.

---

### `math.LongInt` — 95-bit Exact Integer

```ac
use ilib math

<mainloop>
    math.LongInt big = math.pow(2, 68)
    Term.display big                      /* 295147905179352825856 */
    math.LongInt max = math.pow(2, 95) - 1
    Term.display max                      /* 39614081257132168796771975167 */
    math.LongInt over = math.pow(2, 95)
    Term.display over                     /* inf */
<mainloop>
```

- Values ≤ `INT64_MAX` are stored as `INT` and work natively on all backends including BNY.
- Values > `INT64_MAX` and ≤ `2^95 - 1` are stored as exact decimal strings (Python prints them exactly; BNY uses a `__ac_print_cstr__` runtime helper).
- Values exceeding the 95-bit ceiling store `inf`.
- Normal AC math that overflows signed 64-bit folds to `inf` at compile time.

Supported LongInt operations at compile time: `+`, `-`, `*`, `@`, `/`, `%`, `mod`, `pow`, `math.pow`, `mod_int`, `math.mod`.

---

### Raw Strings — `r$...$`

```ac
raw = r$no \n escapes here$
```

`r$...$` copies bytes verbatim with no escape processing. `$...$` continues to process `\n`, `\t`, `\r`, `\\`, `\$`.

---

### `lazy_eval` — Safe Deferred Evaluation

```ac
result = lazy_eval(expr)
```

Wraps the expression in a per-backend safe container. Returns the value on success, or an error object on failure — never aborts.

| Backend | Wrapper |
|---------|---------|
| Python | `try/except` |
| JS, C++, Java | `try/catch` |
| Rust | `catch_unwind` |
| Go | `recover` closure |
| C, V | plain assignment (no exception system) |

`eval(expr)` existed in 0.2. Only `lazy_eval` is new in 0.3.

---

### Whitespace Sentinel — `\ws`

```ac
ws = \ws
```

`\ws` tokenizes to a whitespace sentinel string used by regex and string helpers.

---

### `mod` Infix Keyword

```ac
c = i mod 7
```

`mod` is now recognized as an infix operator at the same precedence as `%`. Both `i mod 7` and `i % 7` compile to the `MOD` IR opcode.

---

### Extended Slash Commands

0.2 only handled `/kill`. 0.3 adds:

| Command | Behavior |
|---------|----------|
| `/kill` | Hard terminate (unchanged) |
| `/stop` | Graceful stop / clean exit |
| `/end` | Break out of enclosing loop; exit if at top level |
| `/restart` | Rerun program body once from top, skip on second pass |
| `/halt n` | Pause execution for `n` seconds |
| `/halt math.inf` | Treated as `/stop` |

`break` was intentionally removed from the keyword table. Use `/end` to break out of a loop.

---

### `using header` / `using namespace`

```ac
using header math
using namespace ilib
```

`using header X` brings all symbols from library `X` into flat scope (e.g. `sin(x)` instead of `math.sin(x)`).

`using namespace ilib` promotes all imported ilib libraries into flat scope.

---

### `flib` and `datac` Import Types

```ac
use flib /path/to/lib.so
use flib mylib.ac
use datac data.datac as db
```

Two new import prefixes:
- `flib` — foreign compiled library path or `.ac`/`.ai` module. AC source flib files are parsed and injected directly into the AST.
- `datac` — compile-time data file import.

Full import table:

| Prefix | Meaning |
|--------|---------|
| `ilib` | Built-in AC libraries (`library/ilib/`) |
| `elib` | External packages (atar/package manager) |
| `clib` | Custom computer libraries |
| `flib` | Foreign compiled or AC library path |
| `datac` | Compile-time `.datac` data file |

---

### `AC LIB` Backend Form

```ac
AC LIB
```

Space-form backend declaration. Marks an AC library source file — no mainloop required. `AC->LIB` builds a shared library (`.so`/`.dll`). Toxic warnings for unused functions are suppressed in LIB mode.

---

### Complex Numbers — `math.i`

```ac
using header math

z = 3 + 4 * math.i
Term.display z          /* (3+4j) in Python */
Term.display math.re(z) /* 3.0 */
Term.display math.im(z) /* 4.0 */
```

`math.i` is the imaginary unit. `math.re(z)` and `math.im(z)` extract the real and imaginary parts.

---

### New Math Constants and Precision Variants

Added in 0.3:

| Constant | Value |
|----------|-------|
| `math.phi` | Golden ratio (1.618…) |
| `math.tau` | 2π (6.283…) |
| `math.em` | Euler–Mascheroni constant (0.577…) |
| `math.inf` | Positive infinity |
| `math.i` | Imaginary unit |

Precision-style constants (return the constant to `n` decimal places):

```ac
Term.display math.pi(50)
Term.display math.e(50)
Term.display math.phi(50)
```

---

### `bundle` Class Construct

```ac
bundle Point
    x = 0
    y = 0
    Make dist func(self)
        return math.sqrt(self.x * self.x + self.y * self.y)
```

`bundle` defines class-like data with methods. Backend lowering is ongoing.

---

### `range` / `sequence` in Expression Position

```ac
FOR i in range 5
    Term.display i

FOR n in sequence(3, 7)
    Term.display n

nums = range 5
```

`range N` produces `0..N-1`. `sequence(x, y)` is inclusive `x..y`.

---

### `save as` and `destroy`

```ac
save as output.txt
destroy x
free x, y, z
```

`save as` records a save directive. `destroy` emits a free/remove. `free` declares names as globally scoped.

---

## Compiler — Optimization Passes

### Constexpr Function Folding

Pure user-defined functions called with all-constant arguments are evaluated at compile time. The interpreter handles:
- Arithmetic and comparison opcodes
- IF/ELSE/END conditional branches
- WHILST loops (both low-level LABEL/JF/JUMP and high-level WHILE_BEGIN/WHILE_END)
- Recursive calls (depth limit: 64)
- Step limit: 15M operations per evaluation

Multi-pass (3 iterations) so nested calls like `square(square(2))` → 16 fully fold.

```ac
/* All seven functions fold at compile time — the binary just prints */
fib = fibonacciIter(45)     /* 1134903170     */
sum = sumNumbers(1000000)   /* 500000500000   */
```

Compile time (full 7-function benchmark): ~1.6s. Run time: 0.00s.

### Scalar Math Constexpr

Known `ilib math` calls with constant arguments fold at compile time:

```ac
x = math.sin(0)    /* → 0  */
y = math.sqrt(81)  /* → 9  */
```

### `--runtime` Flag

Disables all constexpr folding so functions run at actual runtime:

```bash
ac file.ac --runtime --time
```

Use this when benchmarking real throughput or when calling functions with runtime-variable arguments.

### Toxic Unused-Symbol Warnings

Collected pre-optimization (before constexpr removes CALL instructions) so constexpr-inlined functions are not falsely flagged:

- Variables assigned but never read
- Functions defined but never called
- Bundles defined but never instantiated

Suppressed in `AC LIB` / `AC->LIB` output.

---

## Compiler — IR Changes

### Integer Width: `int` → `int64_t`

`IRValue` integers widened from 32-bit `int` to 64-bit `int64_t`. Fixes overflow for large accumulators in benchmark-style programs.

### New IR Opcodes

| Opcode | Purpose |
|--------|---------|
| `ALIAS_DECL` | Register bidirectional alias group |
| `RAISE_CLAUSE` | Emit "Clause: msg" to stderr |
| `LAZY_EVAL` | Safe deferred evaluation wrapper |
| `TAG_BEGIN` / `TAG_END` | Named tag block boundaries |
| `SOFT_HALT` | `/stop` — graceful exit |
| `RESTART_PROGRAM` | `/restart` marker |
| `SLEEP` | `/halt n` — pause |

### Unified IR Codegen

All backends now share a single `UnifiedIRCodeGen` class with per-backend `BackendStrategy` implementations. 0.2 had independent code generators per backend.

### `.irc` Binary Cache (new)

Binary IR cache at version `v12` (`ac-irc-v12-longint-alias`). One file per backend. Cache key includes: source hash, backend, compiler version marker, library FFI mtimes, and `--runtime` flag. Stale caches invalidate when any library FFI file changes.

### `.lir` Gated to BNY/ASM

Human-readable IR text is now only saved for BNY and ASM backends. Higher-level backends (PY, JS, C++, etc.) no longer write `.lir` since it serves no debug purpose there.

### `display` Removed from Keywords

`display` was incorrectly a keyword in 0.2. It is now a plain identifier — `Term.display` is parsed as a method call on the `Term` object.

### First-Byte Fast-Path in Lexer

Keyword lookup now uses a 256-entry `kwFirstByte` table. If the first byte of a word is not the starting byte of any keyword, the full hash lookup is skipped.

---

## Compiler — CLI Flags

| Flag | Added in 0.3 | Notes |
|------|-------------|-------|
| `--runtime` | ✅ | Disable constexpr folding |
| `--time` / `-time` | ✅ | Report compilation and run time |
| `--stop-after-ir` | ✅ | Print LIR and exit |
| `--stop-after-cfg` | ✅ | BNY debug stop |
| `--stop-after-ssa` | ✅ | BNY debug stop |
| `--stop-after-opt` | ✅ | BNY debug stop |
| `--allow-foreign` | ✅ | Enable `<Foreign>` blocks |
| `--save-ast` | ✅ | Explicit `.acc` save |
| `--save-ir` | ✅ | Explicit `.lir` save |
| `--no-run` | ✅ | Compile only (was `-c` in 0.2) |
| `LIB` backend | ✅ | Added to `--target` list |

---

## BNY Backend — Improvements

| Feature | Details |
|---------|---------|
| Float printing | `__ac_print_double__` helper for `xmm0` float values |
| `__ac_print_cstr__` | Null-terminated string printer (hand-rolled strlen + `write` syscall) for LongInt strings > int64 |
| Math constant PLT stubs | `math.pi`, `math.e`, `math.phi`, `math.tau`, `math.em`, `math.inf` via PLT dispatch |
| `mod` opcode | `MOD` emits `idiv`/`cdq` + `mov rax, rdx` correctly |
| `movzx_r64_ptr8` | New X64Emitter instruction for byte-from-memory loads |
| String variable tracking | `stringVarNames_` set to route LongInt display to cstr printer |

**Benchmark (0.3, same 7-function program):**

| Mode | Time |
|------|------|
| BNY with `--runtime` | 0.01s |
| BNY without `--runtime` (constexpr) | 0.00s run / 1.6s compile |
| Python with `--runtime` | 0.45s |
| C (gcc -O2) | 0.008s |

---

## Standard Library

### New Libraries

| Library | Functions |
|---------|-----------|
| `os` | `bash`, `sbash`, `app_open`, `mkfile`, `rmfile`, `mkdir`, `rmdir`, `exists`, `cwd`, `env`, `write_to`, `append_to`, `read` |
| `regex` | `test`, `match`, `search`, `replace`, `replace_all`, `count`, `find_all`, `groups`, `split`, `escape` |
| `string-cheese` | AC string methods library |

**Note:** `os.sbash(cmd)` is a safer shell helper that rejects `sudo`, `su`, `nohup`, `screen`, `tmux`, `disown`, shell functions, and trailing `&`.

### Math Library Additions

| Addition | Notes |
|----------|-------|
| `math.phi` | Golden ratio |
| `math.tau` | 2π |
| `math.em` | Euler–Mascheroni constant |
| `math.inf` | Infinity |
| `math.i` | Imaginary unit (`1j`) |
| `math.re(z)` / `math.im(z)` | Complex part extraction |
| `math.pi(n)` / `math.e(n)` / `math.phi(n)` | n-decimal precision |
| `stat.*` | `avg`, `median`, `q1`, `q3`, `mode`, `min`, `max` |
| `sigma`, `product`, `gradient` | Aggregate functions |
| `LongInt` | 95-bit exact integer declaration |

### Removed / Reorganized

- `event_listener` ilib module removed (event system is now a parser-level construct)
- `files` ilib module removed (file I/O consolidated into `os`)

---

## Error Messages

| Prefix | Meaning |
|--------|---------|
| `Preposterous:` | Parse or runtime fatal error |
| `Toxic: Compiler is slacking off —` | Compiler-side unused-symbol warning |
| `Suggestion:` | Non-fatal hint raised by `raise hint(...)` |
| `Toxic:` | Non-fatal warning raised by `raise toxic(...)` |
| `Clause:` | User-defined clause output |

All parse errors continue to show the offending source line with a `^` pointer (introduced in 0.2).

---

## Tags

Added in 0.3:

| Tag | Notes |
|-----|-------|
| `<Local>` | Local scope block |
| `<EndHere>` | Closes `<StartHere>` (was documented but not consistently recognized) |
| `<Foreign>` | Raw target-language passthrough; requires `--allow-foreign` |
| `<shutoff>` | Shutoff hook block |
| Custom tags via `def tag <name>` | Now fully supported in parser |

BNY rejects `<Foreign>` blocks:
```
Toxic: User attempts fluency in CPU
```

---

## VSCode Extension (0.3.0)

Full grammar rewrite. Everything in the README is now highlighted.

### New rules added

| Rule | What it covers |
|------|---------------|
| `raw-string` | `r$...$` — distinct scope from `$...$` |
| `ws-sentinel` | `\ws` whitespace sentinel |
| `def-tag` | `def tag <name>` custom tag declarations |
| `longint-decl` | `math.LongInt` as a storage type |
| `alias-decl` | `alias a = b` — both names as variables |
| `raise-clause` | `raise ERR\|hint\|Hint\|toxic\|Toxic` with the clause type highlighted |

### Extended rules

| Rule | What changed |
|------|-------------|
| `backend-declaration` | Adds `AC LIB` (space form) and `AC->C++` (literal `+`) |
| `tag` | Adds `<Local>`, `<EndHere>`, `<Free>` |
| `builtin-io` | Adds `Term.print`, `Term.log`, `Term.write`; `lazy_eval`, `animate`, `is_obj` |
| `constants` | Case-insensitive: `TRUE`, `true`, `FALSE`, `false` |
| `operators` | Adds `*` (numeric multiply), `|=` (XOR assign), `mod` keyword, `AND`/`OR`/`NOT` uppercase |
| `types` | Adds `fn`; moves `not`/`NOT` to operators |
| `library-call` | Adds `files`, `keybinds` namespaces |
| `import` | Adds `dict`, `header` keywords |
| `function-decl` | Requires `func` or `object` after name to avoid false positives |

---

## Cache Files

| File | 0.2 | 0.3 |
|------|-----|-----|
| `.acc` | `ACC1` / `ACC_VERSION = 5` | `ACC1` / `ACC_VERSION = 6` |
| `.irc` | Not present | Binary IR cache, `IRC_VERSION = 12` |
| `.lir` | Saved for all backends | Saved for BNY/ASM only |

Current IRC marker: `ac-irc-v12-longint-alias`

---

## Breaking Changes from 0.2

1. **`break` is not a keyword** — use `/end` to exit a loop.
2. **Type coercions renamed**: `dec X` → `to_dec X`, `int X` → `to_int X`, `string X` → `to_string X`, `bool X` → `to_bool X`.
3. **`display` is no longer a keyword** — it is a plain identifier. Code using bare `display(...)` as a function call still works since it is now treated as a user function call. `Term.display` is unaffected.
4. **`.irc` cache files** — first compile after upgrading will regenerate all caches.
5. **ELSIF** renamed to **ELSEIF** in parser (both accepted, but ELSEIF is canonical in 0.3).
