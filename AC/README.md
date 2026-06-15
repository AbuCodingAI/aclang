# AC Language

AC (AbuCompiled) is a high-level, indentation-based, multi-target compiled language. Write once, compile to any of thirteen backends. The compiler lexes AC source into a Pratt-parsed AST, lowers it to a unified IR, runs optimization passes, and emits the selected target language.

Current version: **v0.3.1** — see `ac --version`. (vsix: 0.3.1)

---

## Install

```bash
npm install -g aclang
```

From source:

```bash
cd AC/ac-compiler
make
```

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
| `AC->ASM` | `.s` | x86-64 assembly → linked with `gcc` |
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

---

## Lexical Rules

### Comments

```ac
* single-line comment (asterisk both sides) *

/* multi-line
   comment */
```

The `*` character is a comment delimiter in most positions. Inside `fn` expression lines it becomes numeric multiply.

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
| `"text"` | Double-quoted. Valid inside `fn` expressions only; a syntax error elsewhere |
| `\ws` | Whitespace sentinel literal → compiles to `" \t\n\r"` |

### Numbers

Integer and float literals. Hex-like text with `x` is recognized by the lexer.

### Booleans and Null-Like Values

```ac
truth = True     * also TRUE, true *
lie   = False    * also FALSE, false *
n     = null     * backend null: None / null / nullptr / nil / none *
empty = nil      * AC empty-set sentinel ∅ — distinct from null *
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
| `x |= y` | XOR |

---

## Operators

### Arithmetic

| Operator | Meaning | Notes |
|----------|---------|-------|
| `+` | Addition | Also string/list concatenation |
| `-` | Subtraction / unary negate | |
| `*` | Numeric multiply |Not like Python *|
| `@` | Polymorphic multiply | Numbers, strings, or lists |
| `/` | True division | Always yields float; `5/2 = 2.5` |
| `//` | Integer division | Truncates toward zero; `5//2 = 2` |
| `math.mod(a, b)` | Modulo | math library function |
| `a xsub b` | Inclusive distance | `|a − b| + 1` |

```ac
a = 5 / 2      * a = 2.5 *
b = 5 // 2     * b = 2   *
c = 7 xsub 3   * c = 5   *
```

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

### Precedence (highest to lowest)

1. Unary `-`, `#`
2. `*`, `/`, `//`, `@`
3. `+`, `-`, `xsub`
4. `is`, `#=`, `<`, `>`, `#>`, `#<`, `overlap`
5. `&`, `and`
6. `\|`, `#\|`
7. `or`

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

---

## Immutability and Copying

### `const` — immutable binding

```ac
const MAX = 100
const PI  = 3.14159
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
    Term.display price   * 1.23 *
    Term.display tax     * 0.3 (exact) *
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

Term.display nums[1]    * first element (1-based) *
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
FOR i in range 10          * i = 0..9 *
FOR n in sequence(3, 7)    * n = 3..7 *
```

`range N` generates integers 0 to N−1. `sequence(x, y)` generates x to y inclusive and stops early if x > y.

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

The `OTHER` clause runs when the loop condition is false from the start. GL special form:

```ac
WHILST many hitbox overlap
    Term.display $collision$
```

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
/end        * break out of enclosing loop; at top level, exits program *
continue    * skip to next iteration *
pass        * no-op placeholder *
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

result = apply(square, 5)    * result = 25 *
```

Backends emit the correct first-class function type: function pointers in C, `std::function` in C++, `LongUnaryOperator` in Java, `fn()` in Rust, `func()` in Go.

**Pure functions** with all-constant arguments are folded at compile time.

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

### GL / GUI Tags

| Tag | Meaning |
|-----|---------|
| `<gui>` | GUI container area |
| `<OBJECT>` | Object declarations |
| `<SCREEN>` | Screen / style definitions |
| `<LOGIC>` | Logic / event section |

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
/kill           * hard terminate (os.abort) *
/stop           * graceful stop — runs <shutoff> then exits cleanly *
/end            * break current loop; at top level acts like /stop *
/restart        * re-run program body once from top *
/halt 1.5       * pause for 1.5 seconds *
/halt math.inf  * treated as /stop *
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
* tmp does not exist here *
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
raise ERR                      * "Preposterous: Fatality occurred" + abort *
raise ERR($custom message$)    * "Preposterous: custom message" + abort *
raise hint($try this instead$) * "Suggestion: try this instead" (non-fatal, stderr) *
raise toxic($deprecated$)      * "Toxic: deprecated" (non-fatal, stderr) *
raise MyClause($text$)         * "MyClause: text" (non-fatal, stderr) *
```

