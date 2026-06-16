# Bi Language Specification

**Bi** (Latin for 2) is an easy-to-learn interpreted language written in AC. It's a successor to A syntax, proving that AC can implement full language interpreters.

## Core Features

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
- **Ranges**: `range(1, 10)` → `[1, 2, 3, ..., 10]`

### Control Flow
```bi
if x > 5 {
    say($x is big$)
}

for i in range(1, 10) {
    say(i)
}
```

### Input
```bi
input_line = getline  # reads a line from stdin
if getline is 5 {
    say($You entered 5$)
    halt
}
```

### Functions (Built-in Library)
- `say(value)` - output
- `getline()` - read input line
- `length(list)` - list/string length
- `range(start, end)` - create range
- `halt` - exit program

### Libraries
```bi
library random
library math

use ilib math as m      # alias library
using math.pi           # use specific item
from ilib math use pi   # import only pi

say(m.pi)
say(random.choice([1,2,3,4,5]))
```

### Comments
```bi
# This is a comment
x = 5 # inline comment
```

## Example Program
```bi
say($Welcome to Bi$)
library random

numbers = [1, 2, 3, 4, 5, 6, 7, 8, 9, 0]
for i in range(length(numbers)) {
    say(random.choice(numbers))
}

if getline is 5 {
    say($Goodbye$)
    halt
}
```

## Implementation Notes

- Bi is interpreted (runtime evaluation)
- No compilation step
- Written entirely in AC
- Demonstrates AC's capability for language implementation
- Forward-compatible with future AC features
