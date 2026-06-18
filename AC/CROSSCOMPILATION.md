# AC->BNY: Universal Binary Compilation

## The Vision: One Command, All Platforms

```
$ ac program.ac
```

**Result: Native binary for your platform**
- Linux x86 → ELF64 binary
- Windows x86 → PE binary
- macOS x86 → Mach-O binary
- ARM (any OS) → Native ARM binary

All from the **same** `ac program.ac` command!

---

## Compilation Strategy

### **x86 Platforms: ASM→Binary (Transparent)**

```
AC Source Code
    ↓
   IR (Intermediate Representation)
    ↓
 x86-64 Assembly
    ↓
[Platform Assembler]
    ├─ Linux: as → ELF64 binary
    ├─ Windows: ml64 → PE binary
    └─ macOS: as → Mach-O binary
    ↓
Delete .s file
    ↓
Binary ✓
```

**Process:**
1. AC → IR (always)
2. IR → x86-64 ASM (intermediate)
3. Platform assembler compiles .s to binary
4. Delete .s file automatically
5. Return native binary

**Example:**
```bash
$ ac myapp.ac
[AC->BNY] Detected non-Linux x86 - compiling ASM to binary
[AC->BNY Assemble] as myapp.s -o temp.o && ld -o myapp temp.o
[AC->BNY] Deleted intermediary: myapp.s
[AC->BNY] Binary ready: myapp
```

---

### **ARM Platforms: C→Binary (Transparent)**

```
AC Source Code
    ↓
   IR (Intermediate Representation)
    ↓
  C Code
    ↓
[GCC/Clang]
    ├─ macOS ARM: clang → ARM binary
    ├─ Linux ARM: gcc → ARM binary
    └─ Windows ARM: gcc → ARM binary
    ↓
Delete .c file
    ↓
Binary ✓
```

**Process:**
1. AC → IR (always)
2. IR → C code (intermediate)
3. System C compiler (GCC/Clang) compiles .c to binary
4. Delete .c file automatically
5. Return native binary

**Example:**
```bash
$ ac myapp.ac  # On Apple Silicon Mac
[AC->BNY] Detected ARM - generating C intermediary
[AC->BNY Cross-compile] clang -O2 -o myapp myapp.c
[AC->BNY] Deleted intermediary: myapp.c
[AC->BNY] Binary ready: myapp
```

---

## Platform Detection & Compilation

### **Automatic Platform Detection**

```cpp
detectCPU()   // x86_64, ARM64, ARM32, UNKNOWN
detectOS()    // WINDOWS, MACOS, LINUX, UNKNOWN
```

### **Compiler Selection**

| Platform | CPU | ASM Compiler | C Compiler |
|----------|-----|--------------|-----------|
| Linux x86 | x86_64 | as | gcc |
| Windows x86 | x86_64 | ml64 | gcc |
| macOS Intel | x86_64 | as | clang |
| macOS ARM | ARM64 | (C path) | clang |
| Linux ARM | ARM32/64 | (C path) | gcc |
| Windows ARM | ARM64 | (C path) | gcc |

---

## Key Properties

✅ **Transparent**: User never sees intermediaries (deleted automatically)
✅ **Native**: Each platform gets optimized native code
✅ **Portable**: Same AC code works everywhere
✅ **Simple**: Single `ac program.ac` command
✅ **Fast**: Direct native compilation on x86, C compilation on ARM

---

## Future: User Control

```bash
# Default: Delete intermediaries
ac program.ac → program (binary only)

# Keep assembly (for inspection)
ac --save-asm program.ac → program + program.s

# Keep C code (for inspection)  
ac --save-c program.ac → program + program.c

# Explicit target
ac --target arm64 program.ac → ARM64 binary
ac --target x86-64 program.ac → x86 binary
```

---

## Current Implementation Status

| Component | Status |
|-----------|--------|
| Platform detection | ✅ Complete |
| Linux x86→ELF | ✅ Complete |
| x86 ASM generation | ✅ Complete |
| ASM→Binary compilation | ⏳ In progress |
| ARM IR→C translation | ⏳ In progress |
| C→Binary compilation | ✅ Infrastructure ready |
| Intermediary deletion | ✅ Implemented |

---

## The AC->BNY Promise

> **AC->BNY: One language, one compilation command, every platform.**
> 
> Native performance on x86. Native ARM support via C. All transparently.
