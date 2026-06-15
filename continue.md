# Continue — Pending Work

Work to pick up in the next session.

> **AC+ is not needed until AI V 0.4 and AC V 0.5.** Do not prioritize AC+ work before then.

---

## AC+ Compiler — Test & Verify

The compiler pipeline is fully written but untested end-to-end:

1. **Build the compiler**
   ```bash
   cd AC+/acp-compiler && make
   ```

2. **Compile the AOS kernel directly to .acx**
   ```bash
   # logic.acp is an AC+ LIB — it is NOT compiled directly.
   # kernel.acp imports it via "use flib logic.acp"; the compiler inlines it.
   ./acp AC+/AOS/kernel.acp  # produces kernel.acx (entry=0x0)
   ```
   To inspect intermediate assembly: `./acp AC+/AOS/kernel.acp --emit-asm`
   Trying to compile a library directly gives:
   ```
   acp: logic.acp is an AC+ LIB — library files are not compiled directly.
        Import it with:  use flib logic.acp
   ```

3. **Build bootable AOS image (skeleton.s + kernel)**
   ```bash
   as --32 -o skeleton.o AC+/AOS/skeleton.s
   # Extract the flat binary from the .acx payload (skip 13-byte header):
   dd if=kernel.acx of=kernel.bin bs=1 skip=13
   ld -m elf_i386 -Ttext 0x7C00 --oformat binary -o AOS.img skeleton.o kernel.bin
   qemu-system-x86_64 -drive format=raw,file=AOS.img
   ```

4. **Inspect an ACX file**
   ```bash
   g++ -std=c++17 -o acx_constructor AC+/acx_constructor.cpp
   ./acx_constructor --info kernel.acx
   ```

---

## Known Codegen Limitations to Fix

- **Nested binary expression depth > 1 is unsupported** — codegen uses only `%r10`/`%r11` as scratch registers; evaluating `(a + b) * (c + d)` would give wrong results. Needs a simple stack-based spill for nested ops. Fine for current AOS use case.

- **Memory write size is always `movb`** — `<<` always emits byte-wide writes. If AC+ gains non-byte memory writes (e.g., word or dword), add a size annotation to the MemWrite AST node and derive `movb`/`movw`/`movl`/`movq` from it.

- **Division (`/=`, `/`)** — currently emits a comment placeholder. Needs `idivq` with proper `%rdx:%rax` setup.

- **16-bit and 8-bit register aliases** — only 64-bit and 32-bit registers are in REG_MAP. Add `16box1` (`%di`), `8box1` (`%dil`) etc. if needed for real-mode or byte-granular register work.

---

## AC+ Language Features Not Yet Implemented

- **`FOR` loop with non-range iterables** — currently only `FOR var in range N` is handled; iterating over an array/pointer would need a new AST node.
- **Nested function calls as arguments** — `boot_print(getValue(), 0)` works only if `getValue()` returns in `%rax`; multi-arg nested calls may clash with arg register setup.
- **String literals in AC+** — `$...$` strings are parsed but codegen emits nothing for StringLit nodes. Need `.rodata` section + label + `leaq` to load address.
- **Global / static variables** — no `.data` section support yet; all variables are stack-local.

---

## atar — AC Package Manager (AC 1.0)

**atar** = Spanish "to tie/bind". Installs `.tar` package bundles to the language.

- Install path: `<ac-compiler>/library/elib/<package-name>/`
- Compiler already uses binary-relative lookup for ilib (`readFFIFile` in `ir_codegen.cpp`); elib should follow the same pattern under `library/elib/`
- Usage (planned): `atar install <package>` — no `ac -m` needed
- Like PyPI but for AC

When implementing elib resolution in `ir_codegen.cpp`, mirror the existing ilib `tryBase` logic but use `base + "/library/elib/" + libName` as the path.

---

## AC / AI — Nice-to-Have Next

- **AC REPL `:compile` command** — let the REPL compile to a target other than run, so you can inspect generated code.
- **AI `FOR` loop** — AI currently lacks `FOR var in range N`; it has `WHILST` only.
- **AC bundle inheritance / method dispatch** — `bundle B extends A` style; the class model is partially designed but not wired up.
- **VSCode AC+ syntax extension** — add `.acp` file support to the existing `vscode-ac` extension: `reg`, `stack`, `locate`, `<<`, register names highlighted.

---

## ACX Constructor

`AC+/acx_constructor.cpp` is a standalone C++ file (not part of the acp compiler). Build it separately:
```bash
g++ -std=c++17 -O2 -o acx_constructor AC+/acx_constructor.cpp
```
