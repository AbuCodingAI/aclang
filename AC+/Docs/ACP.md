# AC+ — A Systems Language

AC+ is the low-level, hardware-facing member of the A language family. Where AC targets general-purpose runtimes and AI targets a virtual machine, AC+ targets **bare metal** — generating x86-64 assembly for kernels, bootloaders, and OS-level code.

---

## 1. Position in the A Ecosystem

| Property        | AC                          | AI                        | AC+                          |
|-----------------|-----------------------------|---------------------------|------------------------------|
| Purpose         | General-purpose             | Interpreted / exact math  | Systems / hardware           |
| Output          | 12 language targets         | `.aibc` → AIVM            | x86-64 assembly              |
| Level           | High                        | High                      | Low (register, memory)       |
| File extension  | `.ac`                       | `.ai`                     | `.acp`                       |
| Entry point     | implicit `main`             | implicit `main`           | `<mainloop>` label           |
| Nil             | —                           | —                         | `nil`                        |
| Target decl     | `AC->PY`, `AC->C`, etc.     | `AI->VM`                  | implicit (always ASM)        |
| Use case        | Apps, scripts, games        | Exact decimal computation | Kernels, bootloaders, AOS    |

---

## 2. Syntax Overview

### 2.1 File Header

A library file declares itself with:
```acp
AC+ LIB
```
An executable file has no header — it starts with the `<mainloop>` label.

### 2.2 Imports

```acp
use flib logic.acp      -- local file library
```

`flib` = file library — a **single `.acp` file** you reference by path, inlined directly at compile time (like a C++ `#include`). `clib`, `elib`, and `ilib` are all folder-based — the compiler looks them up in their respective folders (`clib/`, `elib/`, `ilib/`). `flib` is the only exception: it's always a single file and can be anywhere on disk, imported by path.

