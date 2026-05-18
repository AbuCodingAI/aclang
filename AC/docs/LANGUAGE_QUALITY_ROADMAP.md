# AC Language: 7.4 → Low 9 Roadmap

Current rating: **7.4 / 10**  
Target: **8.8 / 10**

Each item below identifies a specific gap, explains why it costs points, and maps to the tasks in `.kiro/specs/compiler-performance-optimization/tasks.md`.

---

## AC Operator Reference (for this document)

| Concept | AC syntax | IR opcode |
|---------|-----------|-----------|
| Equality | `is` | `EQ` |
| Inequality | `#=` | `NEQ` |
| Multiply | `@` | `MUL` |
| AND | `&` or `and` | `AND` |
| OR | `or` | `OR` |
| NOT | `#` | `NOT` |
| XOR | `\|` | `XOR` |
| XNOR | `#\|` | `XNOR` |
| Inclusive range count | `xsub` | `XSUB` |
| Not-greater-than (≤) | `#>` | `LTE` |
| Not-less-than (≥) | `#<` | `GTE` |

Note: AC has no native modulus operator — use `xsub` for range arithmetic.

---

## Why It's at 7.4 Right Now

| Gap | Points lost | Root cause |
|-----|-------------|------------|
| For-loops broken in high-level backends (PY/JS/Java/RS/V/GO) | −0.5 | No high-level IR path for `FOR`; low-level labels get commented out |
| Legacy string IR path silently swallows bugs | −0.4 | `lowerExpr(string)` still fires as fallback even when structured AST exists |
| Type system exists but not connected to IR pipeline | −0.4 | `type.hpp` defines full `Numeral`/`Boolean`/`String`/`List`/`Tuple` hierarchy with backend name maps — but `ir.cpp` never calls it; everything is emitted as `i64` or string |
| Stack-only register allocation in BNY backend | −0.3 | Every value spills to stack; no register reuse |
| No optimization passes — zero constant folding or DCE | −0.3 | Code emitted verbatim; constants re-computed every iteration |
| Missing tests for most language features | −0.2 | Regressions go undetected |
| Incremental compilation not implemented | −0.2 | Full recompile on every change |
| Error messages lack source-line context | −0.2 | Line/col reported but no caret pointing to the actual problem |

---

## Stage 1 — Language Correctness (7.4 → 8.0)

These are bugs, not missing features. Nothing else matters until these are fixed.

### 1.1 High-Level For-Loop IR Path

**Impact: +0.4**

For-loops through Python, JS, Java, Rust, V, Go backends currently emit `// goto L0:` comments because the lowering uses the low-level label/jump path with no high-level equivalent. The result is silently wrong output — the loop body is unreachable.

**What to build:**
- `FOR_BEGIN` / `FOR_END` opcodes already exist in `IROpcode` but the high-level path never emits them
- In `IRGenerator::gen(ForLoop)`, detect `prog.useHighLevelIR` and emit `FOR_BEGIN {iterVar, collRef}` → body → `FOR_END` instead of the current LOAD_VAR / LOAD_INDEX hack
- In each high-level backend's strategy, implement `emitForBegin(iterVar, collection)` and `emitForEnd()`:
  - Python: `for {iterVar} in {collection}:`
  - JS: `for (let {iterVar} of {collection})`
  - Java: `for (long {iterVar} : {collection})`
  - Rust: `for {iterVar} in {collection}.iter()`
  - Go: `for _, {iterVar} := range {collection}`
  - V: `for {iterVar} in {collection}`

**Tasks.md refs:** §5.4 (CFG builder for FOR loops), §5.5 (no high-level flow in IR)

---

### 1.2 Remove Silent Legacy String Fallback

**Impact: +0.3**

`lowerExprNode()` falls back to `lowerExpr(n.value)` in `BinaryExpr` and elsewhere when it can't handle a node. This silently hides parse bugs — the wrong IR is emitted with no error. The structured path should be authoritative.

**What to build:**
- Audit every call site of `lowerExpr(string)` inside `gen()` and replace with either:
  - `lowerExprNode(*child)` for nodes that have structured children
  - An explicit `throw ACError(...)` for unrecognised node types
