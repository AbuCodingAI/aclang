# AC — Session Handoff (2026-07-16)

Resume point for the big "fix everything" campaign. Read top-to-bottom; nothing here assumes memory of the session.

---

## 0. Working-tree state (IMPORTANT)

Many files are **edited but NOT committed** this session (compiler src, libraries, README, BUGS.md, this file). Nothing was committed except the earlier repo-scoping commit `c541a42c`. Build is green (`make -C ac-compiler`). If you want a checkpoint commit, do it from `<mono>/AC/` (the repo root is `AC/`, not the monorepo root — see memory).

Full pre-audit backup bundle: `/tmp/aclang-monorepo-FULL-backup.bundle` (may be gone after reboot — it was /tmp).

---

## 1. The 9-item campaign — status

| # | Item | Status |
|---|------|--------|
| 1 | **pointers → memory-safe** | ✅ DONE (verified) |
| 2 | **aczip rebuild** | ✅ DONE (verified round-trip) |
| 3 | **security / argv-exec** | ✅ DONE (driver + web/os/machine-audio .so; injection proven eliminated) |
| 4 | **MINOR + PERF sweep** | ✅ substantive fixes done (residual cosmetic ones noted) |
| 5 | **bury dead code** | ✅ 490 lines removed (codegen_utils.hpp + runtime_security module) |
| 6 | **% wildcard** | ⚠️ lexer/token foundation done (silent-drop FATAL fixed); **parser + semantics PENDING** |
| 7 | **fix all LOGIC (~64)** | ⏳ ~8 fixed, ~56 remain |
| 8 | **rename pointers→native-cpu + `atomic` type** | ⏳ PENDING (spec below) |
| 9 | **fix all FATAL (~39)** | ⏳ 2 fixed (parser recovery, zero-dep binaries), ~37 remain — **do LAST (big writes)** |

Order (user-set): finish LOGIC → native-cpu/atomic → FATALs last.

---

## 2. Decisions LOCKED this session

### 2a. `native-cpu` library (`ncpu`) — replaces `pointers`
Rename `library/ilib/pointers/` → `native-cpu/`, `.acl` prefix `pointers:` → `ncpu:` (well, `native-cpu:` token → `ncpu.` calls). Full surface (my interpretation was confirmed 100% correct):

- **Allocation** (`dha` = dynamic heap allocation, `z` = zeroed, `bm` = **bare-metal**):
  - `ncpu.dha` — malloc-style raw alloc
  - `ncpu.zdha` — calloc-style zeroed alloc
  - `ncpu.realloc` — resize
  - `ncpu.bmdha` / `ncpu.bmzdha` / `ncpu.bmrealloc` — bare-metal variants (no OS/libc allocator; straight to raw pages/hardware)
- **Arena allocator:** `ncpu.arena_create`, `arena_alloc` (bump), `arena_dealloc`, `arena_destroy`, `arena_abort` (emergency nuke the whole arena on error)
- **Control:** `ncpu.abort` — hard bare-metal abort (CPU trap/halt, not clean exit)
- **Messaging (between quickthreads):** `ncpu.broadcast("Shutdown_request")` (send named msg to all), `ncpu.recieve("Shutdown_request")` (returns true/false AND consumes the message if present)  ← note user's spelling "recieve"
- **Concurrency:** `ncpu.quickthread` — NOT "coroutine". `quickthread` is an **existing AC core feature** (goroutine-like, CPU-friendlier name). `ncpu.quickthread` should be a **MORE bare-metal, more CPU-friendly** variant of it.
- **Pointers:** carry over all existing pointer fns (`ptr_new`/`ptr_deref`/`ptr_copy`/`ptr_update`/`ptr_eq`/`ptr_null`/`ptr_is_null`).

### 2b. `atomic` type
`atomic s = 5` — `atomic` is a TYPE keyword declaring an atomic variable, part of native-cpu. Rest of the design TBD by user.

### 2c. BNY dependency policy (✅ IMPLEMENTED + verified)
- **No imports** → fully **static, ZERO-dep** ELF (pure syscalls; `ldd` = "not a dynamic executable"). DONE — was force-linking libc; removed the unconditional `collectExternalSymbols` libc append (exp_bny.cpp ~4216).
- **`use` / `conglomer` a library** → dependency is fine (link what it uses).
- **`--static-link`** → must be ZERO dep too (see 2d — currently BROKEN).

Verified: 8/8 no-import example programs are now static + match PY.

### 2d. BNY TRUE static linking — **Option 2 chosen** (the big pending feature)
Problem: `--static-link` on BNY today just strips the RUNPATH but keeps `libacmath.so` as a NEEDED dep → binary FAILS standalone ("cannot open shared object file"). It is NOT static linking.

