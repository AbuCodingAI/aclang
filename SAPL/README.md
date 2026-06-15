# SAPL Interpreter - C++ Implementation

A C++ interpreter for SAPL (Standard APL), an APL-inspired, ASCII-friendly array-oriented language.

## Building

```bash
cd SAPL
make
```

## Usage

### Run a SAPL file
```bash
./sapl program.sapl
```

### Interactive REPL
```bash
./sapl -r
```

### Evaluate an expression
```bash
./sapl -e "0<- E I 1 10"
```

## Language Features

### Core Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `E` | Sum reduction | `E [1 2 3]` → 6 |
| `PI` | Product reduction | `PI [1 2 3 4 5]` → 120 |
| `I` | Interval/Range | `I 1 5` → [1 2 3 4 5] |
| `rev` | Reverse array | `rev [1 2 3]` → [3 2 1] |
| `T` | Transpose | `T matrix` |
| `pw` | Shape/Length | `pw [1 2 3]` → 3 |
| `asc` | Ascending grade | `asc [3 1 2]` → [1 2 0] |
| `desc` | Descending grade | `desc [3 1 2]` → [0 2 1] |
| `cmp` | Three-way compare | `5 cmp 3` → 1, `3 cmp 5` → -1, `3 cmp 3` → 0 |

### Arithmetic Operators

- `+` Addition
- `-` Subtraction
- `*` Multiplication
- `/` Division
- `%` Modulo
- `^` Power

### Comparison Operators

- `<` Less than
- `>` Greater than
- `<=` Less than or equal
- `>=` Greater than or equal
- `cmp` Three-way comparison (returns -1, 0, or 1)

### Output

Use `0<-` to output values:
```sapl
0<- E I 1 10
```

### Functions

Define functions with the syntax `name(args) = expression`:
```sapl
factorial(x) = PI I 1 x
mean(data) = E data / pw data
```

### Conditionals

Use `condition : true_branch, false_branch` syntax:
```sapl
abs(x) = {
    x < 0: 0 - x,
    x
}
```

### Arrays

Create arrays with brackets:
```sapl
arr = [1 2 3 4 5]
```

### Comments

Use `;` for single-line comments:
```sapl
; This is a comment
0<- E I 1 10  ; Output sum of 1 to 10
```

## Examples

### Factorial
```sapl
factorial(x) = PI I 1 x
0<- factorial(5)
```

### Sum of Range
```sapl
0<- E I 1 100
```

### Mean
```sapl
mean(data) = E data / pw data
data = [10 20 30 40 50]
0<- mean(data)
```

### Fibonacci
```sapl
fibonacci(n) = {
    n < 2: n,
    fibonacci(n - 1) + fibonacci(n - 2)
}
0<- fibonacci(10)
```

## Architecture

The interpreter consists of:
- **Lexer** (`Lexer` class): Tokenizes SAPL source code
- **Parser** (`Parser` class): Builds an Abstract Syntax Tree (AST)
- **Interpreter** (`Interpreter` class): Evaluates the AST
- **Value** (`Value` class): Represents runtime values (numbers, arrays, strings)
- **Environment** (`Environment` class): Manages variable and function scopes

## License

Part of the AC (AbuCoding) project.