- Keep `lowerExpr` only for genuine legacy-string inputs (the `__fn__`, `__list__` etc. attrs that still come through as strings from the parser)
- Add a `[[deprecated]]` attribute or static assert to prevent new `lowerExpr` call sites from being added silently

**Tasks.md refs:** §3.9 (update IR generation to use integer references)

---

### 1.3 Fix ForLoop `LOAD_VAR` Semantic Bug

**Impact: +0.1**

`IRInstruction init(IROpcode::LOAD_VAR, iterT, {collRef})` is semantically "copy the collection to a temp" — not iterator construction. It accidentally works in low-level backends because the collection reference is passed through directly, but breaks the moment any backend tries to introspect the iterator type.

**What to build:**
- Resolved automatically once §1.1 (high-level for-loop path) is done — C and ASM stay on the low-level path where this doesn't matter
- Interim fix: replace `LOAD_VAR` with `LIB_CALL {mkConst("iter_init"), collRef}` to make intent explicit

---

## Stage 2 — Language Completeness (8.0 → 8.5)

### 2.1 Wire `type.hpp` into the IR Pipeline

**Impact: +0.4**

`type.hpp` already defines a complete type hierarchy:
- `Numeral`: PosInt / PosDec / NegInt / NegDec
- `Boolean`: True / False subtypes
- `String`, `List`, `Tuple` (immutable)
- Per-backend name maps: `toCpp()`, `toPy()`, `toJs()`, `toJava()`, `toGo()`, `toRs()`
- `inferNumeral(string)` — infers subtype from literal value

None of this is called by `ir.cpp` or `ir_codegen.cpp`. Everything is emitted as `i64` regardless of what `type.hpp` says.

**What to build:**

1. **In `ir.cpp` / `lowerExprNode`** — when lowering a `LiteralExpr`, call `Type::inferNumeral(value)` and attach the type to the `IRRef` constant. For `LiteralExpr` with type `"FLOAT"`, emit `IRRef::constant(IRValue(double))` not `mkConst(string)`.

2. **In `ir.hpp` / `IRValue`** — store a `Type` alongside the value variant so typed constants survive through the pipeline:
   ```cpp
   struct IRValue {
       IRType type;
       Type   acType;  // from type.hpp
       std::variant<int, double, std::string, bool> data;
   };
   ```

3. **In `ir_codegen.cpp`** — use `acType.toPy()`, `acType.toCpp()` etc. when declaring variables instead of always emitting `long long` / `int64`:
   - Python: `float` vs `int` based on `isDec()`
   - C/C++: `double` vs `int` based on `isDec()`
   - Rust: `f64` vs `i64` / `u64` based on subtype (already mapped in `toRs()`)
   - Java: `double` vs `long` based on `isDec()`

4. **In `exp_bny.cpp`** — when a slot holds a `PosDec` or `NegDec`, use `movsd`/`addsd`/`subsd` SSE2 instructions instead of integer `mov`/`add`/`sub`

**Tasks.md refs:** §13.1 (typed Machine IR operands), §13.2 (MachineIR builder)

---

### 2.2 Optimization Passes

**Impact: +0.3**

Constant folding is the most visible. Without it, the BNY binary emits full instruction sequences for constants computed at compile time. Even a simple `-O1` reduces output size measurably.

**What to build (in order of impact):**

1. **Constant folding** — fold two `LOAD_CONST` operands into a single `LOAD_CONST` result at IR generation time.
   AC operators to fold:
   - Arithmetic: `+`, `-`, `@` (multiply), `/`
   - Comparisons: `is` (==), `#=` (!=), `<`, `>`, `#>` (≤), `#<` (≥)
   - Logic: `&` (AND), `or` (OR), `#` (NOT), `|` (XOR), `#|` (XNOR), `xsub`

2. **Dead code elimination** — mark-and-sweep: start from instructions with side effects (`Term.display`, `CALL`, `STORE_VAR`, `HALT`), mark operand producers, remove everything else.

3. **Copy propagation** — `t0 = v1; t1 = t0; t2 = t1` → replace with `t2 = v1` directly. Reduces stack load/store pairs in BNY output.