| Type   | Resolved from                           | What                                                      |
|--------|-----------------------------------------|-----------------------------------------------------------|
| `ilib` | `<ac-compiler>/library/<name>/`         | AC built-in / standard libraries                          |
| `elib` | `<ac-compiler>/library/elib/<name>/`    | Packages installed via **atar** (AC's package manager)    |
| `clib` | `<project>/clib/<name>/`               | Libraries you made yourself, as a subfolder of `clib/`    |
| `flib` | any path on disk                        | A single `.ac`/`.acp` file, imported by explicit path     |

**atar** — Spanish for "to tie/bind". Installs `.tar` package bundles into `<ac-compiler>/library/elib/`. Planned for AC 1.0. No `ac -m` required — just `atar install <package>`.

### 2.3 Entry Point

```acp
<mainloop>
    -- program body
<mainloop>
```

The `<label>` ... `<label>` syntax marks both the start and the looping re-entry of the main execution block. The compiled symbol is `__ai_main__`, which the bootloader skeleton jumps to after BIOS setup.

### 2.4 Register Variables

AC+ exposes hardware registers as typed named slots:

```acp
reg 64box1 = 0xB8000     -- 64-bit register, absolute VRAM address
reg 32vault = 0x07       -- 32-bit register, color attribute byte
```

Naming convention: `{bits}{name}` — `64box1` is a 64-bit general register named `box1`.

### 2.5 Memory Writes

The `<<` operator streams a value directly into a hardware memory address:

```acp
64box1 << char_code      -- write char byte to address in 64box1
64box1 + 1 << 32vault    -- write color byte to address + 1
```

This is the AC+ memory-write primitive — equivalent to a C pointer dereference write (`*ptr = val`).

### 2.6 Stack Allocation and Address-Of

```acp
stack memory_status byte     -- allocate a byte on the stack
reg 64box2 = locate memory_status  -- get its address (like & in C)
```

`stack varname type` allocates `varname` on the current stack frame.  
`locate varname` returns its address.

### 2.7 Nil Check and Hardware Halt

```acp
IF 64box2 is nil
    /kill               -- unconditional hardware halt (HLT instruction)
OTHER
    -- normal path
```

`/kill` is AC+'s hard stop — maps to the x86 `hlt` instruction. Used when the system is in an unrecoverable state.

### 2.8 Functions

```acp
AC+ LIB
Make boot_print func(char_code, offset)
    reg 64box1 = 0xB8000
    64box1 += offset
    64box1 << char_code
    reg 32vault = 0x07
    64box1 + 1 << 32vault
    return nil
```

`Make funcname func(params)` defines a function. `return nil` is the void return.  
Functions are called with standard call syntax: `boot_print(76, 0)`.

### 2.9 Control Flow

Shares keywords with AC where it makes sense:

```acp
IF condition
    -- true branch
OTHER
    -- false branch

WHILST condition
    -- loop body
```

Conditions use `is nil` for null-check and `#>` for "not greater than" (or similar relational operators).

### 2.10 Arithmetic

Standard compound assignment works on registers:

```acp
64box1 += offset     -- pointer arithmetic
status = 1
```

Plain variable assignment (without `reg`) works like AC — inferred type, no declaration keyword needed.

---

## 3. Bootloader Integration

AC+ programs intended for bare-metal boot use a companion `skeleton.s` — a 16-bit real-mode entry stub:

```asm
.code16
_start:
    xor %ax, %ax          -- clear segments
    mov %ax, %ds
    mov %ax, %es
    mov $0x7C00, %sp      -- set up stack at boot location
    mov %sp, %bp

    -- BIOS character output (prints "AOS ")
    mov $0x0E, %ah
    mov $'A', %al; int $0x10
    -- ...

    jmp __ai_main__       -- hand control to compiled AC+ mainloop
```

The `skeleton.s` handles the BIOS/MBR setup and then jumps to `__ai_main__`, the symbol that the AC+ compiler emits for `<mainloop>`. The two files are assembled and linked together into a bootable image.

---

## 4. Memory Model

AC+ works directly with physical addresses. The canonical example is VGA text mode:

- Base address: `0xB8000`
- Each character: 2 bytes — `[char_byte][color_byte]`
- `boot_print(char_code, offset)` writes a character at `0xB8000 + offset`

The color attribute `0x07` = light gray on black (standard VGA default).

---

## 5. AOS — The AC+ Operating System

The files in `AC+/` are the kernel seed for **AOS** (Abu Operating System):

- `logic.acp` — hardware logic layer (VRAM printing, low-level I/O)
- `kernel.acp` — boot sequence: memory check → print "LOAD" to screen → idle loop
- `skeleton.s` — real-mode BIOS stub, assembles to MBR boot sector

The boot sequence:
1. BIOS loads MBR, jumps to `skeleton.s`
2. `skeleton.s` prints "AOS " via BIOS, sets up stack, jumps to `__ai_main__`
3. `kernel.acp` checks memory status, writes `L O A D` to VRAM at offsets 0, 2, 4, 6
4. Enters infinite loop (`WHILST status #> 5`) to keep display alive

---

## 6. Differences from AC

AC+ shares most of AC's surface syntax. The differences are only where hardware access demands it.

**Shared with AC:** `Make name func(params)`, `<mainloop>` entry point, `/kill` halt, `IF`/`ELIF`/`OTHER`, `WHILST`, `FOR`, compound assignment (`+=`, `-=`, etc.), plain variable assignment.

**What AC+ adds or changes:**

| Feature          | AC                                  | AC+                                      |
|------------------|-------------------------------------|------------------------------------------|
| Hardware vars    | Plain inferred vars only            | `reg 64box` (register) + `stack varname type` |
| Memory write     | None                                | `<<` — stream value into memory address  |
| Address-of       | None                                | `locate varname` — returns stack address |
| Imports          | `use ilib name` (named library)     | `use flib file.acp` (direct file include)|
| Library header   | Target keyword (`AC->PY`, etc.)     | `AC+ LIB`                               |
| Output target    | Multiple backends (PY, C, Go, ...)  | x86-64 assembly only — no target keyword |
| Nil              | Used in conditions                  | `nil` — explicit hardware null/zero      |
| Purpose          | Application programming             | OS / firmware / kernel                   |


### Table of registers

| Real x86-64 Name | AC+ 64-Bit Alias | Real 32-Bit Sub-Register | AC+ 32-Bit Alias | Architectural System Role / Purpose |
| :--- | :--- | :--- | :--- | :--- |
| **`%rdi`** | `64box1`    | `%edi`  | `32box1`      | 1st Function Parameter / Main Video Data Stream        |
| **`%rsi`** | `64box2`    | `%esi`  | `32box2`      | 2nd Function Parameter / Secondary Stream              |
| **`%rdx`** | `64box3`    | `%edx`  | `32box3`      | 3rd Function Parameter / Register Allocator Pool       |
| **`%rcx`** | `64box4`    | `%ecx`  | `32box4`      | 4th Function Parameter / Register Allocator Pool       |
| **`%r8`**  | `64box5`    | `%r8d`  | `32box5`      | 5th Function Parameter / Register Allocator Pool       |
| **`%r9`**  | `64box6`    | `%r9d`  | `32box6`      | 6th Function Parameter / Register Allocator Pool       |
| **`%rax`** | `64vault`     | `%eax`  | `32vault`     | Math Accumulator / Function Return Value Slot          |
| **`%rbx`** | `64base`      | `%ebx`  | `32base`      | Base Pointer / General Purpose Callee-Saved Scratchpad              |
| **`%rsp`** | `64stack_pin` | `%esp`  | `32stack_pin` | Stack Pointer / Tracks current place on stack frame    |
| **`%rbp`** | `64frame_pin` | `%ebp`  | `32frame_pin` | Frame Pointer / Reference base for local RAM variables |
| **`%r10`** | `64scratch1`  | `%r10d` | `32scratch1`  | Temporary Caller-Saved Scratchpad                      |
| **`%r11`** | `64scratch2`  | `%r11d` | `32scratch2`  | Temporary Caller-Saved Scratchpad                      |
| **`%r12`** | `64saved1`    | `%r12d` | `32saved1`    | Callee-Saved Permanent Variable Slot                   |
| **`%r13`** | `64saved2`    | `%r13d` | `32saved2`    | Callee-Saved Permanent Variable Slot                   |
| **`%r14`** | `64saved3`    | `%r14d` | `32saved3`    | Callee-Saved Permanent Variable Slot                   |
| **`%r15`** | `64saved4`    | `%r15d` | `32saved4`    | Callee-Saved Permanent Variable Slot                   |