---

## IO

### Terminal

```ac
Term.display value
Term.display $literal string$
answer = Term.ask $Enter your name: $
```

`Term.display` is the only terminal output keyword. `Term.print`, `Term.log`, and `Term.write` do not exist.

### Browser / HTML only

```ac
alert $Message$
ok = sure $Continue?$    * window.confirm → bool *
print_page               * window.print() *
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
Term.display s.lower      * hello, world *
Term.display s.upper      * HELLO, WORLD *
Term.display s.strip      * strips whitespace *
Term.display s.length     * 13 *
Term.display s.format     * formatted string *
```

Multi-argument methods:

```ac
Term.display s.find($World$)          * 8 *
Term.display s.replace($World$, $AC$) * Hello, AC *
Term.display s.split($, $)            * [$Hello$, $World$] *
Term.display s.count($l$)             * 3 *
Term.display s.startswith($Hello$)    * True *
Term.display s.endswith($World$)      * True *
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

### `fn` Lines and Method Chains

```ac
fn Term.display $Hello$ & Term.display $World$
```

`&` chains calls with separate arguments. `&&` chains calls reusing the previous argument.

---

## Eval

```ac
code   = $3 + 4 * 2$
result = eval(code)           * evaluates string as AC expression *
safe   = lazy_eval(code)      * safe evaluation; on error returns exception object *
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
using header math          * bring math.* into flat scope *
using math                 * synonym *
using math.sin             * bring only sin into flat scope *
using math.sin, math.cos   * multiple symbols *
using namespace ilib       * unqualified calls resolve to imported ilib namespaces *
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
math.pi           * π *
math.e            * e *
math.tau          * τ = 2π *
math.em           * Euler-Mascheroni γ ≈ 0.5772 *
math.phi          * golden ratio φ ≈ 1.6180 *
math.inf          * +∞ *
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
math.sigma(list)       * Σ sum *
math.product(list)     * Π product *
math.gradient(list)    * numerical gradient *

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

Functions: `bash`, `sbash`, `app_open`, `mkfile`, `rmfile`, `mkdir`, `rmdir`, `exists`, `cwd`, `env`, `write_to`, `append_to`, `read`

`os.bash(cmd)` runs a shell command and returns exit code. `os.sbash(cmd)` is the safer variant — it rejects `sudo`, `su`, `nohup`, `screen`, `tmux`, and trailing `&`. File helpers return `0` on success, `-1` on failure; `exists`, `cwd`, `env`, `read` return their natural values.

---

## GL Library

```ac
AC->JS

use ilib gl

<mainloop>
    <gui>
        <OBJECT>
            obj.Player
            Player.config item=square(50) - location=center - color=200,100,50
        <OBJECT>

        <SCREEN>
            Make Screen object
            Screen.OBJECT resize 1720x1080
            background.config color=green
            animate(60 fps)
        <SCREEN>

        <LOGIC>
            Make jump func(arg)
                arg.move_y -180

            <StartHere>
                configure event-listener
                    use listener to establish rule
                        on value is space
                            jump(Player)
            <EndHere>
        <LOGIC>
    <gui>
<mainloop>
```

AC's GL library is SDL2-backed, not pygame.

### Screen

```ac
gl.screen.init(w, h, $title$)
gl.screen.set_bg(r, g, b)
gl.screen.set_bg_by_name($color$)
gl.screen.set_fps(fps)
gl.screen.animate()
gl.screen.w()    gl.screen.h()
```

