# IoT — Incipiency of Truth(Not internet of things)

IoT is the native low-level truth layer for the UPU.
It follows RISC thinking: long, explicit, minimal, and honest.

IoT does not hide work behind abstractions. If AC says `range`, IoT expands it into real steps.

## Design Rule

Every instruction must be one of:

1. **Essential** — required primitive operation.
2. **Efficiency** — derivable, but too important or expensive to emulate.
3. **Derived** — not hardware-level; built from other instructions.

## Naming Rule

Names must say what they actually do.

`COPY` copies.
`MOVE` moves.
`JMP` jumps.

## Core Instructions

```iot
COPY target source
```

Copies `source` into `target`. Source remains unchanged.

```iot
MOVE target source
```

Moves `source` into `target`. Source is cleared, emptied, or invalidated.

```iot
MEM left right
```

Loads or prepares values for comparison or active memory context.

```iot
ADD target value
SUB target value
MUL target value
DIV target value
```

Arithmetic operations.

```iot
AND target value
OR target value
YET target value
NOT target
```

Logic operations.

`YET` means XOR: one side or the other, but not both.

## Jump Syntax

```iot
JMP label
```

Always jumps.

```iot
JMP cond left < label
JMP cond left > label
JMP cond left = label
```

Conditional jumps.

Example:

```iot
MEM rax rbx
JMP cond rax < less_label
JMP cond rax = equal_label
JMP cond rax > greater_label
```

Derived comparisons should be expressed through primitives:

```iot
a <= b  =>  a < b OR a = b
a >= b  =>  a > b OR a = b
a != b  =>  NOT a = b
XNOR    =>  NOT YET
```

## Example Loop

```iot
COPY counter 0
COPY limit 10

loop:
    MEM counter limit
    JMP cond counter > end

    ADD counter 1
    JMP loop

end:
```

## Philosophy

IoT is not meant to be pleasant to write by hand.

It is meant to be truthful.

High-level AC-family languages may be elegant, short, and expressive.
IoT is the place where those abstractions are expanded into exact machine actions.
