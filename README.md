# AC Language — v0.2.0

A multi-target compiled language with a Pratt parser, unified IR codegen, and a full math library. Write once, compile to 11 backends.

```bash
npm install -g aclang
```

---

## Backends

| Declaration | Output | Target |
|-------------|--------|--------|
| `AC->BNY` | `.acb` native executable | x86-64 ELF/PE |
| `AC->ASM` | `.s` | x86-64 assembly |
| `AC->C` | `.c` | C source |
| `AC->C++` | `.cpp` | C++ source |
| `AC->PY` | `.py` | Python |
| `AC->JS` | `.js` | JavaScript |
| `AC->RS` | `.rs` | Rust |
| `AC->GO` | `.go` | Go |
| `AC->Java` | `.java` | Java |
| `AC->HTML` | `.html` | Interactive HTML |
| `AC->V` | `.v` | V |

---

## Quick Start

```bash
# Install globally
npm install -g aclang

# Compile and run
ac mycode.ac

# Compile to specific backend
ac mycode.ac PY
ac mycode.ac BNY

# Compile only (no run)
ac mycode.ac -c
```

---

## Syntax

### Comments
```ac
/* This is a block comment */
```

### Backend declaration
Must be the first line:
```ac
AC->PY
```

### Variables
```ac
name   = $Alice$
age    = 30
active = true
data   = null
```

### Operators

| Operator | Meaning |
|----------|---------|
| `is` / `==` | Equal |
| `#=` | Not equal |
| `#>` | Not greater (≤) |
| `#<` | Not less (≥) |
| `*` | Numeric multiply |
| `@` | Universal multiply — numbers or `$hi$ @ 3` → `$hihihi$` |
| `+=` `-=` `*=` `@=` `/=` | Compound assignment |

### Conditionals
```ac
IF score > 90
    Term.display $A$
ELSEIF score > 75
    Term.display $B$
OTHER
    Term.display $C$
```

### Loops
```ac
/* Whilst loop */
WHILST i #> 10
    Term.display i
    i += 1

/* For loop */
FOR item in list
    Term.display item
```

### Functions
```ac
Make fib func(n)
    IF n #> 1
        return n
    OTHER
        return fib(n - 1) + fib(n - 2)
```

### Bundles (classes)
```ac
bundle Point
    x = 0
    y = 0
    Make dist func(self)
        return math.sqrt(self.x * self.x + self.y * self.y)
```

### Tag blocks
```ac
<mainloop>
    Term.display $running$
    /kill
<mainloop>
```

### Foreign blocks
Embed raw target-language code:
```ac
<Foreign>
    import numpy as np
    print(np.array([1, 2, 3]).mean())
<Foreign>
```

---

## Math Library

```ac
AC->PY
use ilib math

<mainloop>
    Term.display math.pi          /* 3.141592653589793 */
    Term.display math.pi(50)      /* pi to 50 decimal places */
    Term.display math.phi         /* 1.618033988749895 (golden ratio) */
    Term.display math.e
    Term.display math.tau

    Term.display math.sin(math.pi / 2)
    Term.display math.sqrt(2)
    Term.display math.pow(2, 10)
<mainloop>
```

**Constants:** `math.pi`, `math.e`, `math.phi`, `math.tau`, `math.em`, `math.inf`

**Precision:** `math.pi(n)`, `math.e(n)`, `math.phi(n)` — return the constant to n decimal places

**Trig:** `sin` `cos` `tan` `asin` `acos` `atan` `atan2` `csc` `sec` `cot` `deg2rad` `rad2deg`

**Arithmetic:** `sqrt` `cbrt` `pow` `abs` `floor` `ceil` `round` `ln` `log` `log2` `log10` `hypot` `mod` `gcd` `lcm` `clamp` `is_prime`

**Aggregates:** `math.sigma(list)` `math.PI(list)` `math.gradient(list)`

**Statistics:** `stat_avg` `stat_median` `stat_q1` `stat_q3` `stat_mode` `stat_min` `stat_max` `stat_boxnum`

The math library ships as a prebuilt `libacmath.so` with FFI bindings for Python, JavaScript, Go, Rust, Java, V, and ASM — no install step needed.

---

## Architecture

```
AC Source (.ac)
    ↓
Lexer
    ↓
Pratt Parser → AST
    ↓
Unified IR (SSA-style typed references)
    ↓
IR Cache (hash-based, lives in ac-cache/)
    ↓
Backend Codegen × 11
    ↓
Target output
```

The unified IR means all 11 backends share the same optimization passes — constant folding, copy propagation, dead code elimination — and produce consistent output.

---

## Caching

```bash
ac mycode.ac            # writes ac-cache/mycode.acc on first run
ac mycode.ac            # reads from cache, skips parse
ac mycode.ac --force    # ignore cache, recompile
ac mycode.ac --no-cache # skip cache entirely
```

---

## Installation from source

```bash
git clone https://github.com/AbuCodingAI/aclang.git
cd aclang/AC/ac-compiler
make
sudo make install
```

Requires `g++` and `make`. The Windows `ac.exe` is pre-built in `ac-compiler/ac.exe` (static, no dependencies).

---

## What changed in v0.2.0

- **Pratt parser** — replaced recursive descent; correct operator precedence throughout
- **Unified IR codegen** — single IR layer for all 11 backends replacing per-backend AST walking
- **Math library** — shipped for the first time; full trig, statistics, calculus, complex, FFI for 6 languages
- **`math.phi`** — golden ratio added alongside `math.pi`, `math.e`, `math.tau`
- **`@` operator** — universal multiply (string repetition + numeric)
- **`/*...*/` comments** — replacing old `*...*` syntax
- **Bundle construct** — class-like data + methods
- **`ac.exe`** — pre-built Windows binary included
- **Zero dependencies** — removed the `vercel` and self-referential `aclang` deps from v0.1.6

---

## License

MIT — see [LICENSE](LICENSE).

---

**AC Language — Write once, compile anywhere.**
