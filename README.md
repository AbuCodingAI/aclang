# AC Language Compiler

**A beginner-friendly compiled language that targets 11+ backends, including native x86-64 binaries.**

AC is a clean, readable programming language that compiles to multiple targets - from Python scripts to native machine code. Write once, compile anywhere.

---

## 🚀 Quick Start

```bash
# Build the compiler first
cd ac-compiler && make

# Compile and run (target auto-detected from file)
./ac-compiler/ac mycode.ac

# Compile to specific target
./ac-compiler/ac mycode.ac PY      # Python
./ac-compiler/ac mycode.ac BNY     # Native binary
./ac-compiler/ac mycode.ac ASM     # x86-64 assembly

# Use the REPL
python3 ac-compiler/ac_repl.py
```

---

## 🎯 Supported Backends

| Target | Declaration | Output | Status |
|--------|-------------|--------|--------|
| **Native Binary** | `AC->BNY` | `.acb` executable | ✅ **NEW!** |
| **Assembly** | `AC->ASM` | `.s` x86-64 assembly | ✅ Complete |
| **Python** | `AC->PY` | `.py` script | ✅ Complete |
| **JavaScript** | `AC->JS` | `.js` script | ✅ Complete |
| **C** | `AC->C` | `.c` source | ✅ Complete |
| **C++** | `AC->C++` or `AC->CPP` | `.cpp` source | ✅ Complete |
| **Rust** | `AC->RS` | `.rs` source | ✅ Complete |
| **Go** | `AC->GO` | `.go` source | ✅ Complete |
| **Java** | `AC->Java` | `.java` source | ✅ Complete |
| **HTML** | `AC->HTML` | `.html` interactive | ✅ Complete |
| **V** | `AC->V` | `.v` source | ⚠️ Needs V compiler |

### 🌟 Native Binary Compilation (NEW!)

AC now compiles directly to **native x86-64 executables**:

```ac
AC->BNY

<mainloop>
    Term.display $Hello from native code!$
    /kill
<mainloop>
```

```bash
./ac hello.ac BNY
./hello.acb          # Run the native binary!
```

**Features:**
- True machine code (no interpreter)
- ~16KB standalone executables
- Fast execution
- No dependencies (except libc)

---

## 📖 Language Features

### ✅ Complete Feature Set

- **Functions** with recursion and multiple parameters
- **Control Flow**: IF/ELSIF/OTHER, WHILST loops, FOR loops
- **Data Types**: Integers, floats, strings, booleans, null
- **Collections**: Lists, tuples, dictionaries
- **Operators**: Arithmetic (+, -, *, /, %), comparison, logical
- **Special Operators**: 
  - `@` for multiplication (fn keyword)
  - Compound assignments (+=, -=, *=, @=, /=)
- **String Literals**: `$text$` syntax with escape sequences
- **Case-insensitive**: true/True/TRUE, null/Null/NULL
- **Event System**: GUI event listeners and key bindings
- **Widget System**: Built-in UI components
- **Foreign Blocks**: Embed raw target language code

---

## 📝 Syntax Examples

### Hello World
```ac
AC->PY

<mainloop>
    Term.display $Hello, World!$
    /kill
<mainloop>
```

### Functions and Recursion
```ac
AC->BNY

Make fibonacci func(n)
    IF n <= 1
        return n
    OTHER
        return fibonacci(n - 1) + fibonacci(n - 2)

<mainloop>
    result = fibonacci(10)
    Term.display result
    /kill
<mainloop>
```

### Dictionaries
```ac
AC->PY

<mainloop>
    person = {
        name: $Alice$
        age: 30
        city: $NYC$
    }
    Term.display person
    /kill
<mainloop>
```

### Loops
```ac
AC->JS

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

### Lists and Iteration
```ac
AC->C

<mainloop>
    numbers = [1, 2, 3, 4, 5]
    FOR num in numbers
        Term.display num
    /kill
<mainloop>
```

---

## 🔧 Building the Compiler

### Prerequisites
```bash
sudo apt install -y g++ make
```

### Compile
```bash
cd ac-compiler
make
```

### Install (optional)
```bash
sudo make install
```

### Run REPL
```bash
python3 ac-compiler/ac_repl.py
```

---

## 📚 Detailed Syntax Reference

### Comments
```ac
* This is a comment *
```

### Backend Declaration
Must be at the top of your file:
```ac
AC->PY      * Compile to Python *
AC->BNY     * Compile to native binary *
AC->ASM     * Compile to assembly *
```

### Variables
```ac
name = $Alice$
age = 30
active = true
data = null
```

### Operators

| Operator | Meaning |
|----------|---------|
| `=` | Assignment |
| `is` or `==` | Equality |
| `#=` or `!=` | Not equal |
| `<`, `>`, `<=`, `>=` | Comparison |
| `+`, `-`, `*`, `/`, `%` | Arithmetic |
| `@` | Multiplication (fn keyword) |
| `+=`, `-=`, `*=`, `@=`, `/=` | Compound assignment |

