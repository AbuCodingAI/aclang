# AC Scoping System — Implementation Plan

> Companion to `scoping.md`. That file is the **spec**; this is the **build plan**.
> Target: post-v0.2. Author of model: Abu. Plan dated 2026-06-20.

---

## Progress log (2026-06-20)

- **Phase 0 (done):** regression test `tests/scope_no_false_global.ac` — a function with no
  reference to a mainloop var emits no save/restore for it.
- **Phase 1 (done):** `free var = value` (Form 2). Parsed as a `FreeDecl` carrying the
  assignment as a child (`parser.cpp` ~1276; `ir.cpp` FreeDecl handler lowers children).
  Verified PY + BNY.
- **Phase 2a (done):** NA→free IR logic. Three fixes in `ir_codegen.cpp`:
  1. `inFunctionBody_` flag gates the mainloop free-var save/restore to the **global section**
     only — a loop *inside a function* no longer save/restores mainloop vars it can't see.
     (This was the true remaining cause of the `UnboundLocalError`/corruption, beyond the
     original line-6726 fix.)
  2. `globalVarNames_` repopulated from the clean depth-0 `freeVarSet` (not all-stores).
  3. pendingGlobals detection broadened from `STORE_VAR`-only to **any instruction whose
     result is a named VAR** (so `counter = counter + 1`, an ADD, is detected).
- **Phase 2b (DONE for PY/JS/C/Go; BNY pending):** per-backend global emission for NA→free.
  Scope chosen with Abu: BNY + PY/JS/C first.
  - **PY:** already worked (`global` decl).
  - **JS + Go:** fixed by extending the `promoted` set (`ir_codegen.cpp` ~6776) to include
    mainloop free vars (`globalVarNames_`) that a function *writes*, not just explicit
    `free`-declared vars. These backends already had `promotedGlobals_` infra
    (`var x;` file-scope + seed `declared`).
  - **C:** added `promotedGlobals_` to `CStrategy` (seed `declared` after each clear in
    function/main begin) + file-scope `ac_int v;` emission in the generate() hoist block
    (`promotedGlobalsList_`). Float free vars inferred via IR scan; default `ac_int`.
  - Verified: na_to_free → 11 on PY/JS/C/Go. no_false_global, free_assign green on all.
  - **BNY (pending, large):** emitter stores all VARs as RBP-relative stack slots keyed by
    symbol ID, so `counter` in a function ≠ `counter` in main. NA→free needs a **mutable global
    data region** (`.bss`/`.data` slot pool, parallel to the string `StringPool`), **two new
    RIP-relative encodings** (`mov_r_rip` / `mov_rip_r` with relocations, like `mov_ri64_str`),
    and load/store routing for free-var *names* to those slots. This is ELF/relocation-level
    work — a focused subproject, not a strategy tweak.

  Other compiled backends (C++/Rust/Java/V) still shadow; deferred per chosen scope.

---

## 1. Where things stand

The free/bound/NA model is **partially built**. Understanding what already exists is the
whole game here — the fragility came from a second, broken mechanism layered on top of a
working one.

### What works today

| Piece | Location | Notes |
|-------|----------|-------|
| `free x, y, z` declaration | `parser.cpp:1276` → `NodeType::FreeDecl` | one `FREE_DECL` IR op per name (`ir.cpp:2821`) |
| `<free>` block auto-declare | `ir.cpp:267` | emits `FREE_DECL` before first assign in a free block |
| Loop = bound sandbox | `ir_codegen.cpp:6088-6115` | `emitScopeEnter` before WHILE/FOR, `emitScopeExit` after |
| Save/restore codegen | `ir_codegen.cpp:1011-1024` (Python), mirrored per backend | `_ac_s{depth}_v = v` … `v = _ac_s{depth}_v` |
| `freeVarNames_` = mainloop depth-0 writes | `ir_codegen.cpp:6722-6744` | the set that loops save/restore |
| `free` exempts a var from revert | `ir_codegen.cpp:6746-6757` | declared-free vars persist past loops |

So the **global-scope** half of the spec (mainloop var, bound inside loop, `free` to persist)
is essentially implemented via save/restore.