**Tasks.md refs:** §9.1–9.9 (all optimization passes), §9.10 (pass manager with -O0/-O1/-O2/-O3)

---

### 2.3 Register Allocation (BNY Backend)

**Impact: +0.2**

Currently every temporary and variable spills to `[rbp - N*8]`. A Fibonacci(15) call allocates 40+ stack slots, most only live for one instruction. Linear-scan register allocation eliminates most of these.

**What to build:**
- Compute live intervals: for each `IRRef::TEMP`, find first-define and last-use instruction index
- Linear scan: sorted by start, assign from `{rax, rcx, rdx, r8, r9, r10, r11}` (caller-saved); spill to stack only on conflict
- Modify `FuncFrame` to return a `RegAssignment` map (temp_id → register or stack_offset) rather than always-stack
- Modify `compileInstr` to use registers when available

**Tasks.md refs:** §11.1–11.5 (live intervals, linear scan, spilling, calling convention)

---

### 2.4 Error Messages with Source Context

**Impact: +0.2**

Current format:
```
Preposterous: SyntaxError at line 3 char 10: Expected indentation
```

`Preposterous:` is the intentional error identity prefix (analogous to `Warning:` for warnings and `Suggestion:` for hints like the `<Foreign>` security notice). That stays. What's missing is the source line with a caret showing exactly where the problem is:

```
Preposterous: SyntaxError at line 3 char 10: Expected indentation
  |
3 |     WHILST x<5
  |             ^
```

**What to build:**
- Store the raw source lines in the parser/lexer context
- When `reportError()` fires, look up `line` in the source lines array and emit the offending line + `^` at column
- Add `Suggestion:` emission sites for known-fixable problems (e.g. using `==` instead of `is`, using `display` instead of `Term.display`)
- `Warning:` for non-fatal issues (e.g. unused variable, unreachable code after `break`)

**Tasks.md refs:** §17.1–17.3 (error recovery, partial AST, improved messages)

---

## Stage 3 — Production Quality (8.5 → 8.8–8.9)

### 3.1 DWARF Debug Information

**Impact: +0.15**

With `-g`, the BNY binary should be debuggable with `gdb`. This means `.debug_line`, `.debug_info`, and `.debug_abbrev` ELF sections containing source file, line numbers, and variable names.

**What to build:**
- `DWARFGenerator` class emitting the three mandatory sections
- `addLineInfo(ac_line, text_offset)` called at each `IRInstruction` emit
- `addVariable(name, symbol_id, rbp_offset)` for every `STORE_VAR`
- `addFunction(name, start_offset, end_offset)` for each `FUNC_BEGIN`/`FUNC_END`
- Inject sections into `writeELF` alongside `.text` and `.rodata`

**Tasks.md refs:** §15.1–15.2 (DWARF generator, -g flag)

---

### 3.2 Incremental Compilation / Caching

**Impact: +0.1**

Every `./ac file.ac` re-parses and re-lowers everything from scratch. With caching, unchanged functions don't get re-compiled.

**What to build:**
- Hash source file + backend string → cache key
- Serialize IR to binary (tasks.md §19.9 for IR serialization format)
- On recompile, check if hash matches; if yes, deserialize and skip parse + IR generation
- Store cache in `~/.ac/cache/`

**Tasks.md refs:** §19.2–19.3 (incremental compilation), §19.9–19.10 (IR serialization)

---

### 3.3 Complete Test Suite

**Impact: +0.1**

There are no tests for: IR correctness, optimization passes, BNY output correctness, or backend compatibility. Regressions in one backend are invisible until someone manually runs it.

**What to build:**
- `test/test_backends.py`: for each AC snippet, compile to all 11 backends, run the output, assert expected value
- `test/test_bny.py`: compile small AC programs to BNY, run the binary, assert stdout
- `test/test_ir.py`: generate IR from AST snippets, assert opcode sequences
- `test/test_types.py`: verify `type.hpp` inference wires through correctly to each backend's declared types
- CI: run on every commit (GitHub Actions or similar)

**Tasks.md refs:** §19.5–19.6 (benchmark suite, backward compatibility), §19.11 (end-to-end integration tests)

