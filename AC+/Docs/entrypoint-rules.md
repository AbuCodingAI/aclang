# Entry Point Rules & AC ↔ AC+ Compatibility

---

## 1. Entry Point Rules

### 1.1 Programs that MUST have an entry point

Every **AC** (`.ac`) and **AI** (`.ai`) program that is meant to run must contain a `<mainloop>` block (AC) or equivalent entry block (AI). Without it, the compiler rejects the file:

```
Preposterous: EntryPointError — file.ac has no <mainloop> and is not a LIB
```

This mirrors C's requirement for a `main()` function. Code outside a `<mainloop>` with no LIB header is meaningless — there is no point from which execution starts.

### 1.2 Programs that are EXEMPT (library files)

A file is a library if it declares itself as one:

| Language | Library header  | Effect                              |
|----------|-----------------|-------------------------------------|
| AC       | `AC LIB`        | No entry point required             |
| AC       | `AC->LIB`       | No entry point required             |
| AI       | `AI LIB`        | No entry point required             |
| AC+      | `AC+ LIB`       | Cannot be compiled directly         |

**AC and AI** library files (`.ac`/`.ai` with `AC LIB` / `AC->LIB` / `AI LIB`) are compiled to a shared object (`.so`) or bytecode library — they have an output artifact.

**AC+ library files** (`.acp` with `AC+ LIB`) are *source-only* — they exist purely to be inlined via `use flib`. Running `acp logic.acp` directly is an error:
```
acp: logic.acp is an AC+ LIB — library files are not compiled directly.
     Import it with:  use flib logic.acp
```
The compiler resolves and inlines `use flib` imports before codegen; the library functions end up in the importing program's `.acx`.

### 1.3 AC+ rule

AC+ follows the same rule: an executable `.acp` file needs a `<mainloop>`. A library file (`AC+ LIB`) is exempt and can be a pure collection of `Make` functions.

---

## 2. AC Code in AC+

AC+ is a strict superset of AC's surface syntax. AC source code (`.ac`) that uses only shared constructs runs correctly in AC+ (`.acp`) without modification. This includes:

- `Make name func(params)` — function definitions
- `IF` / `ELSEIF` / `OTHER` — conditionals
- `WHILST` — while loop
- `FOR var in range N` — range loop
- `return expr` — return statements
- `/kill` `/stop` `/end` — halt / exit
- `$...$` strings and `r$...$` raw strings
- `use flib file.acp` — file library imports
- Arithmetic: `+` `-` `*` `/` `@` and compound forms `+=` `-=` etc.
- Comparisons: `is` `#=` `<` `>` `#>` `#<`

---

## 3. AC+ Code in AC — NOT Compatible

AC+ introduces constructs that target hardware registers and raw memory. These are **syntax errors in AC** and must never appear in `.ac` files intended for the standard AC compiler:

| AC+ construct         | Why it fails in AC                                      |
|-----------------------|---------------------------------------------------------|
| `reg 64box1 = ...`    | `reg` keyword unknown to AC; `64box1` is not valid AC identifier syntax |
| `stack varname byte`  | `stack` keyword unknown to AC                           |
| `64box1 << value`     | `<<` is not a defined operator in AC                    |
| `locate varname`      | `locate` keyword unknown to AC                          |
| `64box1 += offset`    | `64box1` cannot be parsed as an AC identifier (starts with digit) |
| `nil`                 | `nil` keyword unknown to standard AC                    |
| `AC+ LIB` header      | Not a valid AC backend declaration                      |

The register naming convention (`64box1`, `32vault`, etc.) is specifically AC+ syntax — identifiers starting with a digit followed by a name are rejected by the AC lexer as invalid tokens.

**Rule of thumb:** Write shared logic in AC syntax and import it via `use flib` into both AC and AC+ programs. Keep all hardware-facing code (registers, memory writes, `locate`, `nil`) in `.acp` files only.
