# AC 1 — Changelog (0.3.0 → 1.0.1)

**Release: AC 1** · npm `aclang@1.0.1` · 2026-07-12

This is the release where AC crossed from *"0.x, getting ready"* to a real first version.
Under Java-style product versioning it is **AC 1**; the npm artifact rides `aclang@1.0.1`
(`1.0.0` was an early prototype and is deprecated). The line went `0.3 → 0.4 → 1.0` (0.9 skipped).

The headline is **backend parity**: capabilities that used to work only on Python/JS now span
all ~13 targets — including the native x86-64 backend, which nearly doubled in size into a
self-contained machine-code runtime.

> Every entry below was verified by diffing the real 0.3.0 source (pulled from npm) against the
> 1.0.1 source — ~8,500 changed lines across the compiler, not git-log guesswork.

---

## 🚨 Breaking Changes — read before upgrading

- **64-bit only.** 32-bit targets/architectures are no longer supported.
- **Symbol operators retired — and they now *tell you so*.** `|`, `&`, and `#|` used to be
  operators (XOR, AND, XNOR); they are retired and throw a **migration error** pointing you at the
  word forms: `|` → "use `bxor`", `&` → "use `band` or `and`", `#|` → "use `not (a xor b)`".
  `|=` (xor-assign) is removed entirely. C-style `<<` / `>>` never were AC syntax — use `ptm` / `ptd`.
- **`&` and `|` changed meaning where still parsed.** In 0.3 `&` was logical AND and `|` was logical
  XOR. Logical ops are now word-only (`and` / `or` / `xor` / `not`); `&`/`|` map to bitwise `band`/`bxor`.
- **`%` is no longer modulo.** It's reserved as the (not-yet-implemented) variable-wildcard operator
  and is dropped by the lexer. Modulo is `mod` / `math.mod` only.
- **Mainloop variables are no longer globals.** Vars declared inside `<mainloop>` no longer leak to
  global scope — declare with `free` where you need persistence.
- **`/` is now "smart".** `7 / 2` → `3.5` (was `3`); `6 / 2` → `3` (integer when it divides evenly).
  Use `//` for always-integer, `///` for always-float.
- **Non-numeric arithmetic is now a compile error.** `$text$ / 2` (and `- * // /// @` on a string)
  raises `Preposterous: TypeError` instead of producing garbage.
- **Native (BNY) backend dropped Windows PE32+ *output*.** It emits Linux ELF64 only now. (This is
  separate from the cross-compiled `ac.exe`, which still ships — that's the *compiler* on Windows,
  not a target.)
- **`aczip` defaults to Zstd** (was gzip). New archives won't open with gzip-only readers.
- **Version scheme reset** to reach 1.0 — re-pin any `0.x` dependency to `aclang@1.0.1` / `@latest`.

---

## ✨ New Language Features

**Operators & math**
- **Division trio** — `/` smart (int when clean, else float), `//` integer (truncates toward zero),
  `///` always-float (a speed knob — skips the divisibility check). New IR opcodes `IDIV`/`FDIV`.
- **Short-circuit `and` / `or`** — `a and b` no longer evaluates `b` when `a` is false. This isn't
  just an optimization: it fixes out-of-bounds reads like `j > 0 and arr[j] > key` that used to touch
  `arr[-1]`.
- **Word logical operators** — `and` / `or` / `xor` / `not` as real keyword tokens (precedence
  OR < XOR < AND), replacing the old string-compare-on-identifier hack.
- **Word bitwise operators** — `band` / `bor` / `bxor` / `bnot` (and unary `~`). New opcodes.
- **Power-of-two shifts** — `x ptm 3` (× 2³), `x ptd 3` (÷ 2³) — literal bit-shifts; constant
  operands fold at compile time.
- **Exponent `^`** — `2 ^ 8`; right-associative (the only right-assoc operator in AC); `lit ^ lit`
  folds at compile time so it works even on the native backend.

**Types, conversion & decimals**
- **Conversion expression forms** — `to_string(x)`, `to_int(x)`, `to_dec(x)`, `to_bool(x)`
  (the statement forms `to_int x = …` also now produce correctly-typed values, not pointer garbage).
