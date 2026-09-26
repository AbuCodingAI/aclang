# AC Language

AC (AbuCompiled) is a high-level, indentation-based, multi-target compiled language. Write once, compile to any of twelve backends (plus an experimental AArch64 backend). The compiler lexes AC source into a Pratt-parsed AST, lowers it to a unified IR, runs optimization passes, and emits the selected target language.

Current version: **AC 1** — see `ac --version`. Licensed under the [GNU GPL v3](#license).

---

## Install

The compiler runs on Linux, macOS and Windows. Its native backends (BNY, ASM, ARM) emit Linux ELF binaries,
so those run on Linux only; every text backend (PY, JS, C, C++, Java, Rust, Go, V) works everywhere its
toolchain is installed. The installer puts `ac` on your `PATH` on Linux and macOS, and `ac.exe` on Windows
(run it from Git Bash, MSYS2 or Cygwin).

```bash
git clone https://github.com/AbuCodingAI/aclang.git
cd aclang
./install.sh
```

`install.sh` installs the packages the compiler needs (via `pacman`, `apt`, `dnf`, `zypper`, or Homebrew on
macOS), builds `ac` from source, builds the ilib shared libraries (Linux only for now), and installs
everything under `/usr/local` with `ac` on your `PATH`. On Windows it installs the prebuilt `ac.exe` under
`~/AC` (add `~/AC/bin` to your Windows `PATH`). It only uses `sudo` where the destination needs it.

| Option | Effect |
|--------|--------|
| `--user` | Install to `~/.local` — no root needed for the install itself |
| `--prefix DIR` | Install somewhere else |
| `--minimal` | Only what is needed to build the compiler (a C++17 toolchain and `make`) |
| `--all` | Also the toolchains for every backend (JDK, Rust, Go) and the optional ilib dependencies |
| `--dev` | Also the cross toolchains used to rebuild `ac.exe` and `ac.arm` |
| `--no-deps` | Do not touch system packages |
| `--prebuilt` | Use the shipped binary instead of compiling: the universal `ac-compiler/ac.com` if present, else `ac` (Linux x86-64), `ac.arm` (Linux AArch64) or `ac.exe` (Windows). Always on for Windows |
| `--dry-run` | Print what would happen, change nothing |
| `--uninstall` | Remove a previous install |

Default packages: a C++17 toolchain and `make`, `python3`, `nodejs`, `nasm`, `zlib` and GTK 3 (for
the `aczip` and `widgets` ilibs). The V backend needs the V compiler, which most distros do not
package — see <https://vlang.io>. `BNY` (AC's own native x86-64 backend) needs no external toolchain.

Building by hand:

```bash
make -C ac-compiler            # ./ac-compiler/ac
make -C ac-compiler arm        # ./ac-compiler/ac.arm   (needs an aarch64 cross g++)
make -C ac-compiler windows    # ./ac-compiler/ac.exe   (needs mingw-w64)
```

### Repository layout

| Path | Contents |
|------|----------|
| `ac-compiler/` | Compiler source (`src/`, `include/`, `Makefile`), the embedded wasm blob sources (`wasm/`), and prebuilt binaries: `ac` (Linux x86-64), `ac.exe` (Windows x86-64), `ac.arm` (Linux AArch64) |
| `library/` | The standard libraries: `ilib/` (native C++ cores with FFI bindings per backend), `elib/` (AC-source packages) |
| `examples/` | Runnable example programs |
| `install.sh` | The installer described above |

---

## Quick Start

```ac
AC->PY

<mainloop>
    Term.display $Hello, AC$
<mainloop>
```

Every executable AC file needs:
1. A backend declaration on line 1 (`AC->XX`)
2. A `<mainloop>` or `<StartHere>`/`<EndHere>` entry block

---

## CLI

```bash
ac file.ac                    # compile and run using backend declared in file
ac file.ac --target PY        # override backend
ac file.ac --all              # compile to every backend at once
ac file.ac --no-run           # compile only, don't execute
ac file.ac --runtime          # run without compile-time constant folding
ac file.ac --time             # print compilation and execution times separately
ac file.ac --stop-after-ir    # print IR (.lir) and exit
```

Full flag reference:

| Flag | Meaning |
|------|---------|
| `--target <B>` / `--backend <B>` | Override backend declared in file |
| `-t <B>` | Short form of `--target` |
| `--all`, `-all` | Compile to every registered backend |
| `--no-run` | Compile only; do not execute |
| `--runtime` | Disable compile-time constant folding |
| `--force` | Ignore existing cache files |
| `--no-cache` | Disable cache reads and writes entirely |
| `--allow-foreign` | Enable raw `<Foreign>` passthrough blocks |
| `-O0` / `-O1` / `-O2` / `-O3` | Optimization level (BNY; default `-O2`) |
| `-g` | Include debug info in BNY output |
| `--stop-after-ir` | Print LIR text and exit |
| `--stop-after-cfg` | Stop after CFG (BNY debug) |
| `--stop-after-ssa` | Stop after SSA (BNY debug) |
| `--stop-after-opt` | Stop after optimization passes (BNY debug) |
| `--save-ast` | Save `.acc` AST cache |
| `--save-ir` | Save `.lir` IR text |
| `--version`, `-v` | Print version |
| `--help`, `-h` | Print usage |

---

## Backends

| Declaration | Output | Runner |
|-------------|--------|--------|
| `AC->BNY` | `.acb` | AC's own native x86-64 binary |
| `AC->ARM` | `.acb` | AC's own native AArch64 (Linux) binary — **experimental**, see below |
| `AC->ASM` | `.asm` | x86-64 NASM assembly → `nasm` + `gcc` |
| `AC->C` | `.c` | C (C99) → compiled with `gcc` |
| `AC->C++` or `AC->CPP` | `.cpp` | C++ (C++17) → compiled with `g++` |
| `AC->PY` | `.py` | Python 3 |
| `AC->JS` | `.js` | Node.js JavaScript |
| `AC->HTML` | `.html` | Browser HTML |
| `AC->Java` | `.java` | Java → `javac` / `java` |
| `AC->RS` | `.rs` | Rust → `rustc` |
| `AC->GO` | `.go` | Go → `go run` |
| `AC->V` | `.v` | V → `v run` |
| `AC LIB` | (source only) | Library file; cannot be compiled directly |
| `AC->LIB` | `.cpp` + `.h` | Shared library (`.so` / `.dll`) |

`AC LIB` marks a source library used via `use flib`. `AC->LIB` builds a compiled shared library.

**`AC->ARM` is experimental.** It handles integers, floats, strings, arrays, dicts, functions and
recursion, and bundles/classes/tuples, printing floats exactly like the other backends. Still
missing: `try`/`catch`, `atomic`, generators (`yield`), and ilib calls (the dynamic-linking path
has a known bug). Anything unimplemented is a compile-time error, never silent wrong output.

---

## Lexical Rules

### Comments

AC has exactly one comment form: C-style `/* ... */`. It covers both single-line and multi-line comments.

```ac
/* a single-line comment */

/* a multi-line
   comment */
```

There is no `* ... *` or `#` comment. `*` is always multiply, and `#` is always an operator prefix (`#>`, `#<`, `#=`, boolean NOT) — neither begins a comment.

### Indentation

AC uses indentation for blocks. Tabs count as 4 spaces.

```ac
IF ready
    Term.display $yes$
OTHER
    Term.display $no$
```

### Strings

| Form | Meaning |
|------|---------|
| `$text$` | Standard string. Supports `\n`, `\t`, `\r`, `\\`, `\$` |
| `r$text$` | Raw string. No escape processing |
| `"text"` | Not an AC string — a syntax error. Write `$text$` |
| `\ws` | Whitespace sentinel literal → compiles to `" \t\n\r"` |

### Numbers

Integer and float literals. Hex-like text with `x` is recognized by the lexer.

### Booleans and Null-Like Values

```ac
truth = True     /* also TRUE, true */
lie   = False    /* also FALSE, false */
n     = null     /* backend null: None / null / nullptr / nil / none */
empty = nil      /* AC empty-set sentinel ∅ — distinct from null */
```

`nil` represents the empty set. `[nil]` is the set containing the empty set `{∅}`.

---

## Variables and Assignment

```ac
x    = 10
name = $Ada$
flag = True
data = null
```

**Compound assignment:**

| Syntax | Meaning |
|--------|---------|
| `x += y` | Add |
| `x -= y` | Subtract |
| `x *= y` | Numeric multiply |
| `x @= y` | Polymorphic multiply |
| `x /= y` | Divide |

---

## Operators

### Arithmetic

| Operator | Meaning | Notes |
|----------|---------|-------|
| `+` | Addition | Also string/list concatenation |
| `-` | Subtraction / unary negate | |
| `*` | Numeric multiply |Not like Python *|
| `@` | Polymorphic multiply | Numbers, strings, or lists |
| `/` | Smart division | Int when the quotient is whole, else float — whatever the operand types: `4/2 = 2`, `8/2.0 = 4`, `5/2 = 2.5` |
| `//` | Integer division | Truncates toward zero; `5//2 = 2` |
| `///` | Float division | Always yields a float; `8///2.0 = 4.0` |
| `math.mod(a, b)` | Modulo | math library function |
| `a xsub b` | Inclusive distance | `|a − b| + 1` |

```ac
a = 5 / 2      /* a = 2.5 (5 doesn't divide evenly) */
d = 4 / 2      /* d = 2   (clean → int) */
b = 5 // 2     /* b = 2   */
e = 4 /// 2    /* e = 2.0 (always float) */
c = 7 xsub 3   /* c = 5   */
```

**How whole numbers display.** A value that came from `/` (or `math.mod`) shows as an integer when
it is whole — `Term.display 8 / 2.0` prints `4`, and so does `to_string(8 / 2.0)`. A float that did
*not* come from `/` keeps its `.0`: `8 /// 2.0` and the literal `8.0` print `8.0`, as do
`math.sqrt(16.0)` and other library results. Fractional values print with up to 16 significant
digits (`%.16g`). This is a display rule only: values are the same numbers either
way, and a whole `/` result is still usable anywhere an integer is (indexing, `//`, comparisons).

### Comparison

| Operator | Meaning |
|----------|---------|
| `is` | Equal (`==`) |
| `#=` | Not equal (`!=`) |
| `<` | Less than |
| `>` | Greater than |
| `#>` | Not greater (≤) |
| `#<` | Not less (≥) |

### Logical

| Operator | Meaning |
|----------|---------|
| `&` / `and` / `AND` | Logical AND |
| `or` / `OR` | Logical OR |
| `\|` | XOR |
| `#\|` | XNOR |
| `#` (prefix) | Boolean NOT |
| `not` | Boolean NOT (synonym for `#`) |

### Bitwise

Bitwise operators are words, except `~`:

| Operator | Meaning |
|----------|---------|
| `a band b` | Bitwise AND |
| `a bor b` | Bitwise OR |
| `a bxor b` | Bitwise XOR |
| `bnot a` / `~ a` | Bitwise NOT |
| `a ptm n` | Power-two multiply: `a * 2^n` (shift left) |
| `a ptd n` | Power-two divide: `a / 2^n` (shift right) |

```ac
Term.display 12 band 10    /* 8  */
Term.display 5 ptm 3       /* 40 */
```

### Precedence (highest to lowest)

1. Unary `-`, `#`
2. `*`, `/`, `//`, `///`, `@`
3. `+`, `-`, `xsub`
4. `is`, `#=`, `<`, `>`, `#>`, `#<`, `overlap`
5. `&`, `and`
6. `\|`, `#\|`
7. `or`

### `~>` — Call Rewiring (proposed, not yet implemented)

`~>` redirects a call to a different function, in place, at the call site. The original callee
is never invoked — `~>` fully replaces it, forwarding the same argument list:

```ac
result = malloc(5) ~> my_custom_heap_function
/* equivalent to: result = my_custom_heap_function(5) */
/* malloc is never called — my_custom_heap_function receives its arguments instead */
```

This is the call-site analog of `alias` (see [Scope and Aliasing](#scope-and-aliasing)): where
`alias x = y` gives two *variables* one shared identity, `expr ~> target` gives a *call
expression* a different destination without editing its argument list. It's meant for swapping
out a low-level primitive (`malloc`, `free`, an ilib call) for a custom implementation at one
call site, without rewriting every argument by hand.

**Shape:**
- LHS must be a call expression: `callee(args...)`. `callee` names the function being
  *replaced* — it is parsed for its argument list only and is never resolved or invoked, so it
  does not need to exist.
- RHS must be a bare function reference (a name resolvable in scope: user-defined or ilib),
  taking the same argument list `callee` would have.
- Arguments are evaluated exactly once, left to right, exactly as written on the LHS.
- Lexes as one token (`~>`), taking precedence over lexing `~` (bitwise NOT) followed by `>`
  (greater-than) — the lexer must match the two-character operator greedily before falling back
  to `~` alone.
- Binds to the call expression as a postfix modifier, not as a general value-level binary
  operator — it does not have a slot in the precedence table above, the way `is`/`type` casts
  don't either.
- Chaining (`f(x) ~> g ~> h`) is undefined for now and reserved for future design — don't rely
  on any particular behavior from it yet.

This section describes the intended design so it can be implemented against a stable spec; the
lexer/parser/IR/codegen work itself hasn't started.

---

## Type Coercion

```ac
to_int    x
to_dec    x
to_string x
to_bool   x

to_int    n = expr
to_dec    price = total / count
to_string label = count
```

`to_string` of a float follows the same display rule as `Term.display` (a whole `/` result gives
`"4"`, `8 /// 2.0` gives `"4.0"`); `to_int` truncates toward zero.

### Sized and atomic integers

```ac
short s = 1000      /* 32-bit signed integer */
mini  m = 100       /* 16-bit signed integer */
atomic hits = 0     /* 64-bit integer; every read/write is a global critical section */
hits += 1
```

An `atomic` read-modify-write (`hits += 1`) is one indivisible critical section on every backend,
so it is safe under `quickthread`.

---

## Immutability and Copying

### `const` — immutable binding

```ac
const MAX = 100
const PI  = 3.14159265358979323846
```

Reassigning a `const` is a compile-time `Preposterous:` error. Backends emit the appropriate immutable form:

| Backend | Emitted form |
|---------|-------------|
| Python | `name: Final = value` (`from typing import Final`) |
| JS | `const name = value` |
| C | `const ac_int name = value` |
| C++ | `const long long name = value` |
| Java | `final long name = value` |
| Rust | `let name = value` (immutable by default) |
| Go | `name := value /* const */` |

### `cp` — explicit deep copy

```ac
cp backup = original
```

Without `cp`, assigning a collection creates a reference. `cp` forces a deep copy so mutations to `backup` do not affect `original`. Backends use `copy.deepcopy()` in Python, `structuredClone()` in JS, and value semantics elsewhere.

---

## Extended Numeric Types

### `math.LongInt` — signed 96-bit integer

Range: `-(2^95)` through `2^95 − 1`

```ac
use ilib math

<mainloop>
    math.LongInt big = pow(2, 68)
    Term.display big
<mainloop>
```

Constant folding handles `+`, `-`, `*`, `@`, `/`, `mod`, `pow`. Values that exceed the 96-bit bounds fold to `inf`.

### `math.GoodDec` — exact decimal

No binary-float rounding. Stored internally as `{unscaled, scale}` where `value = unscaled × 10^scale`.

```ac
use ilib math

<mainloop>
    math.GoodDec price = 1.23
    math.GoodDec tax   = 0.1 + 0.2
    Term.display price   /* 1.23 */
    Term.display tax     /* 0.3 (exact) */
<mainloop>
```

`math.GoodDec x = {unscaled, scale}` raw tuple syntax is not allowed; use normal decimal expressions.

---

## Collections

### Lists

```ac
nums  = [1, 2, 3, 4, 5]
mixed = [$hello$, 42, True]
empty = []

Term.display nums[1]    /* first element (1-based) */
nums[2] = 99
```

Indexes are 1-based in AC source; the compiler converts to 0-based for all backends.

### Tuples

```ac
coords = {10, 20, 30}
```

### Dictionaries

```ac
person = {
    name: $Ada$
    age:  37
}

other = dict-{name: $Grace$, age: 85}
```

### Range and Sequence

```ac
FOR i in range 10          /* i = 0..9 */
FOR n in sequence(3, 7)    /* n = 3..6 */
```

`range N` generates integers 0 to N−1. `sequence(x, y)` generates x up to (but not including) y — exclusive of y, like Python's `range` — and stops early if x >= y.

---

## Expressions

```ac
value  = (2 + 3) * 4
neg    = -value
flag   = not ready
result = func(1, 2, 3)
method = obj.method(1, 2)
item   = list[1]
named  = makeThing(width=10, height=20)
```

---

## Conditionals

```ac
IF score > 90
    Term.display $A$
ELSEIF score > 75
    Term.display $B$
OTHER
    Term.display $C$
```

`skip` exits the rest of the current `IF`/`ELSEIF`/`OTHER` chain:

```ac
IF done
    skip
OTHER
    Term.display $not done$
```

### Ternary

`condition | when_true, # when_false` — read `|` as "such that":

```ac
label = n > 5 | $big$, # $small$
Term.display 3 < 2 | 10, # 20    /* 20 */
```

### `cond` — switch-style dispatch

Evaluates the scrutinee once, then tests each `is` branch:

```ac
cond x
    is 1
        Term.display $one$
    is 2
        Term.display $two$
    OTHER
        Term.display $other$
```

---

## Loops

### WHILST

```ac
i = 0
WHILST i < 10
    Term.display i
    i += 1
OTHER
    Term.display $never started$
```

The `OTHER` clause runs when the loop condition is false from the start.

### FOR

```ac
FOR item in items
    Term.display item

FOR i in range 5
    Term.display i

FOR n in sequence(1, 100)
    Term.display n
```

### Loop Control

```ac
/end        /* break out of enclosing loop; at top level, exits program */
continue    /* skip to next iteration */
pass        /* no-op placeholder */
```

`break` is not in AC — use `/end`.

---

## Functions

```ac
Make square func(n)
    return n * n

Make fib func(n)
    IF n #> 1
        return n
    return fib(n - 1) + fib(n - 2)
```

Both `Make` and `make` are accepted. `pass` is a valid empty body.

**Functions as arguments:**

```ac
Make apply func(f, n)
    return f(n)

result = apply(square, 5)    /* result = 25 */
```

Backends emit the correct first-class function type: function pointers in C, `std::function` in C++, `LongUnaryOperator` in Java, `fn()` in Rust, `func()` in Go.

**Pure functions** with all-constant arguments are folded at compile time.

### Generators

A function containing `yield` is a generator; iterate it with `FOR`:

```ac
Make countdown func(n)
    i = n
    WHILST i > 0
        yield i
        i -= 1

<mainloop>
    FOR v in countdown(3)
        Term.display v      /* 3, 2, 1 */
<mainloop>
```

A generator's `return` value is discarded (there is no `.send()`).

### `quickthread`

`quickthread f(args)` runs `f` concurrently on backends that have real lightweight threads (a
goroutine on Go); on every other backend it runs the call synchronously, with the same result and
no parallelism.

---

## Bundles

`bundle` defines a struct-like data type with optional methods.

```ac
bundle Point
    x = 0
    y = 0
    Make dist func(self)
        return math.sqrt(self.x @ self.x + self.y @ self.y)
```

By default, all members are public (struct-like, no modifiers needed).

### Access Modifiers

Once any modifier is used, every member must be explicitly annotated:

```ac
bundle Dog
    public  name = $Rex$
    private age  = 3
    public  Make speak func(self)
        Term.display self.name
```

Mixing modifiers without annotating every member is a compile-time `Preposterous:` error.

### Instantiation

```ac
p     = Point()
p.x   = 10
value = p.dist()
```

The special method name `init` becomes the constructor. Backends emit: Python `class` + `__init__`, Rust `struct` + `impl`, Java `class`, Go `struct` + methods, C `struct` + functions.

---

## Tags

Tags open and close with the **same** tag name — never XML-style `</tagname>`:

```ac
<mainloop>
    Term.display $Hello$
<mainloop>
```

`<StartHere>` is the exception: it closes with `<EndHere>`.

### Core Tags

| Tag | Meaning |
|-----|---------|
| `<mainloop>` | Program entry point |
| `<StartHere>` / `<EndHere>` | Alternative entry; implicit infinite loop |
| `<bound>` | Scoped block; variables inside cannot leak out |
| `<free>` | Free/global block; variables inside are promoted to global scope |
| `<Local>` | Local scope section |
| `<shutoff>` | Compiled as `__ac_shutoff__()`; called automatically before `/stop` |
| `<Foreign>` | Raw passthrough to target language; requires `--allow-foreign` |

### Custom Tags

```ac
def tag <setup>

<setup>
    Term.display $initializing$
<setup>
```

---

## Slash Commands

```ac
/kill           /* hard terminate (os.abort) */
/stop           /* graceful stop — runs <shutoff> then exits cleanly */
/end            /* break current loop; at top level acts like /stop */
/restart        /* re-run program body once from top */
/halt 1.5       /* pause for 1.5 seconds */
/halt math.inf  /* treated as /stop */
```

---

## Scope and Aliasing

### `free` — declare as globally scoped

```ac
free x, y, z
```

Inside a function, marks `x`, `y`, `z` as global (like Python's `global`).

### `<free>` block

All variables declared inside are globally promoted:

```ac
<free>
    counter = 0
    score   = 0
<free>
```

### `<bound>` block

Variables declared inside cannot leak out:

```ac
<bound>
    tmp = heavy_compute()
    Term.display tmp
<bound>
/* tmp does not exist here */
```

### `alias` — bidirectional live binding

```ac
alias x = y
```

Assigning to either `x` or `y` writes both. Transitive: `alias a = b` and `alias b = c` means assigning any of `a`, `b`, `c` writes all three.

### `destroy` — remove variable

```ac
destroy x
```

Deallocates / removes `x` from scope.

---

## Error Handling

### try / catch / after

```ac
try
    result = 10 / 0
catch
    report err
    Term.display err
catch ZeroDivisionError
    Term.display $division by zero$
after
    Term.display $always runs$
```

- `catch` (bare) — catch-all
- `catch TypeName` — typed catch (backends that support it)
- Multiple `catch` clauses tested in order
- `report <var>` binds the exception message; must be the first line in a `catch` block if used
- `after` is optional; always runs after all catch branches

### raise

```ac
raise ERR                      /* "Preposterous: Fatality occurred" + abort */
raise ERR($custom message$)    /* "Preposterous: custom message" + abort */
raise hint($try this instead$) /* "Suggestion: try this instead" (non-fatal, stderr) */
raise toxic($deprecated$)      /* "Toxic: deprecated" (non-fatal, stderr) */
raise MyClause($text$)         /* "MyClause: text" (non-fatal, stderr) */
```

---

## IO

### Terminal

```ac
Term.display value
Term.display $literal string$
answer = Term.ask $Enter your name: $
```

`Term.display` is the only terminal output keyword.

### Browser / HTML only

```ac
alert $Message$
ok = sure $Continue?$    /* window.confirm → bool */
print_page               /* window.print() */
```

### Styled display (HTML renders, others fall back to plain)

```ac
bold.display      $text$
italic.display    $text$
header.display    $text$
link.display      $url$
title.display     $text$
code.display      $text$
para.display      $text$
underline.display $text$
mark.display      $text$
hr.display        $text$
```

---

## String Methods

String methods are auto-dispatched when calling `.method` on a string variable (string-cheese):

```ac
use ilib string-cheese

s = $Hello, World$
Term.display s.lower      /* hello, world */
Term.display s.upper      /* HELLO, WORLD */
Term.display s.strip      /* strips whitespace */
Term.display s.length     /* 13 */
Term.display s.format     /* formatted string */
```

Multi-argument methods:

```ac
Term.display s.find($World$)          /* 8 */
Term.display s.replace($World$, $AC$) /* Hello, AC */
Term.display s.split($, $)            /* [$Hello$, $World$] */
Term.display s.count($l$)             /* 3 */
Term.display s.startswith($Hello$)    /* True */
Term.display s.endswith($World$)      /* True */
```

Alternate casings are accepted: `.LOWER`, `.UPPER`, `.STRIP`, `.TRIM`, `.LEN`, `.FIND`, `.REPLACE`, etc.

---

## Method Calls and Properties

```ac
obj.prop = value
obj.prop += value
obj.method(1, 2)
obj.method $string argument$
obj.config width=10 - height=20
```

Attribute separator in `config` calls is ` - ` (space-dash-space).

### `fn` — single-line functions

```ac
fn square(n)=n^2
fn add(a, b)=a + b
fn origin()=0, 0            /* returns a tuple, like `return 0, 0` */

<mainloop>
    Term.display square(5)      /* 25 */
    Term.display add(2, 3)      /* 5  */
```

`fn name(args)=expr` is exactly `Make name func(args)` whose whole body is `return expr` — same scoping,
same backends, and the function can be passed as a value (`apply(square, 6)`). Use `Make` when the body
needs more than one statement. (`fn` used to prefix "multiply / method-chain" lines; that form is gone.)

---

## Eval

```ac
code   = $3 + 4 * 2$
result = eval(code)           /* evaluates string as AC expression */
safe   = lazy_eval(code)      /* safe evaluation; on error returns exception object */
```

---

## Imports and Libraries

```ac
use ilib math
use elib somepackage
use clib systemlib
use flib /path/to/lib.so
use flib mylib.ac
use datac data.datac as records

from ilib math use sin, cos, sqrt
```

| Prefix | Source |
|--------|--------|
| `ilib` | Built-in AC libraries in `library/ilib/` |
| `elib` | External packages installed via `atar` |
| `clib` | Custom/system libraries in `library/clib/` |
| `flib` | Foreign compiled library (`.so`/`.dll`) or AC source file |
| `datac` | Compile-time data file (`.datac`) |

**Namespace imports:**

```ac
using header math          * bring math./* into flat scope */
using math                 /* synonym */
using math.sin             /* bring only sin into flat scope */
using math.sin, math.cos   /* multiple symbols */
using namespace ilib       /* unqualified calls resolve to imported ilib namespaces */
```

**datac — compile-time data baking:**

```ac
use datac data/people.datac as people

<mainloop>
    FOR row in people
        Term.display row[$name$]
<mainloop>
```

---

## Math Library

```ac
use ilib math
```

### Constants

```ac
math.pi           /* π */
math.e            /* e */
math.tau          /* τ = 2π */
math.em           /* Euler-Mascheroni γ ≈ 0.5772 */
math.phi          /* golden ratio φ ≈ 1.6180 */
math.inf          /* +∞ */
```

Precision-call form (returns string to N decimal places):

```ac
math.pi(50)
math.e(100)
math.phi(20)
```

### Scalar Functions

```ac
math.sqrt(x)       math.cbrt(x)        math.pow(base, exp)
math.abs(x)        math.abs.int(x)     math.hypot(a, b)
math.floor(x)      math.ceil(x)        math.round(x)
math.clamp(x, lo, hi)
math.ln(x)         math.log(base, x)   math.log2(x)    math.log10(x)
math.mod(a, b)     math.mod.int(a, b)
math.to_int(x)     math.to_dec(x)
math.gcd(a, b)     math.lcm(a, b)
math.is_prime(n)
math.eval($expr$)
```

### Trigonometry

```ac
math.sin(x)    math.cos(x)    math.tan(x)
math.csc(x)    math.sec(x)    math.cot(x)
math.asin(x)   math.acos(x)   math.atan(x)
math.acsc(x)   math.asec(x)   math.acot(x)
math.atan2(y, x)
math.deg2rad(x)  math.rad2deg(x)
```

### Statistics and Aggregates

```ac
math.sigma(list)       /* Σ sum */
math.PI(list)          /* Π product (uppercase PI; math.pi lowercase is the constant) */
math.gradient(list)    /* numerical gradient */

math.stat.avg(list)
math.stat.median(list)
math.stat.q1(list)
math.stat.q3(list)
math.stat.mode(list)
math.stat.min(list)
math.stat.max(list)
```

Known constant math calls are folded at compile time.

---

## Regex Library

```ac
use ilib regex

<mainloop>
    Term.display regex.test($hello world$, $\bworld\b$)
    Term.display regex.search($foo 42$, $\d+$)
    Term.display regex.replace($cat$, $cat$, $dog$)
<mainloop>
```

Functions: `test`, `match`, `search`, `replace`, `replace_all`, `count`, `find_all`, `groups`, `split`, `escape`

Regex patterns are standard PCRE / ECMAScript — no custom AC pattern DSL.

---

## OS Library

```ac
use ilib os

<mainloop>
    here = os.cwd()
    os.mkdir($tmp-ac$)
    os.write_to($tmp-ac/out.txt$, $hello$)
    Term.display os.read($tmp-ac/out.txt$)
<mainloop>
```

Functions: `bash`, `sbash`, `app_open`, `mkfile`, `rmfile`, `mkdir`, `rmdir`, `exists`, `cwd`, `env`, `write_to`, `append_to`, `read`, `pid`

`os.bash(cmd)` starts a shell command and returns its child PID. `os.wait(pid)` waits for it and returns its exit code. `os.sbash(cmd)` remains the protected blocking variant — it rejects `sudo`, `su`, `nohup`, `screen`, `tmux`, and trailing `&`. File helpers return `0` on success, `-1` on failure; `exists`, `cwd`, `env`, `read` return their natural values.

`os.pid(process)` returns the PID from a process handle, which makes `os.pid(os.bash(cmd))`
explicit and composable.

---

## Event System

```ac
configure event-listener
    use listener to establish rule
        on value is space
            jump()
        on value is w
            moveUp()
        on value is s
            moveDown()
```

`input keyname` simulates a key press:

```ac
input KEY_W
```

Keybinding shorthand:

```ac
bind KEY_W to moveUp
bind space to jump
```

---

## Foreign Blocks

```ac
<Foreign>
    import numpy as np
    print(np.array([1, 2, 3]))
<Foreign>
```

Requires `--allow-foreign`. Content is passed verbatim — must already be valid target-language code. BNY rejects foreign blocks entirely:

```
Toxic: User attempts fluency in CPU
```

---

## Error and Diagnostic System

All fatal messages use the `Preposterous:` prefix:

| Type | Example output |
|------|---------------|
| Parse error | `Preposterous: ParseError at line 7 col 3: unexpected token` |
| Compile error | `Preposterous: CompileError: unknown backend APL` |
| Semantic error | `Preposterous: SemanticError: bundle mixes modifiers` |
| Const reassignment | `Preposterous: Cannot reassign const variable 'MAX'` |
| Runtime fatal | `Preposterous: my custom message` (from `raise ERR`) |

Non-fatal diagnostic clauses write to stderr and continue execution:

| Clause | Output prefix |
|--------|--------------|
| `raise hint(...)` | `Suggestion:` |
| `raise toxic(...)` | `Toxic:` |
| `raise AnyName(...)` | `AnyName:` |

---

## IR and Optimization

Pipeline:

```
AC source → lexer → Pratt parser → AST → unified IR → library lowering → backend codegen
```

Optimization passes:

| Pass | Purpose |
|------|---------|
| Constant folding | Fold known arithmetic and comparisons at compile time |
| Local constant folding | Track constants in straight-line IR |
| Constexpr function folding | Evaluate pure user functions with all-constant arguments |
| Scalar math constexpr | Fold known `ilib math` calls |
| Copy propagation | Substitute temp results into destination variables |
| DCE | Remove unused temp-producing instructions |

---

## Cache Files

| File | Meaning |
|------|---------|
| `.acc` | Binary AST cache (`ACC1`, version 9) |
| `.irc` | Binary IR cache (`ACIR`, version 17); keyed by source + backend + lib FFI mtimes |
| `.lir` | Human-readable IR text; useful for BNY/ASM debugging |

Cache marker: `ac-irc-v12-longint-alias`

Use `--force` or `--no-cache` when working on compiler internals.

---

## Keyword Index

**Control flow:** `IF`, `ELSEIF`, `OTHER`, `FOR`, `in`, `WHILST`, `cond`, `is`, `continue`, `skip`, `pass`, `return`

**Slash commands:** `/kill`, `/stop`, `/end`, `/restart`, `/halt`

**Exceptions:** `try`, `catch`, `report`, `after`, `raise`, `ERR`

**Functions:** `Make` / `make`, `func`, `fn`, `eval`, `lazy_eval`, `yield`, `quickthread`

**Imports:** `use`, `using`, `from`, `as`, `ilib`, `elib`, `clib`, `flib`, `datac`, `header`

**Collections:** `range`, `sequence`, `dict`

**Types / coercions:** `to_int`, `to_dec`, `to_string`, `to_bool`, `short`, `mini`, `atomic`, `const`, `cp`

**Scope / aliasing:** `alias`, `free`, `destroy`, `bound`

**Bundles:** `bundle`, `private`, `public`

**Events:** `configure`, `event-listener`, `listener`, `establish`, `rule`, `value`, `on`, `input`, `bind`, `to`, `many`

**IO:** `Term.display`, `Term.ask`, `alert`, `sure`, `print_page`

**Literals:** `True`, `False`, `null`, `nil`

**Operators:** `not`, `#`, `overlap`, `xsub`, `and`, `AND`, `or`, `OR`, `band`, `bor`, `bxor`, `bnot`, `~`, `ptm`, `ptd`, `///`, `~>` (proposed)

**Reserved / partial:** `programLoop`, `temp`, `at`, `save`, `type`

---

## License

AC is free software under the **GNU General Public License, version 3** (`GPL-3.0-only`) — see
[LICENSE](LICENSE), which also carries one additional term under section 7 of the GPL: **author
anonymity**. The copyright holder is the pseudonym *AbuCodingAI*; that pseudonym satisfies every
copyright-notice and attribution requirement in the license, and nobody may be required to
identify the author by legal name. In short: you may use, modify and redistribute AC under the
GPL, and you must keep the pseudonymous notice — but you never need to know, or be told, who
wrote it.