---

### 3.4 `--all` Flag — Compile to Every Backend

**Impact: +0.05** *(already implemented)*

`ac file.ac --all` (or `-all`) compiles the source to all registered backends in a single invocation, writing one output file per backend. Useful for cross-platform distribution, regression testing all backends at once, and verifying that a change doesn't break any target.

**How it works:**
- AST is parsed once and cached (`.acc`); each backend re-uses it
- IR is generated independently per backend (backends differ in `useHighLevelIR`, type mappings, etc.)
- Backends are iterated in alphabetical order for deterministic output
- BNY backend failure (non-Linux) is non-fatal; remaining backends still compile
- Exit code is 0 only if all backends succeed

**Usage:**
```
ac program.ac --all             # compile to all 12 backends
ac program.ac --all --force     # skip .acc cache
```

**Implementation:** `main.cpp` — `compileAll` flag, `compileOne` lambda, `BackendRegistry::getBackendNames()` iteration.

---

### 3.5 Benchmark Suite

**Impact: +0.05**

The benchmark programs already exist in `examples/` but there is no automated runner comparing AC output to reference implementations.

**What to build:**
- `benchmarks/` directory with `fibonacci.ac`, `factorial.ac`, `array_sum.ac`, `matrix_mult.ac`, `quicksort.ac`
- `bench.sh`: compile each to BNY + to C, run both, compare stdout and measure timing
- Add `make bench` target

**Tasks.md refs:** §19.4–19.5 (benchmark suite, performance measurement)

---

## Priority Order for Maximum Rating Gain

| Priority | Item | Est. effort | Rating gain | Status |
|----------|------|-------------|-------------|--------|
| 1 | §1.1 High-level for-loop IR path | 2–3 days | +0.4 | ✅ done |
| 2 | §2.1 Wire `type.hpp` into IR pipeline | 2–3 days | +0.4 | ✅ done (float propagation in all backends) |
| 3 | §1.2 Remove silent legacy fallback | 1 day | +0.2 | ✅ done |
| 4 | §2.2 Constant folding | 1–2 days | +0.2 | ✅ done |
| 5 | §2.4 Error messages with source context | 1 day | +0.2 | ✅ done |
| 6 | §2.3 Register allocation | 3–4 days | +0.2 | ✅ done (linear scan in BNY) |
| 7 | §3.1 DWARF debug info | 3–4 days | +0.15 | ✅ done |
| 8 | §2.2 DCE + copy propagation | 2–3 days | +0.1 | ✅ done |
| 9 | §3.3 Test suite | 2 days | +0.1 | ✅ done (test/test_backends.py: 69 pass, 0 fail) |
| 10 | §3.2 Incremental compilation | 3–4 days | +0.1 | ✅ done |
| — | §3.4 `--all` flag | done | +0.05 | ✅ done |

**Cumulative gain: +2.10** → lands at approximately **9.0 / 10** (all priority items complete)

---

## What Will NOT Get It to 10

Even after all of the above:

- Standard library exists (`library/` — camera, event_listener, gui, keybinds, speech-text, widgets) but lacks a formal module system to `use` libraries from AC source files
- No type inference or generics
- No module system / `use` keyword (libraries can't be imported from AC programs yet)
- No REPL
- No LSP for IDE support
- Single-file programs only (no multi-file compilation)

Those are 9.5+ territory and require the language to stabilize API-first before implementation.

---

## How This Maps to tasks.md

| tasks.md phase | Items here | Current status |
|----------------|-----------|----------------|
| Phase 3 — CFG Lowering | §1.1, §1.3 | Not started |
| Phase 5 — Optimization | §2.2 (folding/DCE/copy prop) | Not started |
| Phase 6 — Register Alloc | §2.3 | Not started |
| Phase 7 — Machine Code | §2.1 (type wiring for floats) | Partial (`type.hpp` exists, not connected) |
| Phase 8 — Debug Info | §3.1 | Not started |
| Phase 9 — Error Handling | §2.4 | Partial (line/col exists, no caret) |
| Phase 10 — Integration | §1.2, §3.2, §3.3, §3.4 | Not started |
