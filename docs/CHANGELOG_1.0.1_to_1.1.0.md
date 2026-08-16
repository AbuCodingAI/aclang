# AC 1.1 — Changelog (1.0.1 → 1.1.0)

**From:** npm `aclang@1.0.1` · **To:** `aclang@1.1.0` · 2026-07-20

`aclang@1.0.1` was a **rushed** first cut of AC 1 — real, but shipped fast. **1.1.0 is the substance
release**: two new wired-in integer types, an optimization model that gives you TCC-fast *or*
GCC-fast from one flag, genuinely standalone native binaries, and a deep pass over the type system,
the standard library, and compile-time safety.

Theme: **"the same program gives the same answer on every backend, can't be made to crash or run a
shell on you, and — on the native backend — links to nothing."** Mostly things that were subtly
wrong now being right, plus a few real new capabilities.

> Every entry is verified by re-running the affected backends (`--all` sweeps + BNY-vs-Python diffs),
> not git-log guesswork. Where something is only partly done, it's called out honestly.

---

## ✨ New Language Features

- **`short` — 32-bit signed integer** (`short a = 5`) and **`mini` — 16-bit signed integer**
  (`mini m = 3`), plus the cast forms `short(x)`/`mini(x)`. Narrower and faster than the 64-bit `int`
  default, wired into the core type system through a single width→backend-type mapping in the type
  header (`include/type.hpp`) — C/C++ `int32_t`/`int16_t`, Rust `i32`/`i16`, Go `int32`/`int16`, Java
  `int`/`short`. Python/JS/native-BNY (and V, which lacks the cast-type plumbing) have no fixed-width
  int, so the width is advisory there. An overflowing *literal* (`short w = 5000000000`) is a clean
  compile error on **every** backend. **Mixed-width arithmetic promotes to the bigger type** — AC's
  rule: `mini + int` → `to_int(mini) + int`, `short + mini` → both promoted to `int` — so `short a +
  mini m` compiles and runs identically on all 9 backends (a literal, e.g. the `4` in `a * 4`, adapts
  to the variable's width instead of forcing a promotion).

- **`-O4` — the runtime-speed tier.** AC's `-O` levels now split cleanly: **`-O0`–`-O3` optimize for
  *compile speed* (TCC-style — every level stays fast to build), and `-O4` optimizes for *runtime
  speed* (GCC-style — pays compile time for a fast binary). The expensive whole-program constant
  folding (evaluating e.g. `fib(35)` at compile time) now lives only at `-O4`; the low levels do only
  cheap, safe passes. `compfold x = …` still force-folds a single expression at any level, and
  `--runtime` still disables folding — all three compose. **One compiler, TCC and GCC in one flag.**
  (Before, all four `-O` levels were identical — `optLevel` was parsed and thrown away.)

---

## 🧱 Native (BNY) — standalone binaries via `--static-link`

- **`--static-link` now produces a real, zero-dependency binary** — no gcc, no ld, **no external
  processes at all**. BNY reads a freestanding ilib object in-process, splices its machine code
  straight into the binary, and binds each ilib call to it. Verified on integer-math programs
  (`math.gcd`/`abs_int`/`is_prime`/`mod_int`/`lcm`): `ldd` reports *"not a dynamic executable."*
  - A used ilib function with no freestanding implementation yet → **graceful fallback** to a working
    *dynamic* binary (was: dropped the runpath and produced a broken one).
  - Zero-import programs are fully static as before.
  - *Rollout:* the splicer + reader are done; the remaining work is freestanding rewrites of the
    float/transcendental and string ilibs (each just needs `-ffreestanding`, then its `.o` drops into
    the same path) plus a relocation applier for non-self-contained objects.

- **BNY zero-dep static for zero-import programs** (`ldd` = "not a dynamic executable") — was
  force-linking libc on every binary.

---

## 🐛 Type system — the "mangled types" pass

- **One authority for assumed call types.** `math.mod` (and every ilib function's return type) is now
  classified in ONE place (`acCallReturnsFloat` in `type.hpp`), consulted by the IR, the text
  backends, AND the native backend — instead of each re-deriving it and disagreeing. This fixes a
  class of BNY bugs where a float value's raw bits were treated as an integer:
  - **`total += math.mod(m, 10)`** in a loop → correct sum (was garbage like `4886407794720243712` —
    a double's bit pattern summed as an int). The float pre-scan now recognizes ilib float returns and
    a `STORE_VAR`-of-a-float propagates correctly.
  - **Float parameters to user functions** — `nsqrt(2.0)` now computes `1.414…` (was garbage): a param
    a caller passes a float to is loaded as a `double`, not sign-converted from its bits.
  - **`armstrong` and similar math-loop programs** are now deterministic on BNY (were flaky).
- **Whole-valued floats print `.0`** on BNY (`12.0`, not `12`) — matching Python.

## 🐛 Correctness — front-end, IR, backends

- **Word operators respect the *right* boundary** — `a*orbit` no longer matches the `or` operator
  inside `orbit`.
- **String↔string constant folding is real** — `"cat" is "dog"` used to fold to *always-equal*.
- **Computed dict keys keep their value** — `ages[first + last]` no longer has the list-index `−1`
  wrongly applied to a computed string key.
- **`math.mod` / `//` are floor-consistent** across PY/C/C++/Go/V/Java and the folder.
- **Negative-step streams iterate** (`stream(10, 0, -1)` ran zero times before).
- **Hoisting fixes:** a recursive function whose name collided with a hoisted local no longer breaks
  C/C++ (`ackermann`); a loop accumulator built inside `<mainloop>` no longer comes back empty on the
  text backends (`string_repeat` — now matches BNY).
- **C++ string functions compile** (empty `return` → `std::string()`/`0.0`, not `return 0`; a
  `$hi $ + name` return is detected as string-returning); **C++ mixed string concat** (`"hi " + n`,
  `"a" + "b"`, `to_string`-of-a-double) all route through an overloaded `ac_cat()`.
- **C `iota`/`stream` can't segfault** (negative-size malloc guarded); **C `eval()` links** (was an
  undefined symbol); **HTML no longer double-declares** `let x`.
- **Python `int + str`** (`count + name`) routes through a runtime `_ac_add` (was a `TypeError`).
- **C++ dict read of a missing key** throws via `.at()` (was a silent default-insert).
- **`to_int` from a string literal** now parses on Rust and V (`"42".parse()` / `'42'.i64()`).
- **exp_bny fatal fixes:** cross-function label collision (function A could jump into B) — labels now
  carry a per-function prefix; SysV float-argument ABI corrected for mixed int/float ilib calls;
  `math.mod 0` no longer `SIGFPE`s (zero-divisor guard).

## 🔒 Security & safety

- **Compile-time command injection closed** — the whole toolchain (gcc/g++/rustc/…, library loading,
  `atar`) now uses **argv-array exec** (fork+`execvp` / `_spawnvp`, no shell), so a malicious `.ac`
  (`use flib "/x/$(cmd).so"`) can't run arbitrary commands at compile time. Cross-platform, not just
  POSIX shell-escaping. The dead, never-linked "runtime security" module was removed.
- **HTML output is escaped** — `Term.display.bold/link/…` used to concatenate values straight into
  `innerHTML`/`<a href>` (XSS). All styled output now goes through an `_esc()` HTML-escaper.
- **Compiler UB / hang guards** — the `^` power-fold no longer spins on a huge constant exponent and
  wraps in unsigned (no signed-overflow UB); `ptm`/`ptd` shift-fold checks `0≤b<64`; `ADD/SUB/MUL`
  const-folds wrap in unsigned; the remaining `lexer.cpp` ctype calls cast to `(unsigned char)`.

## 📚 Standard library

- **`native-cpu` (pointers)** memory-safe: size-tracked allocations, bounded `pt_deref`, growing
  `pt_update`, deep `pt_copy`, value-comparing `pt_eq`. *(The full native-cpu library — `dha`/`z`/
  `bare-metal` arena, `quickthread`, broadcast/receive, the `atomic` type — is a 1.1.x feature in
  progress; the memory-safety core landed here.)*
- **`aczip`** rewritten around one zlib codec (preserves filenames, zip-slip-safe, race-free,
  div-by-zero guarded). **`math`** hardened (shift-by-64 UB, `lcm(0,…)`, C-ABI exception guards).
  **`string-cheese`** results no longer alias (round-robin buffers — two `stringm.*` calls in one
  expression stay distinct). **`ml.softmax`** guards an empty input. Numerous FFI-binding
  compile/load fixes across all backends.

---

## ⚠️ Known Issues (honest)

- **BNY float printing** matches Python's value but not its exact `repr` — `3.14159` shows as
  `3.141589999999999` and the last digit of long decimals can differ (shortest-round-trip / Ryu is
  not implemented in the hand-written machine-code printer). Whole-valued `.0` is fixed.
- **`--static-link`** currently bundles only the integer-math ilib (the only one rewritten
  freestanding so far); other ilibs fall back to dynamic linking. See the rollout note above.
- **The text ASM backend** still has gaps (bitwise/loops/print-var/frame) — slated for replacement by
  the exp_bny-driven NASM emitter rather than patched.

---

*AC 1.1 = `aclang@1.1.0`. Re-pin `@latest`. Previous line: [0.3.0 → 1.0.1](CHANGELOG_0.3_to_1.0.md).*
