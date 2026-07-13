# AC v1.0 Roadmap

**Target Release:** 2-4 weeks  
**Current Version:** v0.4.0  
**Status:** Pre-release, working toward v1.0 production-ready

## Versioning policy (decided 2026-07-05)
Java-style INTEGER components — 0.x is the prototype line, and minor versions count as
integers: 0.9 → **0.10** → 0.11 → ... 1.0 is EARNED, not reached by decimal rollover.
After 1.0: standardized releases — every release gets a git tag, a CHANGELOG entry, and a
version bump in `ac --version`; no more untagged working-tree "releases".

---

## What's DONE ✅

### Core Language
- ✅ Lexer, Parser (Pratt), AST, IR, Codegen
- ✅ 12 working backends (Python, JS, C, C++, Rust, Go, Java, V, HTML, ASM, BNY, LIB)
- ✅ Smart constant folding (`compfold` keyword; `^`/ptm/ptd/comparisons fold to int consts)
- ✅ Logical operators (and, or, xor, not/#) — folds are int 0/1 on every backend
- ✅ Bitwise operators as WORDS: band, bor, bxor, bnot/~ (symbols | #| & |= RETIRED with
     migration-hint errors)
- ✅ `^` = exponentiation on ALL backends (ac_ipow builtin + const folding; BNY machine code)
- ✅ ptm/ptd = LITERAL bit shifts (<< / >> on text backends; shl/sar on BNY)
- ✅ Term.display is a first-class PRINT opcode (not a LIB_CALL)
- ✅ Scoping: loop sandbox (default) / `bound` (function-persist) / `free` (global) — all 3 live
- ✅ iota n (lazy range), stream(a,b[,step]) (lazy sequence), sequence step arg on all backends
- ✅ `quickthread f(args)` — goroutine on Go, plain call elsewhere
- ✅ Functions, bundles, tag blocks, loops, conditionals
- ✅ Pratt parser with correct precedence

### I/O & System
- ✅ Term.display (multi-type output)
- ✅ Term.ask (string input)
- ✅ File I/O delegated to `ilib/os` (bash-based)
- ✅ Error handling framework (try/catch/after exists)

### Standard Library
- ✅ Math library (complete: trig, stats, constants)
- ✅ string-cheese (upper/lower/strip/replace/find/split/b/endian/...)
- ✅ os, regex, web — real implementations
- ✅ **BNY can now call os./regex./stringm./web. namespaces natively** (2026-07-05:
     generic ExtSym mapping ac_<ns>_<fn>, per-lib .so routing, char*-return marking)
- ✅ Pointers framework (Phase 3 foundation)

### Compilation
- ✅ x86 ELF64 (Linux) — fully tested
- ✅ PE32+ (Windows) — tested via Wine on Linux
- ✅ x86 ASM generation
- ✅ AC→ASM for ARM (generates .s files)
- ✅ AC→LIB for shared libraries (.so/.dll) — FIXED 2026-07-05: was emitting goto-style
     C++ (jump-crosses-initialization); now rides high-level IR: 110/110 examples pass
- ✅ Cache system (IR cache)

### Platforms
- ✅ Linux x86-64 (fully tested)
- ✅ Windows x86-64 (tested via Wine)
- 🟡 macOS x86-64 (code generated, untested)
- 🟡 macOS ARM64 (code generated, untested)
- 🟡 Linux ARM64 (infrastructure, via AC→C)

---

## What's NEEDED for v1.0 ✅ → 🟢

### 1. **Refinement: try/catch/after** (1 week)

```ac
// Currently works, needs:
try
    risky_operation()
catch Error as e
    raise ERR(ErrorMSG+$risky_operation doesn't work$)
after
    cleanup()
```

**Status (2026-07-05 verification):**
- [x] Div-by-zero no longer constant-folds to 0 (it silently erased the runtime error —
      try/catch around `x // 0` never fired on ANY backend; fixed in applyBinOp + optimizer)
- [x] PY: catches ✓   [x] Java: catches ✓
- [ ] JS: `a // 0` → Infinity (JS division never throws — decide: emit explicit throw-on-zero?)
- [ ] Go: panic occurs but catch body didn't print — verify recover() wiring
- [ ] Rust: catch_unwind wiring present, needs the same verification
- [ ] Document syntax clearly; add 5+ test cases (try_catch.ac / try_report.ac exist)

**Estimate:** 3-5 days

---

### 2. **macOS Compatibility Verification** (1 week)

**What's needed:**
- [ ] Compile ac on native macOS (Intel or Apple Silicon) Not Apple Silicon, we can test C programs on x86
- [ ] Build test suite (50+ programs) 
- [ ] Run tests, fix breakage
- [ ] Document any platform-specific quirks
- [ ] Update README with confidence level

**Current state:** "Pray it works"  
**Target state:** "Tested and working" or "Known issues documented"

**Estimate:** 3-7 days (depending on breakage found) /* I also need an Intel Mac, actually, I have one */

---

### 3. **Concurrency Verification** (1 week)

**Current state:** spawn/wait/channels framework exists but untested

**What's needed:**
- [ ] Test `spawn worker(1, 2, 3)` on all backends
- [ ] Test `wait` synchronization
- [ ] Test channels (if implemented)
- [ ] Verify no deadlocks
- [ ] Add concurrency test suite

**Example:**
```ac
spawn worker(1)
spawn worker(2)
spawn worker(3)
wait
```

**Estimate:** 3-5 days

---

### 4. **Documentation & Examples** (1 week)

- [ ] Update README (done)
- [ ] Add 20+ example programs in `AC/examples/`
- [ ] Write quick-start guide
- [ ] Document ilib/os file I/O
- [ ] Document error handling patterns
- [ ] Changelog for v1.0

**Estimate:** 3-5 days

---

### 5. **Test Suite** (optional, but recommended)

- [x] 110 examples in AC/examples/ (pristine .ac only) — ALL compile, all non-interactive
      ones RUN on their header target
- [x] AC/tests/run_examples_all.sh → ALL_BACKENDS_MATRIX.md (110 × 12 compile matrix)
- [ ] 100+ test programs covering:
  - Operators, functions, bundles
  - File I/O (via os library)
  - Error handling
  - Concurrency
  - All 11 backends
  - Math library

**Estimate:** 2-3 days (if automated)

---

## Roadmap Timeline

```
Week 1: Refinement
├─ try/catch/after testing & fixing (3-5 days)
├─ macOS compatibility check (3-7 days)
└─ Concurrency verification (3-5 days)

Week 2: Documentation & Release
├─ Examples & guides (3-5 days)
├─ Test suite (optional, 2-3 days)
├─ Release notes & changelog (1 day)
└─ v1.0 release (1 day)

Total: 2-4 weeks depending on bugs found
```

---

## Blockers & Risks

### 🟢 **Low Risk (unlikely to delay v1.0)**
- Cosmetic bugs in error messages
- Minor documentation gaps
- Performance improvements

### 🟡 **Medium Risk (might delay by 3-5 days)**
- macOS compilation issues (likely fixable quickly)
- Concurrency deadlocks (probably framework issues)
- File I/O edge cases (os library issues)

### 🔴 **High Risk (would delay v1.0)**
- Major parser regression
- Cross-platform compatibility catastrophe
- Loss of key developer (impossible, I am the only key dev)

---

## NEW for v1.0 — found & remaining (2026-07-05 audit/fix session)

### Fixed this session (see memory/ac_operator_overhaul_fixes.md for detail)
- BNY: print-helper stack clobber (loop-carried r12), IDIV missing, ac_length/ac_ipow helpers,
  float-const display (raw-bits bug), arrays in mainloop, call-arg lowering (5 paths),
  $-delimiters in method-arg strings
- LIB backend: was generating goto-C++ → 110/110 now
- Rust: math.mod → mod_f keyword collision + f64/i64 comparison coercion
- IR cache: instruction attrs now serialized (bound/copy markers survived-loss bug); IRC v15/ACC v9
- Loop save/restore of not-yet-defined vars (NameError)
- sequence/stream step dropped on high-level backends
- Foreign emission: \n-escaped one-liners → raw passthrough. NOTE: <Foreign> bodies are
  TARGET-SPECIFIC source — cross-backend Foreign failures under --all are EXPECTED, not bugs.

### Still open, ordered (v1.0 blockers first)
0. **Array-parameter type inference** (BIGGEST toolchain gap): functions taking arrays are
   emitted as `long long arr` / `arr int64` / `mut arr: i64` on C++/LIB/Go/Rust/Java — the
   generated code fails the target compiler whenever a function receives an array. Needs
   call-site type propagation (arg is ALLOC list → param is vector<long long>&/[]int64/Vec<i64>).
   This accounts for ~38 of LIB's remaining toolchain failures (array examples with functions).
   NOTE: the ALL_BACKENDS_MATRIX measures GENERATION; add a toolchain-level pass (grep
   "compilation failed") for true per-backend health. LIB toolchain-level: 72/110 after the
   high-level-IR + ac_length fixes (was ~44 generation-level before).
1. **Call-on-indexed-expression** `funcs[i](x)` — parser drops the call; blocks perfect-ish.ac.
   Needs postfix-call parsing + indirect-call codegen (emitIndirectCall exists on text backends;
   BNY needs call-through-register).
2. **BNY Term.ask** input broken (read syscall returns 0) — most concrete user-facing BNY defect.
3. **BNY list display** — `Term.display arr` prints the block pointer; needs __ac_print_list__
   (same helper family as BNY iota value-display, also missing).
4. **Parser catch-order dead handler** (parser.cpp ~650: catch(runtime_error) before
   catch(ACError)) — ACError handler is dead code; compiler itself warns.
5. **web/aczip/keybinds on BNY + compiled backends**: web mostly wired; needs web.acl + web_c.h
   scaffolding (clone machine-audio's) for the C-family; ml lib needs a real implementation.
6. Foreign-in-function double-wraps (top-level Foreign is canonical; fix the wrapper).
7. regex.replace on BNY/C++ core replaces first occurrence only — decide all-vs-first semantics.
8. `length x < 10` parses as `length (x < 10)` — greedy prefix; document or add precedence.

## Post-v1.0 (v1.1+)

These are **NOT blocking v1.0:**

- ARM ASM backend (post-v1.0, needs ARM hardware)
- Lambdas (nice-to-have, complex)
- Proper closures (nice-to-have)
- Bootstrapping (nice-to-have)
- LSP/IDE support (nice-to-have)
- LineUp/match statements (nice-to-have)

---

## Definition of v1.0 Done

### ✅ Must Have
- [ ] try/catch/after working
- [ ] Concurrency verified
- [ ] macOS status known (working or documented limitations)
- [ ] 100+ programs test in examples/
- [ ] README reflects reality
- [ ] No known show-stoppers

### 🟡 Should Have
- [ ] Cross-platform test suite
- [ ] Performance benchmarks
- [ ] API documentation complete

### 🟢 Nice to Have
- [ ] Bootstrapping
- [ ] LSP support
- [ ] ARM ASM backend

---

## Commit Strategy for v1.0

```bash
# Week 1: Feature fixes
git commit -m "Fix try/catch exception propagation"
git commit -m "Verify concurrency on all platforms"
git commit -m "Test macOS compatibility"

# Week 2: Documentation
git commit -m "Add v1.0 examples & guides"
git commit -m "Update README for v1.0"
git commit -m "Release notes: v1.0"

# Release
git tag v1.0
git push origin main --tags
```

---

## Success Criteria for v1.0

✅ **AC v1.0 is production-ready when:**

1. Users can compile AC code to working binaries on Linux, Windows, macOS
2. File I/O works (via os library)
3. Error handling works (try/catch)
4. Concurrency works (spawn/wait)
5. Math library complete
6. Documentation is clear
7. No critical bugs reported
8. Test suite passes on all platforms

**We're very close.** 2-4 weeks tops.