### The bug that was fixed (2026-06-20)

`ir_codegen.cpp:~6726` additionally did:

```cpp
if (ins.opcode == STORE_VAR && ins.result.kind == VAR)
    globalVarNames_.insert(ref(ins.result));   // every mainloop var → "global"
```

`globalVarNames_` then drove `pendingGlobals` in `genFunction` (`:6504-6519`), making **every
function** emit save/restore for **every** mainloop var — including vars the function never
references and that are out of scope. Result: `sieve_count` save/restore injected into `sum`,
`loop_work`, etc. → undefined-name errors (Py/C++), corrupted labels and `140736012014688`
(BNY). The line is now commented out.

### What that fix cost us (a real gap, not a regression to ignore)

Emptying `globalVarNames_` means **spec rule 2 (NA → free)** no longer holds: a function that
writes a mainloop var no longer promotes that write to the caller's free scope. We traded a
crash for a missing feature. Reinstating it correctly is **Phase 2** — the difference is it
must be **reference-aware** (only vars the function actually reads/writes, and only ones that
exist in an enclosing free scope).

---

## 2. The gaps to close

1. **Form 2 — `free var = value`** (assignment): not parsed.
2. **Form 3 — `free f()`** (call override): not parsed, not modelled in IR.
3. **NA → free promotion**, reference-aware: reinstate rule 2 without the corruption.
4. **Context-sensitive call semantics**: same function, free when called at depth 0, bound when
   called inside a loop (spec "context-sensitive scope promotion").
5. **Replace save/restore string hack** with real scope frames (correctness + nesting + name
   collisions). Lower priority; the hack is adequate until 1-4 land.

---

## 3. Phased plan

### Phase 0 — Lock in the fix (DONE)

- [x] Comment out `globalVarNames_` mass-insert (`ir_codegen.cpp:~6726`).
- [x] Verify simple function-mutation + mainloop still compiles & runs (BNY/PY).
- [ ] **Add regression test** `tests/scope_no_false_global.ac`: two functions, one mainloop
      var used in only one of them; assert the other function's output has no `_ac_s*` save/restore.

### Phase 1 — Form 2: `free var = value`

Smallest, highest-value win. Pure additive parsing + reuse of existing `FREE_DECL`.

- **Parser** (`parser.cpp:1276`, the `KW_FREE` block): after consuming names, if the next token
  is `=`, parse it as: emit a `FreeDecl` for the single name **plus** a normal `Assign` node for
  `name = expr`. Keep the existing comma-list path when there's no `=`.
- **IR**: no new op — `FREE_DECL` + `STORE_VAR` already exist. Ensure the `FREE_DECL` precedes
  the store so the var is exempted from loop revert (the `:6746` exemption keys off `FREE_DECL`).
- **Tests**: `free total = total + i` inside a loop persists after the loop.

**Files:** `parser.cpp` only. ~15 lines. No codegen changes.

### Phase 2 — NA → free promotion (reference-aware reinstatement)

Restore spec rule 2 without the corruption.

- **`genFunction`** (`ir_codegen.cpp:6504-6519`): rebuild `pendingGlobals` from the intersection
  of (a) names this function actually **assigns** (scan `func.instructions` for `STORE_VAR`
  results — already done at `:6510`) and (b) a true free-scope set.
- Repopulate `globalVarNames_` **but** restrict it to mainloop depth-0 vars (the existing
  `freeVarSet` at `:6722-6744` is already exactly this — reuse it instead of the broken
  all-stores insert). Assign `globalVarNames_ = freeVarSet` once, after that scan.
- Guard: never add a name that is a parameter (`paramSet`, already checked at `:6514`) or a
  function-local temp (`_ac_`/`t_` prefixes).
- **Tests**: `mutate()` (writes `x`) called at mainloop depth 0 changes free `x`; a function
  that doesn't touch `x` emits **no** global decl for it.

**Files:** `ir_codegen.cpp`. ~10 lines, mostly reusing `freeVarSet`.

### Phase 3 — Form 3: `free f()` call override

The genuinely new semantic.

