# AC v0.3.1 — Work Queue

Implementation status tracker for **AC v0.3.1** (bugfix / polish release).

Legend:
- **DONE (0.3.1)**: implemented and shipped in v0.3.1
- **PARTIAL (0.3.1)**: some work landed in v0.3.1, but the full item is not finished
- **TODO (post-0.3.1)**: not implemented yet

---

## 1. Typed `catch` Clauses — **DONE (0.3.1)**

Currently `catch` is all-or-nothing. Add per-type catch:

```ac
try
    result = 10 / 0
catch ZeroDivisionError
    Term.display $division by zero$
catch UserRaise
    Term.display $user raised something$
catch
    Term.display $anything else$
after
    Term.display $always runs$
```

- `catch` bare = catch-all (existing)
- `catch TypeName` = catch a specific error class
- Multiple `catch` clauses are allowed; they are tested in order.
- Backend support depends on target; backends without typed catches treat `catch TypeName` as a catch-all.

---

## 2. `using math.sin` — Single-Function Import — **DONE (0.3.1)**

Currently `using header math` pulls the entire math namespace flat. Add single-symbol import:

```ac
using math.sin
using math.sqrt
```

Brings only `sin` and `sqrt` into flat scope. `sin(x)` works without `math.` prefix.
Multiple names on one line or separate lines both valid.

---

## 3. LongInt 96-bit Arithmetic (signed) — **PARTIAL (0.3.1)**

`math.LongInt` is **signed 96-bit**. Bounds are:

- Minimum: `-(2^95)`
- Maximum: `(2^95 - 1)`

Design target: represent LongInt as three 32-bit limbs (lower, middle, upper):

```
96 bits = lower[31:0] | middle[63:32] | upper[95:64]
```

Operations to implement in the BigNat engine:
- `+`, `-`, `*`, `@` — supported for compile-time folding within signed 96-bit bounds
- `/`, `%`, `mod` — need proper multi-limb division
- `pow(a, b)` — repeated squaring within 96-bit
- Comparison: `<`, `>`, `is`, `#=`
- `math.LongInt` variables should survive runtime (not just compile-time folding)
- BNY backend: represent LongInt as **three 32-bit limbs** (32+32+32 = 96). Do not confuse this with HugeInt (3×64 = 192).

Design note:
- Decide overflow behavior (wrap vs clamp vs raise). Avoid “fold to `inf`” for integer types unless there is already a hard dependency on that behavior.

Future roadmap:
- `BigInt` (64+64 bits)
- `HugeInt` (64+64+64 bits; scale LongInt by 2)
- `MemInt` (allocates resources until safeguards / target size)

---

## 4. AI / AC Parity — **TODO (post-0.3.1)**

Main difference between AC and AI is decimal handling. Audit and sync:

- Division behavior: AC uses integer division by default; AI uses float division. Confirm the intended canonical rule (note: AI decimals are represented as `map<exponent:coefficient>`, and integer division applies when both operands are ints).
- Coercions: `to_int` seems consistent; `to_dec` differs between AC and AI. Decide whether to unify behavior or document it as a deliberate delta.
- Shared IR opcodes and passes should produce identical output for non-decimal programs
- Test: run the same benchmark source through both AC and AI, compare checksums
- Document the delta in a PARITY.md

---

## 5. `test.ac` — Fix Empty Output — **TODO (post-0.3.1)**

`test.ac` compiles but produces no output. Diagnose and fix:

- Check if `<mainloop>` is properly recognized
- Check if the backend declaration is valid
- Run with `--stop-after-ir` to see what IR is generated
- If the file is intentionally empty/stub, add a minimal test case

```ac
    AC->PY
<mainloop>
    fn Term.display "hello"
<mainloop>
```

---

## 6. `bundle` Backend Lowering — **PARTIAL (0.3.1)**

Bundle (class-like construct) lowering is marked "ongoing." Complete it:

```ac
bundle Point
    x = 0
    y = 0
    Make dist func(self)
        return math.sqrt(self.x * self.x + self.y * self.y)
```

- Python: emit as a `class` with `__init__` and methods
- JS: emit as a `class`
- C: emit as a `struct` + standalone functions
- C++: emit as a `class`
- Java: emit as a `class`
- Rust: emit as a `struct` + `impl` block
- Go: emit as a `struct` + methods on pointer receiver
- BNY: struct layout in stack frame, method dispatch by label

Access control & lowering notes:
- `private` and `public` modifiers exist. Rule: if any modifier is used, all members must be explicitly annotated.
- Do not force a “struct-only” model: bundle lowering may choose struct vs class vs enum depending on features used (especially visibility / method dispatch / backend capability).

---

## 7. `<Foreign>` Block Fix — **DONE (0.3.1)**

`<Foreign>` blocks dump raw target-language text verbatim. BNY correctly rejects them (`Toxic: User attempts fluency in CPU`). For other backends:

- Verify `--allow-foreign` flag is required (already gated)
- Verify the dumped text appears in the right place in the output (inside `main`, not at file scope)
- Test: `<Foreign>` inside a function body vs at top level

---

## 8. `catch` — Bare `catch` Without `try` — **DONE (0.3.1)**

Currently the parser may allow stray `catch` keywords. Enforce that `catch` only appears after `try`.

---

## 9. `cond` / Switch-Style Dispatch — **DONE (0.3.1)**

```ac
cond x
    is 1
        Term.display $one$
    is 2
        Term.display $two$
    OTHER
        Term.display $other$
```

Compiles to a chain of `IF/ELSEIF/OTHER`. Value over bare `IF`:
- Single expression evaluated once (no repeated `x` in each branch).

---

## 10. Go `os` Import Cleanup — **DONE (0.3.1)**

Go backend always emits `import "os"` even when `os.Exit` is unused. Add a flag to track whether the `os` package is actually needed before emitting the import. Pre-existing issue, not regression.

---

## 11. `Term.print` / `Term.log` / `Term.write` Semantics — **DONE (0.3.1)**

Decision: remove them. AC keeps only `Term.display` (and `Term.ask` as input). `Term.print`/`Term.log`/`Term.write` should error.

---

## 12. `temp.attr` Scoping — **TODO (post-0.3.1)**

`temp.attr` is documented as "scopes a change to the current block only." Verify IR lowering emits a save/restore around the block boundary. Currently underspecified.

---

## 13. Operator `xsub` Edge Cases — **TODO (post-0.3.1)**

`xsub` is `abs(a - b) + 1` (inclusive distance). Verify:
- Negative inputs
- Float inputs
- BNY codegen uses `llabs` correctly
- Document in README that it is integer-only

---

## Notes

- v0.3.1 landed: items 1, 2, 7, 8, 9, 10, 11; plus partial progress on 3 and 6.