- **`const x = expr`** — immutable binding; reassigning a `const` is fatal.
- **`math.GoodDec price = 0.1 + 0.2`** — explicit exact-decimal declaration.
- **`math.LongInt`** reworked to **signed 96-bit** with new GoodDec arithmetic; **`math.isclose()`**
  for float comparison; **`math.eval()`** safe expression evaluator.

**Collections & builtins**
- **Dictionaries `{key: value}`** — the *literal* existed in 0.3, but AC 1 makes dicts actually
  **work on every backend** (get/set, variable keys, value-type inferred). `scores = {$ada$: 10}`.
- **`length x`** (no-paren) and **`length(x)`** — array/string length builtin.
- **`iota N`** and **`stream(a, b[, step])`** — as values they produce a concatenated string; as FOR
  collections they iterate lazily. `stream` gained the optional 3rd `step` arg.
- **List repetition** — `nums = [0] @ n` (or `[0] * n`) builds an n-element list.
- **Nested list literals** — `[[1,2],[3,4]]` parse fully (0.3 truncated at the first `]`).
- **Computed list elements** — `[x@2, x@3, 7]` evaluate to real values (were placeholder text).
- **Multi-value return** — `return x, y, z` (collected into a list).

**Control flow & structure**
- **`cond` multi-branch dispatch** — indentation block of `is <value>` / `other` arms.
- **`LineUp value { 1: …, 2: …, default: … }`** — brace-style switch (shares the `cond` engine).
- **`quickthread f(args)`** — run a call on a lightweight thread (Go goroutine; falls back to a
  synchronous call on backends without threads).
- **`bound` declarations** — `bound total = 0` — a loop accumulator scoped to the enclosing function
  (survives the loop sandbox) but never promoted to a true global — the middle ground between a plain
  local and `free`.
