# Bi Language Specification

**Bi** (Latin for 2) is an easy-to-learn interpreted language implemented in AC. It demonstrates AC's capability for language implementation and VM creation.

## Syntax Overview

### Output
```bi
say($Hello World$)
```

### Variables & Assignment
```bi
x = 5
name = $Alice$
list = [1, 2, 3, 4, 5]
```

### Data Types
- **Numbers**: `5`, `3.14`, `-42`
- **Strings**: `$Hello$`
- **Lists**: `[1, 2, 3]`
- **Booleans**: `true`, `false`
- **Nil**: `nil` (null/none value)

### Control Flow
```bi
if x > 5 {
    say($x is big$)
} else {
    say($x is small$)
}

for i in range(1, 10) {
    say(i)
}

while x < 10 {
    x = x + 1
}
```

### Functions
```bi
function add(a, b) {
    return a + b
}

result = add(3, 4)
```

### Input
```bi
input_line = getline
if input_line == 5 {
    say($You entered 5$)
    halt
}
```

### Libraries
```bi
library random
library math

using math.pi
from ilib math use pi, sqrt
```

### Comments
```bi
# This is a comment
x = 5 # inline comment
```

## Built-in Functions

| Function | Description |
|----------|-------------|
| `say(value)` | Print value to stdout |
| `getline()` | Read a line from stdin |
| `length(list)` | Get list/string length |
| `range(start, end)` | Create range [start, end] |
| `range(n)` | Create range [0, n-1] |
| `halt` | Exit program |

## Operators

| Operator | Description |
|----------|-------------|
| `+` | Addition |
| `-` | Subtraction |
| `*` | Multiplication |
| `/` | Division |
| `==` | Equality |
| `!=` | Inequality |
| `<` | Less than |
| `>` | Greater than |
| `<=` | Less than or equal |
| `>=` | Greater than or equal |

## Example Programs

### Hello World
```bi
say($Hello World$)
```

### Calculator
```bi
function add(a, b) {
    return a + b
}

function subtract(a, b) {
    return a - b
}

x = 10
y = 5

say(add(x, y))
say(subtract(x, y))
```

### FizzBuzz
```bi
for i in range(1, 101) {
    if i % 15 == 0 {
        say($FizzBuzz$)
    } else if i % 3 == 0 {
        say($Fizz$)
    } else if i % 5 == 0 {
        say($Buzz$)
    } else {
        say(i)
    }
}
```

## Implementation

### Files
```
bi/
├── src/
│   ├── main.ac      # Entry point (generates Python runner)
│   ├── lexer.ac     # Lexical analyzer (tokenizes Bi source)
│   ├── parser.ac    # Parser (converts tokens to AST)
│   ├── bytecode.ac  # Bytecode format and VM
│   └── interpreter.ac  # Interpreter
├── test/
│   └── hello.bi     # Test program
└── docs/
    └── README.md    # This file
```

## Running Bi

```bash
# Using AC compiler
ac bi/src/main.ac bi/test/hello.bi
python bi/src/main.py bi/test/hello.bi

# Or use the binary wrapper
./bi/bin/bi bi/test/hello.bi
```

## Notes

- Bi is implemented in AC but generates Python code for execution
- The AC compiler is used to translate the Bi implementation to Python
- Bi itself is interpreted at runtime via the Python VM
- This demonstrates AC's capability for language implementation

## License

MIT - see [LICENSE](../LICENSE).