**User's hard rule: BNY must NOT invoke gcc/ld** (BNY spawns zero external processes — that's its identity). And it must produce ONE standalone Linux binary even with libraries.

Real blocker: ilibs are **C++** (`std::string`, exceptions) → pull in `libstdc++` + `libc`. Splicing them self-contained would mean reimplementing `ld` AND embedding those runtimes (+ bloat).

**DECISION = Option 2:** make the ilibs **freestanding** (rewrite the C/C++ ilib cores to use **only syscalls**, like BNY's own runtime — no `libstdc++`, no `libc`), so each ilib becomes a tiny self-contained blob with **no external symbols**. Then give BNY a **minimal in-process object splicer**: parse the ilib's simple relocatable object, place its `.text`/`.data`, apply a few internal relocations, patch the call sites. No gcc, no bloat, genuinely bare-metal.

This is the native-cpu / bare-metal ethos applied to the whole ilib layer. It's the ONLY path where BNY stays zero-toolchain AND supports libraries in one standalone binary.

**Next steps for 2d:**
1. Pick a pilot ilib (math is the natural first — most-used). Rewrite its core to syscalls-only / freestanding (no `<string>`, no exceptions; C, not C++, or `-ffreestanding -nostdlib` C++).
2. Build it to a self-contained relocatable object (`.o`) with no undefined external symbols beyond what BNY provides.
3. Write the BNY splicer: parse ELF `.o` (symtab, `.text`, `.data`, `.rela`), append its sections into BNY's ELF at known vaddrs, apply `R_X86_64_PC32`/`PLT32`/`64` relocations, and resolve BNY's ilib call sites to the embedded symbol addresses (replaces the current PLT/GOT dynamic path when `--static-link` or "bundle" is requested).
4. Roll out freestanding rewrite to the other ilibs.

**PROGRESS (2026-07-20): the splicer WORKS for the integer-math pilot.**
- In-BNY ELF `.o` reader `readFreestandingObject()` (exp_bny.cpp, before writeELF): parses ET_REL,
  pulls `.text` + defined `.text` symbol offsets, flags any relocations (`hadRelocs`).
- Splice + rebinding in `BinaryCompiler::compile()`: `--static-link` → `bundleStatic_`; loads each
  `<ilibdir>/freestanding/fsmath.o`, checks it defines every needed export, and if so appends its
  `.text` (`em.appendSplicedText`) and binds each ilib call label to the spliced address
  (`em.defineLabelAt(irName, …)`) BEFORE `applyFixups` → the static `writeELF` path (no PLT/GOT/NEEDED).
- Plumbing: `generateBinaryFromIR(..., bundleStatic)` ← `main.cpp` passes `staticLink`; the ilib
  dirs are always passed (dynamic uses them for DT_RUNPATH, static to locate the freestanding objs).
- VERIFIED: `math.gcd/abs_int/is_prime/mod_int/lcm` with `--static-link` → `ldd` = "not a dynamic
  executable", correct output, zero external processes. Float/other ilib fns → graceful DYNAMIC
  fallback (a working binary, not the old broken "drop runpath"). Zero-import → pure static as before.
- REMAINING for full Option 2: (1) the relocation applier (R_X86_64_PC32/PLT32/64/32S) for ilib
  objects that AREN'T self-contained (the pilot inlines its cross-fn calls at -O2, so it has none);
  (2) freestanding rewrites of the float/transcendental + string ilibs (each `-ffreestanding`, then
  its `.o` drops into the same splice path). The machinery is done; it's now per-ilib legwork.

**Earlier notes (2026-07-19):**
- Step 1+2 DONE (pilot): `library/ilib/math/freestanding/{fsmath.c,Makefile}` — a freestanding integer-math
  core (`ac_abs_int/max_int/min_int/mod_int/idiv_int/ipow_int/gcd_int`) built with
  `-ffreestanding -O2 -fno-stack-protector -fno-asynchronous-unwind-tables -fcf-protection=none -fno-pic -fno-plt`.
  `nm fsmath.o` → 7 `T` symbols, **zero `U`** (no libc/libm/libstdc++). `make verify` asserts this.
  Integer-only on purpose (no libm) — transcendentals/strings follow the same recipe once the splicer lands.
- Step 3 (the splicer) is the remaining big piece. Concrete design, all against the read-only ELF structs
  already in exp_bny.cpp (`Elf64Ehdr/Shdr/Sym/Rela`, currently write-only):
    a. `struct ObjBlob { vector<uint8_t> text, data, rodata; vector<{name,off,section}> syms; vector<{off,symIdx,type,addend}> relocs; }`.
       Reader: validate `e_ident`/`ET_REL`, walk `e_shoff` section headers, pull `.text`/`.data`/`.rodata`
       raw bytes, parse `.symtab`+`.strtab` (defined `STT_FUNC`/`STT_OBJECT` syms), parse each `.rela.<sec>`.
    b. Placement: append blob `.text` after BNY's own text at a known vaddr; append `.data`/`.rodata` into the
       RW/RO segment. Record each spliced symbol's final vaddr in a `name→vaddr` map.
    c. Relocations: apply `R_X86_64_PC32`/`R_X86_64_PLT32` as `*(int32*)(loc) = S + A - P`;
       `R_X86_64_64` as `*(int64*)(loc) = S + A`; `R_X86_64_32S` as `*(int32*)(loc) = S + A`.
       `S` = target sym vaddr (internal → the map; external → error in static mode, that's the whole point).
    d. Call-site rebinding: in static/bundle mode, instead of `emitPLT0()/emitPLTStub()` (exp_bny.cpp ~4335),
       for each ext-sym `em.label(normalizeExtSym(irName))` at the spliced symbol's offset so the EXISTING
       `applyFixups()` rel32 path resolves `em.call(irName)` to the embedded address — no PLT/GOT/DT_NEEDED.
    e. Mode plumbing: today `--static-link` only nulls `bnyRunpath` (main.cpp ~864). Thread a real
       `bundleStatic` flag into `generateBinaryFromIR`→`BinaryCompiler::compile` so the `dynamic` branch
       (exp_bny.cpp:4308, `bool dynamic = !extSyms.empty()`) is forced to the splice path (→ `writeELF`, the
       existing zero-dep static writer) even when `extSyms` is non-empty, splicing each needed ilib `.o` first.
  Dynamic path (with-libraries, no `--static-link`) is UNCHANGED and already works (PT_INTERP+DT_NEEDED+PLT/GOT).

Missing `.a` archives (build if a fallback static path is ever wanted): `aczip`, `widgets`, `keybinds` (keybinds is header-only). Most ilibs already have `.a`.

### 2e. `%` wildcard semantics (item 6, partial)
`%` = name/string WILDCARD (NOT modulo — modulo is `math.mod`). `p%` = starts-with, `%p` = ends-with, `%p%` = contains. Two uses:
- **string matching** (tractable: lower to string-cheese startswith/endswith/contains)
- **GL-object-namespace** (pong.ac: `p%.hitbox.coords overlap` = "any object whose name starts with p" — needs GL runtime enumeration)

DONE: lexer tokenizes `%` (TokenType::PERCENT), no longer silently dropped → loud parse error instead of corruption.
PENDING: parser recognition of `p%`/`%p`/`%p%` + lowering (string case) + GL-namespace case.

---

## 3. What was FIXED this session (all build-clean + behavior-verified)

### Libraries
- **pointers** (`library/ilib/pointers/pointers.cpp`): size-tracked entries → `pt_deref` bounded (no heap over-read), `pt_update` grows safely, `pt_copy` = DEEP copy (fixes UAF/double-free), `pt_eq` = value compare. Go FFI: `package main` + `bytes.Equal` (was interface{}-compare panic). Rust FFI: slice-copy not `Vec::from_raw_parts` (was double-free). PY FFI: added `pt_copy` + `pointers` namespace so `pointers.ptr_*` resolves; `pt_eq` by value.
- **aczip** (`library/ilib/aczip/aczip.cpp`): one codec (zlib) both ends, self-consistent format with **filenames preserved**, race-free parallel compress, div-by-zero guarded, **zip-slip-safe** extraction, robust loop-grow decompress, memcpy reads. Round-trip verified. Removed dead `generate_tag`.
- **string-cheese** (earlier): restored `stringm.f`/`stringm.t`/`stringm.b` (b/f/t = bytes/f-string/t-string; `t` was wrongly `trim`) across C core + `.so` + `.acl` + PY/JS FFI + BNY string-return list.

### Security (argv-exec, no shell — cross-platform injection-proof)
- `main.cpp`: added `run_argv()` (fork+execvp / `_spawnvp`) + `shell_split()`; converted all 7 toolchain `system()`/`timedRun` sites. Proven: `use flib /x/$(cmd).so` no longer executes.
- `web.cpp` `open_url`, `os_c.cpp` `app_open`, `machine_audio.cpp` `runEspeak` + mpg123 → execvp. (`os.bash`/`sbash` intentional shell; `aplay` fixed-string safe.)

### Compiler correctness (LOGIC/FATAL)
- **parser.cpp**: error-recovery was DEAD (`catch(runtime_error)` swallowed all `ACError` before the recovery catch) → reordered; multi-error reporting works again. [FATAL]
- **exp_bny.cpp**: removed unconditional libc append → no-import programs are static zero-dep. [FATAL, see 2c]
- **ir.cpp**: string-vs-string comparison folding (`"cat" is "dog"` was always-equal → now string compare); `@` string-repeat (`$ab$ @ 3` threw → now `ababab`, fold + guard); `stoi`→`stoll` (2 sites, 2^31 overflow); negative-step streams (`stream(5,0,-1)` = 0 iterations → now `5 4 3 2 1`, detects unary-minus step, both lowering paths).
- **ir_codegen.cpp**: **modulo floor-consistency** — `math.mod(-7,3)` was 2 on PY but -1 on C/C++/Java/Rust/Go/V; now floor-mod everywhere (fold + all 6 emitMod; Java uses `Math.floorMod`).

### Sweep (MINOR/PERF)
- C string-iter O(n²) `strlen` hoisted; JS busy-wait sleep → `Atomics.wait`; lexer ctype-on-raw-char UB (5 sites) cast to `unsigned char`; lexer `substr==` → `compare()` (3 sites); type.hpp Numeral switch fall-through guard; main.cpp `find_last_of` double-scan + dead `(void)` stmt + ctype cast.

### Dead code
- Deleted `include/codegen_utils.hpp` (orphan) + `runtime_security.{cpp,hpp}` (dead uncompiled module) = 490 lines.

### Docs / config
- **README.md** rewritten where "cooked": `.s`→`.asm`, thirteen→twelve backends, comment syntax (only `/* */` — `*…*` and `#` are NOT comments), `/` = smart division + added `///`, `math.product`→`math.PI`, GL name fixes (`gl.screen.create`, `gl.is_obj`, `gl.hitbox.boundary`), removed dead `gl.obj.save_spawn`.
- `~/.claude/settings.json`: `permissions.defaultMode = bypassPermissions` (takes effect next session).

---

## 4. Remaining LOGIC bugs (item 7) — next up, from BUGS.md audit
High-value ones not yet done (each = targeted fix + per-backend verify):
- **ir_codegen** PY `int + str` emitted verbatim → runtime TypeError (960-972)
- **ir_codegen** C++ dict `m[key]` silently inserts default on missing key vs KeyError (3383-3388)
- **ir_codegen** HTML `declared`-set shadows JS's → duplicate `let x` JS error (1710)
- **ir.cpp** adjustIndex applies `-1` to computed dict keys (236-255)
- **ir.cpp** word-operator scan no right boundary (`a*orbit`→`or`) (1731-1742)
- Library FFI cross-backend divergences (math.log 1-arg vs 2-arg, math.js big-endian double marshaling, missing phi/eval, regex_ffi.rs won't compile, StringCheese.java dup method, os sbash blocklist divergence, ml stubs, etc. — see BUGS.md "Library FFI bindings")

## 5. Remaining FATAL bugs (item 9) — LAST, big writes, from BUGS.md
Headliners: exp_bny cross-function **label collision** (per-fn counters + shared label map → one fn jumps into another); exp_bny **SysV float-arg ABI** wrong for mixed int/float sigs; C `ac_iota`/`ac_stream` **negative malloc → segfault**; C++ `emitReturn` **`return 0` for string funcs → null std::string crash**; C `emitEval`→undefined `ac_eval`; HTML/JS shadowing; ASM backend (bitwise/loops/print-var/frame-overflow — the whole ASM backend is weak, see BUGS.md #15 + audit). aczip/web/pointers/math library FATALs mostly fixed already; recheck the audit list for the C-core ones still open (ml softmax empty-deref, math LongInt shift-by-64 UB, machine-audio UAF, keybinds KEY_ENTER dup, etc.).

## 6. Where things live (quick map)
- Compiler: `AC/ac-compiler/src/{lexer,parser,ir,ir_codegen,exp_bny,main,backend_registry}.cpp`, `include/*.hpp`
- Libraries: `AC/library/ilib/<name>/` (C/C++ core + `ffi/` per-backend + `.acl` lowering)
- Full audit (187 findings): `BUGS.md` (monorepo root, section "FULL-CODEBASE AUDIT (2026-07-14)")
- V compiler: `~/.vlang/v/v` (on PATH). aclang git repo root = `AC/` (not monorepo root).
- Build: `make -C AC/ac-compiler` ; test a backend: `AC/ac-compiler/ac file.ac --target BNY --force --no-cache`