### Object Management

```ac
gl.obj.create($name$)
gl.obj.geometry($name$, w, h)
gl.obj.square($name$, size)
gl.obj.pos($name$, x, y)
gl.obj.color($name$, r, g, b)
gl.obj.color_by_name($name$, $color$)
gl.obj.velocity($name$, vx, vy)
gl.obj.set_speed($name$, speed)
gl.obj.set_direction($name$, deg)
gl.obj.speed_mult($name$, mult)
gl.obj.move_x($name$, dx)
gl.obj.move_y($name$, dy)
gl.obj.x($name$)     gl.obj.y($name$)
gl.obj.w($name$)     gl.obj.h($name$)
gl.obj.to_draw($name$)
gl.obj.circle_fall($name$, frac, $dir$)
gl.obj.circle_fell($name$)
gl.obj.set_spawn($name$)
gl.obj.regen($name$)
gl.obj.animate($name$, $dir$, speed)
gl.obj.is($name$)
gl.obj.is_draw($name$)
gl.obj.save_spawn($name$)
```

Direction constants: `RightDir`, `LeftDir`, `UpDir`, `DownDir`

### Drawing

```ac
gl.draw.create($name$)
gl.draw.curveshape($name$, $expr$)
gl.draw.vertex($name$, vx, vy)
gl.draw.line($name$, x1, y1, x2, y2, r, g, b)
gl.draw.circle($name$, cx, cy, radius, r, g, b)
gl.draw.clear($name$)
gl.draw.to_obj($name$)
```

### Hitbox

```ac
IF a.hitbox.coords overlap b.hitbox.coords
gl.hitbox.overlap($a$, $b$)
gl.hitbox.overlap_boundary($name$)
gl.hitbox.overlap_pattern($name$, $pattern$)
gl.hitbox.many_overlap()
```

### Keys

```ac
gl.key.pressed($key$)
gl.key.just_pressed($key$)
```

### Frame Loop

```ac
gl.frame.begin()
gl.frame.update(dt)
gl.frame.render()
gl.frame.end()
gl.frame.delta()
```

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
| Compile error | `Preposterous: CompileError: unknown backend RS` |
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
| `.acc` | Binary AST cache (`ACC1`, version 6) |
| `.irc` | Binary IR cache (`ACIR`, version 12); keyed by source + backend + lib FFI mtimes |
| `.lir` | Human-readable IR text; useful for BNY/ASM debugging |

Cache marker: `ac-irc-v12-longint-alias`

Use `--force` or `--no-cache` when working on compiler internals.

---

## Keyword Index

**Control flow:** `IF`, `ELSEIF`, `OTHER`, `FOR`, `in`, `WHILST`, `cond`, `is`, `continue`, `skip`, `pass`, `return`

**Slash commands:** `/kill`, `/stop`, `/end`, `/restart`, `/halt`

**Exceptions:** `try`, `catch`, `report`, `after`, `raise`, `ERR`

**Functions:** `Make` / `make`, `func`, `fn`, `eval`, `lazy_eval`

**Imports:** `use`, `using`, `from`, `as`, `ilib`, `elib`, `clib`, `flib`, `datac`, `header`

**Collections:** `range`, `sequence`, `dict`

**Types / coercions:** `to_int`, `to_dec`, `to_string`, `to_bool`, `const`, `cp`

**Scope / aliasing:** `alias`, `free`, `destroy`

**Bundles:** `bundle`, `private`, `public`

**Events:** `configure`, `event-listener`, `listener`, `establish`, `rule`, `value`, `on`, `input`, `bind`, `to`, `many`

**IO:** `Term.display`, `Term.ask`, `alert`, `sure`, `print_page`

**Literals:** `True`, `False`, `null`, `nil`

**Operators:** `not`, `#`, `overlap`, `xsub`, `and`, `AND`, `or`, `OR`

**Reserved / partial:** `programLoop`, `temp`, `at`, `save`, `type`

---

## License

MIT — see [LICENSE](LICENSE).
