# AC Compiler (`ac-compiler`)

AC (AbuCompiled) is a high-level programming language focused on simplicity, fast compilation, and efficient execution. It allows developers to write readable code that can be compiled to multiple backends while remaining approachable for beginners.

This directory contains the **AC language compiler** implementation (C++17), plus a small Python REPL and tests.

If you’re looking to learn the language (syntax/features/stdlib), start with `AC/README.md`.

It is intentionally “single-binary”: the `ac` executable lexes and parses AC source, lowers it to a unified IR, optionally caches intermediate artifacts, then emits one of the supported backends.

## Quickstart

Build the compiler:

```bash
make -C AC/ac-compiler
```

Hello world:

```ac
AC->PY

<mainloop>
    Term.display $Hello, AC$
<mainloop>
```

Run it:

```bash
AC/ac-compiler/ac hello.ac
```

## Build

From the repo root:

```bash
make -C AC/ac-compiler
```

Artifacts:

- `AC/ac-compiler/ac` (Linux/macOS)
- `AC/ac-compiler/ac.exe` (Windows build target; see below)

Windows cross-build (requires `x86_64-w64-mingw32-g++`):

```bash
make -C AC/ac-compiler windows
```

See `AC/ac-compiler/Makefile`.

## Run

```bash
AC/ac-compiler/ac path/to/file.ac
AC/ac-compiler/ac path/to/file.ac --target PY
AC/ac-compiler/ac path/to/file.ac --no-run
AC/ac-compiler/ac path/to/file.ac --stop-after-ir
```

The full CLI help text is generated in `AC/ac-compiler/src/main.cpp` (including `--allow-foreign`, optimization flags, BNY debug stops, cache flags, etc.).

## Learn AC

- Language guide + feature reference: `AC/README.md`
- Extra docs (design notes, parity notes, roadmaps): `AC/docs/`

## Directory layout

- `AC/ac-compiler/include/`
  - Public compiler headers: tokens/AST/IR/types/tags/backends/errors.
- `AC/ac-compiler/src/`
  - Core compiler implementation:
    - `lexer.cpp`: tokenization + indentation handling, keyword set, `<Foreign>` capture.
    - `parser.cpp`: Pratt parser + statement parsing + tag parsing.
    - `ir.cpp`: AST → `AC_IR::IRProgram` lowering, plus IR passes.
    - `ir_codegen.cpp`: unified IR code generator for all backends.
    - `backend_registry.cpp`: backend name → extension + runner command mapping.
    - `exp_bny.cpp`: BNY backend (native x86-64 binary emitter / debug pipeline).
    - `acc_cache.hpp`: `.acc` AST cache format.
    - `ir_cache.hpp`: `.irc` binary IR cache format.
    - `lib_lower.hpp`: `.acl`-driven library lowering pass for `LIB_CALL`s (e.g. `gl:*`).
- `AC/ac-compiler/test/`
  - Pytest suite and small C++ unit tests for parser/IR internals.
- `AC/ac-compiler/relics/`
  - Historical per-backend codegens (not used by the unified IR codegen).
- `AC/ac-compiler/ac_repl.py`
  - Tiny REPL helper (Python).

Note: `ac-cache/`, `*.o`, `*.d`, `__pycache__/`, `.pytest_cache/`, `.hypothesis/` are build/test artifacts.

## Compilation pipeline (high level)

The compiler’s “front door” is `AC/ac-compiler/src/main.cpp`:

1. Read source file.
2. Determine backend:
   - `AC->XX` header (preferred).
   - `AC LIB` sentinel for source-only libraries (not directly compilable).
   - `--target/--backend` CLI override.
3. Cache lookup (unless `--no-cache`):
   - `.acc` AST cache via `AC/ac-compiler/src/acc_cache.hpp`.
   - `.irc` IR cache via `AC/ac-compiler/src/ir_cache.hpp`.
4. Lex → parse → AST.
5. **flib injection** (important):
   - `use flib some_module.ac` / `.ai` gets parsed and inlined into the parent AST
     (functions, bundles, and nested imports), so downstream lowering sees a single AST.
6. Lower AST → unified IR (`AC_IR::generateIR` in `AC/ac-compiler/src/ir.cpp`).
7. Library lowering:
   - `.acl` specs (e.g. `library/ilib/gl/gl.acl`) rewrite names like `gl:obj.config`
     into canonical `ac_gl_*` calls via `AC/ac-compiler/src/lib_lower.hpp`.
8. Codegen from unified IR:
   - `generateFromIR(...)` in `AC/ac-compiler/src/ir_codegen.cpp`.
9. Optionally run the generated artifact unless `--no-run`.

## Caches (`.acc`, `.irc`, `.lir`)

### `.acc` (AST cache)

- Purpose: skip lexing + parsing for unchanged sources.
- Validation: timestamp + `ACC_MAGIC`/`ACC_VERSION`.
- Format details: `AC/ac-compiler/src/acc_cache.hpp`.

### `.irc` (binary IR cache)

- Purpose: skip lowering + optimization + (partially) library scanning for unchanged sources.
- Validation: magic + version + **hash**.
- Hash includes: source + backend + a compiler “cache marker” string.
- Format details: `AC/ac-compiler/src/ir_cache.hpp`.

### `.lir` (human-readable IR)

Enabled by default; useful when debugging `--stop-after-ir` output.

## Library dependency tracking (FFI + ACL mtimes)

The IR cache hash is extended with a suffix of:

- Any `library/ilib/<lib>/ffi/<lib>_ffi.<backend-ext>` mtime
- Any `library/ilib/<lib>/<lib>.acl` mtime

This is computed by scanning the source for `ilib <name>` and `header <name>` forms, in
`ffiMtimesSuffix(...)` in `AC/ac-compiler/src/main.cpp`.

## `<Foreign>` blocks

`<Foreign>` is gated behind `--allow-foreign` in `AC/ac-compiler/src/main.cpp` and checked again during IR lowering (`AC/ac-compiler/src/ir.cpp`).

BNY rejects `<Foreign>` entirely (“Toxic: User attempts fluency in CPU”).

Lexing/capture is in `AC/ac-compiler/src/lexer.cpp` (raw text is stored as an internal tag payload).

## Backends

Backends are registered in `AC/ac-compiler/src/backend_registry.cpp`.

Important design note: the registry’s “generator” functions are dummy placeholders; the real work is done by the unified IR code generator (`AC/ac-compiler/src/ir_codegen.cpp`). The registry primarily provides:

- Backend name
- Output extension
- “runner” command string used when `ac` is allowed to execute outputs

## Error format

Compiler errors are normalized into the “Preposterous:” style via `AC/ac-compiler/include/error.hpp`.

## Tests

Python tests (pytest):

```bash
cd AC/ac-compiler
pytest -q
```

C++ test helpers exist in `AC/ac-compiler/test/` as small standalone binaries/sources.