- **Lexer/Parser**: in the `KW_FREE` block, if after `free` the lookahead is `IDENTIFIER` then
  `(`, parse a call and wrap it: new `NodeType::FreeCall` (or reuse `FunctionCall` with an
  `attrs` flag `"__free__"`).
- **IR**: new flag on the `CALL` instruction (e.g. an `isFreeCall` bool, or a `FREE_DECL`-style
  marker emitted immediately before the call) telling codegen "do **not** wrap this call's
  effects in the loop sandbox."
- **Codegen** (`ir_codegen.cpp:6088-6115`): the loop save/restore wraps the whole body. For a
  free call, the callee's writes to free vars must survive. Simplest correct lowering: a
  `free f()` implies the vars `f` writes are in the loop's **exempt** set for this iteration —
  i.e. extend the per-loop exemption (Phase 2's `globalVarNames_`) to include the callee's
  written-globals at this call site.
- **Tests**: `mutate()` inside a loop reverts; `free mutate()` inside the same loop persists.

**Files:** `parser.cpp`, `ir.cpp` (CALL flag), `ir_codegen.cpp`. ~40-60 lines.

### Phase 4 — Context-sensitive promotion per call site

Spec: the *same* function is free at depth 0, bound inside a loop. Today the loop sandbox
already reverts everything on exit, so a plain `f()` inside a loop is effectively bound — this
mostly **falls out of Phases 2+3** for free. Work here is verification + the nested-loop
transition table (`scoping.md` "Transition table": inner bound → restored to outer bound).

- Audit `loopDepth_` handling (`ir_codegen.cpp:6090/6105`) for correct nesting of `_ac_s{depth}_`
  prefixes so inner loops restore to the **outer bound** value, not the free value.
- **Tests**: nested loops, mutation in inner loop, assert outer-loop value semantics.

**Files:** tests + possibly small `emitScopeEnter/Exit` depth fixes.

### Phase 5 — Replace save/restore with real scope frames (optional, later)

The `_ac_s{depth}_name` prefix is fragile: name collisions, deep nesting, and non-Python
backends each re-implement it. Long-term, model a scope as an explicit frame
(push/pop a dict/struct of bound shadows). Defer until 1-4 are proven; it's a refactor, not a
feature.

---

## 4. Per-backend checklist

Every phase that touches codegen must be verified on **all** backends, since each has its own
`emitScopeEnter/Exit` (Python `:1011`, JS, C/C++, Go, Rust, Java, V, BNY). BNY is the canary —
it crashed loudest on the original bug.

| Backend | Scope mechanism | Verify |
|---------|-----------------|--------|
| Python | `global` decl + `_ac_s*` save/restore | ✅ primary |
| JS / HTML | global promotion + save/restore | |
| C / C++ | block scope + save/restore | watch `auto` type of shadow |
| Go / Rust / V | block scope + save/restore | mutability keywords |
| BNY | stack slots + labels | **canary — run every phase** |

---

## 5. Test matrix (add to `tests/`, gitignored — keep local)

| Test | Asserts |
|------|---------|
| `scope_no_false_global.ac` | function without a var emits no save/restore for it (Phase 0) |
| `scope_free_assign.ac` | `free x = …` in loop persists (Phase 1) |
| `scope_na_to_free.ac` | `f()` at depth 0 mutates free var (Phase 2) |
| `scope_bound_revert.ac` | `f()` in loop reverts after (Phase 2/4) |
| `scope_free_call.ac` | `free f()` in loop persists (Phase 3) |
| `scope_nested_loops.ac` | inner bound restores to outer bound (Phase 4) |

Each test compiles to **all** backends and checks output equality against a known checksum.

---

## 6. Order of attack (recommended)

1. **Phase 0 regression test** — pin the fix so it can't silently come back.
2. **Phase 1** — `free var = value`. Cheap, isolated, immediately useful (kills `free`-then-assign tedium).
3. **Phase 2** — reinstate NA→free properly. Restores the feature the bugfix removed.
4. **Phase 3** — `free f()`. The new capability.
5. **Phase 4** — verify/repair nesting.
6. **Phase 5** — refactor save/restore only if it becomes a real pain.
