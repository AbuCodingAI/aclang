# aca-compiler (AC*)

Minimal AC* compiler frontend for `.aca` files.

Current focus:
- Typing system: `Numeral` (`NumInt`, `NumDec`), `String`, `Boolean` (`true`/`false`, case-insensitive)
- Exact decimal literals: stored as `map<exponent, coefficient>` (base-10 exponent → digit coefficient)
- Strings: `$...$` and raw `r$...$`

Build:
```bash
make -C "AC*/aca-compiler"
```

Run:
```bash
"AC*/aca-compiler/bin/aca" "AC*/benchmark.aca"
```

