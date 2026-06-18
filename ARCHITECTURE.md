# AC Language: Dual-Mode Architecture

## The Vision: Pure Native + Pure Transpiler

AC is engineered with a **dual-mode strategy** that maximizes both performance and portability:

### 🚀 **Pure Native Powerhouse (Linux x86-64)**

**AC → BNY Backend → x86-64 Assembly → ELF64 Binary**

- Generates **native machine code directly**
- No intermediate transpilation layer
- Maximum performance
- Direct syscall integration
- Optimized for server workloads on Linux

**Targets:**
- Ubuntu, Fedora, Arch Linux, Mint, Bazzite
- Any Linux x86-64 system with libc

### ⚡ **Pure Transpiler (ARM Architecture)**

**AC → C Code → GCC/Clang → Native Binary**

- Intelligently transpiles to C when targeting ARM
- Leverages existing C compiler ecosystem
- Works on any platform with GCC or Clang
- Portable: compile once AC code, run anywhere

**Usage:**
```ac
AC->BNY   // On x86 Linux: generates native ELF
AC->BNY   // On ARM: auto-transpiles to C, uses GCC/Clang

AC->C     // Explicit: always use C transpiler
```

**Targets:**
- macOS ARM (Apple Silicon) - uses Clang
- Windows ARM - uses GCC
- Linux ARM - uses GCC
- Any architecture with C compiler support

### 📊 Architecture Decision Tree

```
User writes: AC->BNY or AC->ASM
         ↓
Is target x86-64 Linux?
   ├─ YES → Native compilation (ELF64 direct)
   └─ NO → Auto-transpile to C + system C compiler
             (macOS/Win/Linux on ARM)
```

## Why This Design?

1. **Performance where it matters**: Native code on Linux servers
2. **Portability where needed**: C transpilation on ARM
3. **Single source**: User writes same AC code everywhere
4. **Compiler intelligence**: Platform detection automatic
5. **No user complexity**: "Genius protocol" is transparent

## Current Status

- ✅ **Pure Native** (Linux x86): BNY backend complete
- ⚠️ **Pure Transpiler** (ARM): C backend + AC→C transpiler (in progress)
- ✅ **Multi-backend**: Python, JavaScript, C
- ✅ **Operator coverage**: Logical, bitwise, comparison
- ✅ **I/O system**: Term.display, Term.ask with proper types
- ✅ **Memory safety**: Pointers + References framework

## The AC Promise

> AC is a **pure native powerhouse on Linux** and a **pure transpiler on ARM** — 
> one language, infinite platforms, maximum efficiency where it counts.