### Conditionals
```ac
IF x > 10
    Term.display $x is large$
ELSIF x > 5
    Term.display $x is medium$
OTHER
    Term.display $x is small$
```

### Functions
```ac
Make greet func(name)
    Term.display $Hello, $
    Term.display name
    return 0

Make add func(a, b)
    return a + b
```

### Loops
```ac
* While loop *
WHILST condition
    * code *

* For loop *
FOR item in list
    * code *
```

### Lists and Tuples
```ac
mylist = [1, 2, 3, 4, 5]
mytuple = (1, 2, 3)
```

### Dictionaries
```ac
person = {
    name: $Alice$
    age: 30
    city: $NYC$
}
```

### Tags (Blocks)
```ac
<mainloop>
    * Main program code *
<mainloop>

<gui>
    * GUI definitions *
<gui>

<StartHere>
    * This loops forever *
<EndHere>
```

### Custom Tags
```ac
def tag <setup>
    * initialization code *

<setup>
    * use the tag *
<setup>
```

### Foreign Blocks
Embed raw target language code:
```ac
<Foreign>
    # This is raw Python code
    import numpy as np
    arr = np.array([1, 2, 3])
    print(arr.mean())
<Foreign>
```

### Event Listeners
```ac
configure event-listener
    use listener to establish rule
        on value=space
            jump(player)
        on value=w
            moveUp(player)
```

### Objects and Methods
```ac
Obj.Player
Player.config item=square(50)
Player.speed = 5
Player.jump()
```

### Error Handling
```ac
raise ERR($Invalid input$)
```

### Program Control
```ac
/kill    * Exit program *
```

---

## 🎯 Benchmark Results

All backends pass comprehensive tests:

```
=== AC Language Benchmark ===
Computing Fibonacci(15)...     610 ✓
Computing Factorial(10)...     3628800 ✓
Computing Sum(1 to 1000)...    500500 ✓
Computing 2^20...              1048576 ✓
=== Benchmark Complete ===
```

**Tested on:** Python, JavaScript, C, C++, Rust, Go, ASM, BNY, HTML

---

## 📦 File Extensions

- `.ac` - AC source files
- `.acc` - Compiled cache files
- `.acb` - Native binary executables (BNY backend)

---

## 🏗️ Architecture

```
AC Source (.ac)
    ↓
Lexer (tokens)
    ↓
Parser (AST)
    ↓
Backend Selection
    ↓
Code Generator
    ↓
Target Output (.py, .js, .c, .acb, etc.)
```

**Key Components:**
- **Lexer**: Tokenizes source code
- **Parser**: Builds Abstract Syntax Tree (AST)
- **AST**: Intermediate representation
- **IR**: Optional optimization layer (in development)
- **Backends**: 11 code generators
- **Cache**: Fast recompilation with `.acc` files

---

## 🎓 Advanced Features

### Caching System
The compiler caches parsed AST to `.acc` files for faster recompilation:
```bash
./ac mycode.ac        # First run: parses and caches
./ac mycode.ac        # Subsequent runs: uses cache
./ac mycode.ac -f     # Force recompile
```

### Compile-Only Mode
```bash
./ac mycode.ac -c     # Compile but don't run
```

### Backend Override
```bash
./ac mycode.ac PY     # Override to Python
./ac mycode.ac BNY    # Override to native binary
```

---

## 🔬 Technical Details

### Native Binary (BNY) Backend
- Generates x86-64 assembly via ASM backend
- Assembles and links with gcc
- Produces ELF executables
- ~16KB for simple programs
- Dynamically linked with libc

### Assembly (ASM) Backend
- Full x86-64 instruction set
- Proper calling conventions (System V ABI)
- Stack frame management
- Register allocation
- Function calls and recursion

### IR System (Optional)
- Intermediate Representation for optimizations
- SSA-style with typed references
- Currently in development
- Direct AST→Code generation used in production

---

## 🤝 Contributing

The AC compiler is open for contributions! Areas of interest:
- New backend targets
- Optimization passes
- Standard library expansion
- Error message improvements
- Documentation

---

## 📄 License

[Your License Here]

---

## 🎉 Acknowledgments

Built with passion for clean syntax and multi-target compilation.

**Special Features:**
- Native binary compilation
- 11 backend targets
- Clean, readable syntax
- Fast compilation
- Comprehensive language features

---

## 📞 Support

For issues, questions, or contributions, please [open an issue](https://github.com/AbuCodingAI/aclang/issues).

---

**AC Language - Write Once, Compile Anywhere** 🚀