- **`cp x = y`** — explicit deep copy.
- **`compfold x = expr`** — force compile-time constant folding (with a `Toxic` warning if it can't).
- **`export`** (prefix or postfix) — mark which functions a file shares for multi-file AC projects.
- **`conglomer <file.h>`** — pull in a raw native C/C++ header **and dynamically link `lib<stem>.so`**.
  Distinct from `use` (AC libraries). Honored on C/C++/native; other backends reject it loudly.
- **`bundle` access modifiers** — `private field = …` / `public Make m func(…)` inside a bundle;
  and `bundle` classes are now actually lowered to every backend (0.3 shipped the syntax only).
- **`using math.sin`** — single-symbol import sugar (expands to a selective import).
- **try / catch / after** — multiple `catch` clauses, **typed catch** (`catch TypeError`), and a
  distinct `after` (finally) clause.
- **Foreign blocks** — raw target-language passthrough, gated behind `--allow-foreign` (and cheekily
  refused on the native backend — see *Diagnostics*).
- **Intentional infinite mainloop** — an unclosed `<mainloop>` compiles to `while(true)` under
  `--allow-infinite` (instead of silently closing).
- **`value` / `rule` as soft keywords** — usable as identifiers in `Make` / `alias` / `const` / `destroy`.
- **`destroy a, b, c`** — frees all listed names (0.3 freed only the first).

## 📚 New Libraries & Tooling

- **Web library** — HTTP/HTTPS, CORS, requests, SQL (via JaSQL), FFI across all backends.
- **`atar`** — AC's package manager, GitHub-based registry (installs `elib` packages).
- **`aczip`** — compression (XZ + Zstd), per-file parallel compression, FFI for Go/Rust/Java/V/ASM.
- **`ilib/ml`** — machine-learning scaffolding (PyTorch/TensorFlow-oriented) with FFI for all backends.
- **`ilib/pointers`** — pointer library (C++ core + FFI). *(Native backend: wired but not yet
  implemented — see Known Issues.)*
- **String helpers** — `stringm.ischar`, `stringm.isws`, `stringm.getline`, `stringm.scan`.
- **Runtime security** (new `runtime_security.cpp`) — anti-DDoS / anti-ping-flood / resource limits
  baked into compiled programs.
- **New flags** — `--allow-infinite`, `--allow-foreign`, `--static-link`, parallel `--all`, plus
  `DT_RUNPATH` so native binaries find their `.so` deps without `LD_LIBRARY_PATH`.

---

## 🎯 Backend Parity — the headline

Whole new shared layers in the codegen — **string inference**, **float inference**, and
**cross-block hoisting** — didn't exist in 0.3. They're threaded through every typed backend:

- **String inference** (`detectStringVars`): the compiler figures out which locals/params hold
  strings and declares them with each backend's native string type, iterating them as characters.
- **Float inference** (`detectFloatVars`): pre-inferred float locals come out `double`/`f64`/`float64`
  and mixed int/float ops cast correctly — fixing accumulators that used to truncate or fail to compile.
- **Cross-block hoisting** (`hoistVars_`): locals first written inside a loop/`if` are hoisted to the
  top for block-scoped targets (C/C++/V/Java/JS/Rust).

### What each backend gained since 0.3

- **C** — the biggest transformation (it was the weakest backend). In 0.3, **dictionaries were a dead
  comment** (`/* dict … */`) and there were no dynamic arrays. AC 1 adds a whole runtime: stretchy-buffer
  lists (`ac_arr_new`/`push`), a real `ac_dict` (with `KeyError`), string params (`const char*`),
  `ac_concat` for `+`, `strcmp` equality, guarded `ac_idiv`, and `conglomer` header linking.
- **C++** — end-to-end string inference (`.c_str()` at ilib boundaries, `std::string` params/returns),
  `std::vector<long long>` lists with `push_back` + a list printer, numeric-valued `std::map` dicts,
  `ac_fstr` whole-float formatting, `conglomer` (wrapped in `extern "C"`).
- **HTML** — **now a ~30-line subclass of the JS backend** (was a duplicated ~400-line backend).
  Inherits all of JS; browser math works only because JS gained a native `Math` shim (no FFI in browsers).
- **JavaScript** — native `Math` shim object (drops the `ffi-napi` dependency), array-aware printing
  (`_acp`, since Node column-wraps long arrays), throwing `ac_idiv` (so `try/catch` catches div-by-zero),
  `let` block-scope hoisting, parameter-declaration fix (reassigning a param no longer TDZ-crashes).
- **Java** — **lists are real `ArrayList<Long>`** now (were fixed-size `long[]`): appends propagate to
  callers, `.size()`/`.get()` auto-unbox. Native `AcMath` math shim (no FFI), `.equals` string
  comparison (was `==` reference-compare → silently wrong), numeric-valued `HashMap`, string returns,
  block-scope hoisting.
- **Rust** — native math shim (`math.mod` → `math.mod_f`, keyword clash), **list move-safety**
  (`.clone()` on `Vec`/`String` args + loop vars to satisfy the borrow checker), numeric-valued
  `HashMap<String, i64>`, string support (`as_bytes()` indexing, `.to_string()` iteration), f64 inference.
- **Go** — native `math` package shim (**no cgo/FFI**, sign-correct modulo), numeric-valued
  `map[string]int64`, string support, `float64` inference with mixed-op casting.
- **V** — numeric-valued `map[string]i64`, f64 inference (with V's name-lowercasing), string params
  and 1-char iteration, cross-block hoisting.
- **Python** — smart `/` via an `ac_div` helper (fully exact on the dynamic backend), string-aware `+`,
  typed catch.
- **LIB** — inherits all the C++ gains (string/list/dict/float/conglomer).

### Native (BNY) — from integer-only to self-contained machine code

The native x86-64 backend nearly doubled (2,619 → 4,461 lines). In 0.3 it emitted integer-only ELF
binaries that leaned on `libacmath.so` even to print a float. AC 1 emits **self-contained** binaries:

- **Floating point (SSE/XMM)** — `addsd`/`subsd`/`mulsd`/`divsd`, `ucomisd` comparisons with correct
  unsigned condition codes (0.3's raw-bit compare made `3.14 > 100` come out *true*), float args in
  XMM0–5, and `ac_print_double` **inlined in machine code** (was an external `.so` symbol).
- **Strings** — machine-code `__ac_concat__`, `__ac_streq__` (content compare, not pointer),
  `__ac_strlen__`, `__ac_itoa__` (int→string; `to_string(int)` used to pass the int as a fake pointer
  → segfault), 1-char indexing, and a fixpoint `preScanStrings` type pass.
- **Dictionaries & dynamic arrays** — `__ac_dict_get__`/`__ac_dict_set__`, heap list blocks with
  `arr.append`, and `Term.display arr` → `[e0, e1, …]` via `__ac_print_arr__`.
- **Real conditionals** — `IF`/`ELSE`/`END` markers were **ignored** in 0.3 (all branch bodies ran
  sequentially!); now they emit real conditional jumps.
- **Type casts, guarded division, native I/O** — `to_int`/`to_string` casts implemented; integer
  divide-by-zero raises a themed `ZeroDivisionError` instead of SIGFPE; `Term.ask` reads stdin via
  syscall; `/stop` → exit, `/halt n` → `nanosleep`, `raise` → stderr + exit.
- **Linking** — `DT_RUNPATH` bakes ilib `.so` dirs into the binary so it runs standalone; expanded
  per-namespace lib routing (os/regex/stringm/web/ml). DWARF debug info retained; producer string
  now `AC 1`.

---

## 🐛 Correctness & Bug Fixes

**Headline wins**
- **No more compiler hang** on `+=` / `-=` in expression position (e.g. `Term.display total += 1`).
- **`continue` no longer loops forever** inside `for` (it was skipping the counter increment).
- **Divide-by-zero is a clean error**, not a crash — C/native used to core-dump (SIGILL/SIGFPE); C++
  now actually catches it in `try/catch`; and it no longer silently folds to `0`.
- **The `IF`/`ELSE`-ignored bug on native** (branch bodies running unconditionally) is fixed.
- **All backends now agree on the correct number** — folded float constants were rounded (every text
  backend was wrong the *same* way) and C/C++ integer accumulators truncated `+= double`; a 7-function
  benchmark now returns the identical checksum on all 10 runnable backends.
- **Appending to a global list from inside a function** no longer segfaults.

**Parser / syntax** — nested lists, computed list elements, `value`/`rule` identifiers, misaligned
dedents (now an `IndentError`), hex/float tokenization, hyphenated keywords, no-trailing-newline files,
unary `-a+b` binding, `funcs[i](x)` call-statements, and `count < max` (no longer swallowed as a tag).

**Types & inference** — `to_int`/`to_string`/`to_dec` casts produce correct typed values; whole-function
float inference; `True`/`False` in int lists lower to `1`/`0`.

**Per-backend** — JS param-shadowing/TDZ, Java `.equals` compare, C/C++/V cross-block hoisting (fixes
`sieve`, `fib_list`, `array_append`), Go/V `sleep` mapping.

**Caching** — warm-cache compiles no longer lose cast result types (`resultType` now serialized; cache
version 16 → 17); instruction attribute markers round-trip.

**Native** — mixed int/float comparison, `cond` with loop bodies, try/catch/after, `/stop`/`/halt`/`raise`,
XMM args beyond the 2nd, list printing, and the append-to-promoted-global-free-var corruption.

---

## 🎭 Diagnostics — AC's error voice

AC has exactly two diagnostic severities, and both are roasts by design — the personality *is* the
severity system, not decoration on top of it:

- **`Preposterous:`** — the error severity (compilation stops). AC's `error:`.
- **`Toxic:`** — the warning severity (compilation continues). AC's `warning:`. There is no plain
  `Warning:` level; *every* warning AC emits is a `Toxic:` roast.

The catalogue:

| Message | Fires when |
|---|---|
| `Toxic: User used C, not effective` | a trailing `;` (C muscle memory) — once per file, still compiles |
| `Toxic: User attempts fluency in CPU` | a `foreign { … }` block on the native (BNY) backend — no target language, only CPU instructions |
| `Toxic: Compiler is slacking off — 'x' assigned but never read` | a dead variable / uncalled function |
| `Toxic: Compiler requested 3 business days to finish this math` | a `compfold` computation blows the 15,000,000-operation compile-time budget |
| `Toxic: What did WE do to you for this math?` | a `compfold` value can't be resolved at compile time (falls back to runtime) |
| `Preposterous: SyntaxError (It's all Greek to me) …` | any syntax error (the specific reason still follows) |
| `Preposterous: 3rd grade mathematics violated (ZeroDivisionError)` | integer divide-by-zero (C / C++ / JS / native) |
| `Preposterous: TypeError: '<op>' requires numeric operands (strings are not numbers)` | arithmetic on a string |
| `Preposterous: try block missing catch clause` | a `try` with no `catch` |
| `[os.sbash] The p in bash stands for protection` | `os.sbash` blocks an unsafe shell command (all FFI backends) |
| `Toxic: gcc choked on the generated C (exit N)` (and g++/rustc/javac/LIB variants) | the host toolchain rejected AC's emitted code |
| `Toxic: --output is ignored with --all …` | `--output` given alongside `--all` |
| `Preposterous: I don't speak <name>` | an unknown backend (`AC->Klingon`, `--target COBOL`) |
| `Preposterous: The system has unexpectedly ragequit, please try a smaller number` | *staged* — `math.MemInt` would exceed its 64 MB/value cap (catchable) |

*New in AC 1: the last five. `Toxic: Compiler is slacking off` and the first two roasts predate this release.*

---

## ⚡ Performance
- `aczip` rewritten for speed — per-file parallel compression; Zstd default (faster + smaller than gzip).
- Smart constant folding (`compfold`) evaluates pure expressions at compile time; extended to fold the
  division trio, bitwise ops, `ptm`/`ptd` shifts, and `^` powers.
- Program-wide dead-const-store elimination (also prevents Go/Java "unused variable" hard errors).
- `-O2` on the C backend.

## 📦 Versioning & Packaging
- **Java-style product versioning** — `ac --version` prints `AC 1 (aclang 1.0.1)`. "AC 1" is the
  release; the npm semver rides underneath. Future "AC 1" fixes are `1.1`, `1.2`, …; "AC 2" is next major.
- **`aclang@1.0.0` is deprecated** — it was an early prototype that did not compile programs.
- **Slim npm package** — ~3 MB download / ~10 MB unpacked. Ships only the prebuilt `ac` (Linux) and
  `ac.exe` (Windows) binaries, the ilib `library/`, the launcher, and README/LICENSE — not the compiler
  source tree, examples, tests, or IDE. Examples and docs live on GitHub.

---

## ⚠️ Known Issues (honest)

- **`math.mod` → `to_string`** misformats on C/C++/Go/Rust/V (a `double` FFI return prints as
  `1.000000`), so `type_coercion` / `dec_to_bin` differ from Python there.
- **`ilib/pointers` on the native backend is wired but unimplemented** — `__ac_ptr_new/deref/update`
  are *called* but have no machine-code definitions, so pointer programs won't link on BNY yet.
- **`%` wildcard operator** is unimplemented and silently dropped; `pong.ac` can't parse. (`;` is *not*
  dropped — it's roasted. `!` / `?` are dropped.)
- **Rust list parameters** are moved unless cloned — some `foo(list); use(list)` patterns still error.
- **Native float display** can show extra trailing digits (cosmetic); a few niche opcodes
  (ALIAS_DECL, EVENT_*, LAZY_EVAL, EVAL) aren't lowered on BNY.
- **`quickthread` runs synchronously** on backends without lightweight threads (e.g. native).
- **Per-example native parity** on the core suite isn't yet 1:1: roughly Go 19/22, Rust 17/22, C/V 16/22
  (remaining gaps: some string edge-cases, the `math.mod` cluster, Rust list-moves). PY/JS/C++/BNY are strongest.
- **Division exactness**: on typed backends `/` is friendly-display but a `double` under the hood — not
  full IEEE exactness. Dynamic backends (PY/JS) are fully exact.
- **`AC→ASM`** emits real x86-64 **NASM** — assemble it with `nasm -f elf64` (not GNU `as`, which
  speaks AT&T syntax). Output is produced on any host.
- **`AC→BNY` platform routing**: ELF on Linux x86 (done). The intended macOS/Windows-x86 → x86 ASM
  and ARM → C routes aren't wired yet — on ARM hosts it currently defers to `AC→C`.
- **Broader roadmap gaps**: lambdas not implemented; closures partial; concurrency present but unverified;
  no LSP; macOS untested; `ilib/ml` is scaffolding; no JSON/crypto/DB libraries yet; memory is
  unmanaged (no auto-free, no bounds/overflow checks).

---

*Built from a full 0.3.0 ↔ 1.0.1 source diff (~8,500 changed lines) plus the 2026-07 correctness audit,
every entry cross-verified against the current compiler source.*
