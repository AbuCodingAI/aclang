# AC Language - Quick Reference Card

## 🚀 Compilation

```bash
./ac mycode.ac          # Auto-detect target from file
./ac mycode.ac PY       # Compile to Python
./ac mycode.ac BNY      # Compile to native binary
./ac mycode.ac ASM      # Compile to assembly
./ac mycode.ac -c       # Compile only (don't run)
./ac mycode.ac -f       # Force recompile (ignore cache)
```

## 🎯 Backends

| Code | Target | Output |
|------|--------|--------|
| `BNY` | Native Binary | `.acb` executable |
| `ASM` | x86-64 Assembly | `.s` file |
| `PY` | Python | `.py` script |
| `JS` | JavaScript | `.js` script |
| `C` | C | `.c` source |
| `CPP` or `C++` | C++ | `.cpp` source |
| `RS` | Rust | `.rs` source |
| `GO` | Go | `.go` source |
| `Java` | Java | `.java` source |
| `HTML` | HTML | `.html` page |

## 📝 Basic Syntax

```ac
AC->BNY                          * Backend declaration *

* This is a comment *

<mainloop>                       * Main program block *
    name = $Alice$               * String variable *
    age = 30                     * Integer variable *
    active = true                * Boolean *
    data = null                  * Null value *
    
    Term.display $Hello!$        * Print to console *
    /kill                        * Exit program *
<mainloop>
```

## 🔢 Operators

```ac
* Arithmetic *
a + b                            * Addition *
a - b                            * Subtraction *
a * b  or  fn a @ b             * Multiplication *
a / b                            * Division *
a % b                            * Modulo *

* Comparison *
a is b  or  a == b              * Equal *
a #= b  or  a != b              * Not equal *
a < b                            * Less than *
a > b                            * Greater than *
a <= b                           * Less or equal *
a >= b                           * Greater or equal *

* Compound Assignment *
a += 5                           * Add and assign *
a -= 3                           * Subtract and assign *
a *= 2                           * Multiply and assign *
a @= 2                           * Multiply and assign (fn) *
a /= 4                           * Divide and assign *
```

## 🔀 Control Flow

```ac
* If Statement *
IF condition
    * code *
ELSIF other_condition
    * code *
OTHER
    * code *

* While Loop *
WHILST condition
    * code *

* For Loop *
FOR item in list
    * code *
```

## 📦 Data Structures

```ac
* List *
numbers = [1, 2, 3, 4, 5]
numbers[0]                       * Access element *

* Tuple *
coords = (10, 20, 30)

* Dictionary *
person = {
    name: $Alice$
    age: 30
    city: $NYC$
}
```

## 🔧 Functions

```ac
* Simple Function *
Make greet func(name)
    Term.display $Hello, $
    Term.display name
    return 0

* Recursive Function *
Make factorial func(n)
    IF n <= 1
        return 1
    OTHER
        return fn n * factorial(n - 1)

* Call Function *
result = factorial(5)
greet($Bob$)
```

## 🎨 Strings

```ac
text = $Hello, World!$           * Basic string *
path = $C:\folder\file.txt$      * Escape sequences work *

* Escape Sequences *
$Line 1\nLine 2$                 * Newline *
$Tab\there$                      * Tab *
$Quote: \"text\"$                * Quote *
$Backslash: \\$                  * Backslash *
```

## 🏷️ Tags (Blocks)

```ac
<mainloop>                       * Main program *
    * code *
<mainloop>

<gui>                            * GUI definitions *
    * code *
<gui>

<StartHere>                      * Infinite loop *
    * code repeats *
<EndHere>

* Custom Tag *
def tag <setup>
    * initialization *

<setup>
    * use it *
<setup>
```

## 🌐 Foreign Code

```ac
<Foreign>
    # Raw Python code
    import numpy as np
    print(np.array([1,2,3]))
<Foreign>
```

## 🎮 Event Listeners

```ac
configure event-listener
    use listener to establish rule
        on value=space
            jump(player)
        on value=w
            moveUp(player)
        on value=s
            moveDown(player)
```

## 🎯 Objects

```ac
Obj.Player                       * Define object *
Player.config item=square(50)    * Configure *
Player.speed = 5                 * Set property *
Player.jump()                    * Call method *
```

## ⚠️ Error Handling

```ac
raise ERR($Invalid input$)       * Raise error *
```

## 📊 Common Patterns

### Hello World
```ac
AC->PY
<mainloop>
    Term.display $Hello, World!$
    /kill
<mainloop>
```

### Sum Numbers
```ac
AC->BNY
<mainloop>
    sum = 0
    i = 1
    WHILST i <= 100
        sum += i
        i += 1
    Term.display sum
    /kill
<mainloop>
```

### Fibonacci
```ac
AC->ASM
Make fib func(n)
    IF n <= 1
        return n
    OTHER
        return fib(n-1) + fib(n-2)

<mainloop>
    result = fib(10)
    Term.display result
    /kill
<mainloop>
```

### List Processing
```ac
AC->JS
<mainloop>
    numbers = [1, 2, 3, 4, 5]
    sum = 0
    FOR num in numbers
        sum += num
    Term.display sum
    /kill
<mainloop>
```

## 🔍 Tips

- Use `*` for comments
- Use `$` for strings
- Indentation matters (like Python)
- `fn` keyword for multiplication in expressions
- `@` operator for multiplication
- Case-insensitive: `true`/`True`/`TRUE` all work
- `null`/`Null`/`NULL` all work
- `/kill` exits the program
- `Term.display` prints to console
- Functions use `Make name func(args)`
- Conditions use `IF`/`ELSIF`/`OTHER`
- Loops use `WHILST` and `FOR`

## 🎯 Native Binary Tips

```bash
# Compile to binary
./ac mycode.ac BNY

# Run the binary
./mycode.acb

# Check binary info
file mycode.acb
ls -lh mycode.acb
```

**Binary features:**
- ~16KB size
- Native x86-64 code
- No interpreter needed
- Fast execution
- Standalone (only needs libc)

---

**AC Language - Write Once, Compile Anywhere** 🚀
