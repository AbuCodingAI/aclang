# AC v1.0 Roadmap

**Target Release:** 2-4 weeks  
**Current Version:** v0.9.0  
**Status:** Pre-release, final touches before production

---

## What's DONE ✅

### Core Language
- ✅ Lexer, Parser (Pratt), AST, IR, Codegen
- ✅ 11 working backends (Python, JS, C, C++, Rust, Go, Java, V, HTML, ASM, BNY)
- ✅ Smart constant folding (`compfold` keyword)
- ✅ Logical operators (and, or, xor, not)
- ✅ Bitwise operators (&, |, bor, ~)
- ✅ Functions, bundles, tag blocks, loops, conditionals
- ✅ Pratt parser with correct precedence

### I/O & System
- ✅ Term.display (multi-type output)
- ✅ Term.ask (string input)
- ✅ File I/O delegated to `ilib/os` (bash-based)
- ✅ Error handling framework (try/catch/after exists)

### Standard Library
- ✅ Math library (complete: trig, stats, constants)
- ✅ String operations (partial)
- ✅ Pointers framework (Phase 3 foundation)

### Compilation
- ✅ x86 ELF64 (Linux) — fully tested
- ✅ PE32+ (Windows) — tested via Wine on Linux
- ✅ x86 ASM generation
- ✅ AC→ASM for ARM (generates .s files)
- ✅ AC→LIB for shared libraries (.so/.dll)
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
    report $Error: $ + e
after
    cleanup()
```

**Status:** Implemented but needs:
- [ ] Test on all backends
- [ ] Verify error propagation
- [ ] Document syntax clearly
- [ ] Add 5+ test cases

**Estimate:** 3-5 days

---

### 2. **macOS Compatibility Verification** (1 week)

**What's needed:**
- [ ] Compile ac on native macOS (Intel or Apple Silicon)
- [ ] Build test suite (50+ programs)
- [ ] Run tests, fix breakage
- [ ] Document any platform-specific quirks
- [ ] Update README with confidence level

**Current state:** "Pray it works"  
**Target state:** "Tested and working" or "Known issues documented"

**Estimate:** 3-7 days (depending on breakage found)

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
- Loss of key developer (unrealistic)

---

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
