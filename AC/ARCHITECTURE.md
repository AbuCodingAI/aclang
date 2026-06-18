# AC Language: Dual-Mode Architecture

## The Vision: Universal Native Performance

AC uses an **intelligent dual-mode strategy** that provides native performance everywhere:

### 🚀 **Pure Native (x86 Architecture)**

**All x86 platforms use direct x86-64 assembly generation:**

```
AC Code → x86-64 Assembly → Binary
  ↓
  ├─ Windows x86    → PE executable
  ├─ macOS Intel    → Mach-O executable  
  └─ Linux x86      → ELF64 executable
```

**Benefits:**
- No intermediate transpilation
- Maximum performance
- Direct syscall/API integration
- Optimized for each platform

### ⚡ **Pure Transpiler (ARM Architecture)**

**All ARM platforms intelligently fall back to C:**

```
AC Code → C Code → Native ARM Binary
  ↓
  ├─ Windows ARM    → GCC → Binary
  ├─ macOS ARM      → Clang → Binary
  └─ Linux ARM      → GCC → Binary
```

**Benefits:**
- Leverages existing C compiler ecosystem
- Works on any platform with GCC/Clang
- Near-native performance
- Portable source code

---

## **Platform Matrix**

| Platform | Architecture | Compilation | Output |
|----------|--------------|-------------|--------|
| Windows | x86-64 | Direct ASM | PE Binary |
| Windows | ARM | AC→C→GCC | ARM Binary |
| macOS | x86-64 (Intel) | Direct ASM | Mach-O Binary |
| macOS | ARM (Apple Silicon) | AC→C→Clang | ARM Binary |
| Linux | x86-64 | Direct ASM (BNY) | ELF64 Binary |
| Linux | ARM | AC→C→GCC | ARM Binary |

---

## **How It Works**

```ac
AC->BNY   // User writes same code everywhere

// Compiler automatically detects target:

Windows x86-64?
  ✅ Direct x86 ASM → PE format

Windows ARM?
  ⏳ Transpile to C → GCC → ARM binary

macOS Intel?
  ✅ Direct x86 ASM → Mach-O format

macOS ARM (M1/M2/M3)?
  ⏳ Transpile to C → Clang → ARM binary

Linux x86-64?
  ✅ Direct x86 ASM → ELF64 format (BNY backend)

Linux ARM?
  ⏳ Transpile to C → GCC → ARM binary
```

---

## **The AC Promise**

> AC is a **pure native powerhouse on x86** across all platforms 
> and a **pure transpiler on ARM** for maximum portability.

- **x86**: Maximum native performance everywhere
- **ARM**: Universal portability via C
- **Single Source**: One AC program, infinite targets
- **Automatic**: Compiler handles platform detection

---

## **Current Implementation Status**

| Component | Status | Details |
|-----------|--------|---------|
| **x86 Native** | ✅ Complete | PE (Windows), Mach-O (macOS), ELF64 (Linux) |
| **BNY Backend** | ✅ Complete | Linux x86-64 ELF64 native generation |
| **ARM Transpiler** | ⏳ In Progress | AC→C→GCC/Clang pipeline |
| **Platform Detection** | ⏳ In Progress | Automatic architecture selection |
| **Multi-backend** | ✅ Complete | Python, JavaScript, C backends |
| **Operators** | ✅ Complete | Logical, bitwise, comparison |
| **I/O System** | ✅ Complete | Term.display, Term.ask with proper types |
| **Memory Safety** | ✅ In Progress | Pointers + References framework (Phase 3) |

---

## **Architecture Philosophy**

AC rejects the false choice between performance and portability:

- **Not** "pick one platform and optimize"
- **Not** "transpile everything to be portable"
- **Instead**: Native where it matters (x86), portable where it's needed (ARM)

This makes AC ideal for:
- 🖥️ **Servers** (x86 Linux powerhouse)
- 📱 **Cross-platform tools** (ARM portable via C)
- 🚀 **Performance-critical applications** (native on x86)
- 🌍 **Distributed systems** (one source, many targets)
