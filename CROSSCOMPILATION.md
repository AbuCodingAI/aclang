# AC Cross-Compilation Strategy

## Architecture: Universal IR with Platform-Specific Backends

```
AC Source Code
     ↓
   AC IR (Unified Intermediate Representation)
     ↓
     ├─ x86 Path: IR → x86 ASM → Binary (ELF/PE/Mach-O)
     │  Delete: .s assembly file
     │
     └─ ARM Path: IR → C Code → Binary (via GCC/Clang)
        Delete: .c source file
```

## Compilation Flows

### **x86 Platforms (Native Assembly)**

**All x86 compiles to native ASM then to binary:**

```
Windows x86:  AC → IR → x86 ASM → [ELF write] → PE binary ✓ (delete .s)
macOS x86:    AC → IR → x86 ASM → [Mach-O write] → Binary ✓ (delete .s)
Linux x86:    AC → IR → x86 ASM → [ELF64 write] → Binary ✓ (delete .s)
```

**Advantages:**
- Direct native code generation
- Maximum performance
- No intermediate compilation step
- Assembly file deleted after binary creation

### **ARM Platforms (C Intermediary)**

**All ARM compiles to C, then to binary:**

```
Windows ARM:  AC → IR → C Code → [GCC] → Binary ✓ (delete .c)
macOS ARM:    AC → IR → C Code → [Clang] → Binary ✓ (delete .c)
Linux ARM:    AC → IR → C Code → [GCC] → Binary ✓ (delete .c)
```

**Advantages:**
- Portable to any platform with C compiler
- Leverages optimized GCC/Clang toolchains
- Works on all ARM architectures (ARMv7, ARMv8, etc.)
- C intermediary automatically deleted

## Implementation Details

### Platform Detection
```cpp
detectCPU()  // Returns: X86_64, ARM64, ARM32, UNKNOWN
detectOS()   // Returns: WINDOWS, MACOS, LINUX, UNKNOWN
needsCrossCompilation()  // Returns true if ARM
```

### Compiler Selection
```
macOS → Clang
Linux/Windows → GCC
```

### Intermediary File Handling
```
x86:  Generate .s (assembly)
      ↓
      Compile to binary
      ↓
      Delete .s
      
ARM:  Generate .c (C source)
      ↓
      Compile with GCC/Clang to binary
      ↓
      Delete .c
```

## Future: User Options

```bash
# Default: delete intermediary
ac program.ac  → program

# Keep assembly source (for inspection)
ac --save-asm program.ac  → program, program.s

# Keep C source (for inspection)
ac --save-c program.ac  → program, program.c

# Explicit target
ac --target arm64 program.ac  → program (ARM binary)
ac --target x86-64 program.ac  → program (x86 binary)
```

## Current Status

✅ **Platform detection implemented**
⏳ **x86 ASM generation to binary (delete .s)**
⏳ **ARM IR→C translation**
⏳ **GCC/Clang invocation for ARM**
⏳ **User flags for intermediary control**

## Key Principle

> **One IR, infinite platforms:**
> AC's unified IR representation compiles to native code on x86,
> and intelligently falls back to C for ARM—all transparently.
