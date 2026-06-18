# AC Language: 64-Bit Only Architecture

## The Case for 64-Bit Only

AC should be **64-bit only** for modern software development. Here's why:

---

## Market Reality

### 32-Bit is Dead (Outside Legacy Systems)

| Market | 32-bit Status | Notes |
|--------|---------------|-------|
| **Desktop/Laptop** | ❌ Dead (2010+) | Last Windows 32-bit: XP/Vista |
| **Mobile (Android)** | ❌ Dead (2019+) | Google Play requires 64-bit |
| **Mobile (iOS)** | ❌ Dead (2015+) | App Store dropped 32-bit in iOS 11 |
| **Cloud/Servers** | ❌ Dead (2008+) | All production servers are 64-bit |
| **ARM Devices** | ❌ Dead (2020+) | Modern Raspberry Pi: 64-bit |
| **Gaming** | ❌ Dead (2013+) | PS5/Xbox Series X: 64-bit only |

### Only Legacy Systems Use 32-Bit

- **Banks** (COBOL + legacy mainframes)
- **ATMs** (Windows XP embedded)
- **Industrial control** (Older PLCs)
- **Old embedded systems** (Pre-2010 firmware)

---

## Why AC Doesn't Need 32-Bit Support

### ✅ **Compelling Reasons for 64-Bit Only**

#### 1. **No Market Demand**
```
Who needs 32-bit in 2025?
- Banks: Use COBOL/mainframes (separate ecosystem)
- Enterprises: Standardized on 64-bit a decade ago
- Startups: Never touch 32-bit
- Hobbyists: Raspberry Pi 4/5 are 64-bit default
```

#### 2. **AC is a Modern Language**
- Built in 2024-2025 with modern idioms
- Targets Python, JavaScript, Rust, Go, C++ backends
- These languages/runtimes dropped 32-bit support long ago
- No reason to support what the ecosystem abandoned

#### 3. **Massive Code Complexity for Zero Benefit**
```
32-bit support requires:
- Dual addressing schemes
- Different register conventions (x86 vs x86-64)
- Separate runtime helpers
- Testing on both platforms
- Binary size bloat

For who? Nobody uses AC on 32-bit systems.
```

#### 4. **Banks Can't Switch Anyway**
```
The banking problem:
- Billions of lines of COBOL
- 50+ year migration track record
- Switching costs: $$$$$billions
- Risk: Lost customers, regulators sue
- Reality: Will never leave COBOL

AC can't solve this problem.
```

So why support 32-bit just because banks are stuck?

#### 5. **All Dependencies Are 64-Bit**
```
AC's ecosystem:
- GCC/Clang: 64-bit preferred
- glibc: 64-bit everywhere
- Linux kernel: 64-bit default
- libacmath: 64-bit FFI only

Forcing 32-bit support breaks this chain.
```

---

## The Banking Exception

### Why Banks Are Stuck

```c
// COBOL from 1975, still running in 2025

IDENTIFICATION DIVISION.
PROGRAM-ID. PROCESS-CUSTOMER-ACCOUNTS.

DATA DIVISION.
FILE SECTION.
FD CUSTOMER-FILE.
01 CUSTOMER-RECORD.
   05 CUST-ID PIC 9(10).
   05 BALANCE PIC S9(13)V99.

PROCEDURE DIVISION.
   READ CUSTOMER-FILE INTO CUSTOMER-RECORD
   ...
```

**Why they're stuck:**
1. **100M lines of code** written in COBOL
2. **Millions of $ in tests** validating the system
3. **Regulatory compliance** audits every change
4. **Zero tolerance for bugs** (customer money at stake)
5. **Completely separate ecosystem** (mainframes, CICS, DB2)

**Why AC can't help:**
- AC doesn't run on mainframes
- AC doesn't replace COBOL semantics
- AC doesn't solve the 50-year legacy problem
- Banks would still need COBOL

**The migration reality:**
```
Option A: Stay on 32-bit COBOL (safe, known)
Option B: Rewrite billions of lines in new language (catastrophic risk)

Banks choose Option A every time.
```

---

## AC's 64-Bit Only Strategy

### What We Support

```
Platform Matrix (64-bit only):

✅ Linux x86-64 (Intel/AMD)
   ├─ ELF64 native binaries
   ├─ GCC/Clang toolchain
   └─ Full Linux ecosystem

✅ Linux ARM64 (Raspberry Pi 4/5, Apple Silicon via Rosetta)
   ├─ Via C + GCC/Clang
   └─ Full Linux ecosystem

✅ macOS x86-64 (Intel Macs)
   ├─ Mach-O native binaries
   └─ Full Apple ecosystem

✅ macOS ARM64 (Apple Silicon M1/M2/M3)
   ├─ Via C + Clang
   └─ Full Apple ecosystem

✅ Windows x86-64
   ├─ PE64 native binaries
   ├─ MinGW toolchain
   └─ Full Windows ecosystem

❌ NOT Supported:
   ├─ x86-32 (32-bit Intel/AMD) — dead
   ├─ ARM32 (32-bit ARM) — dead
   ├─ i386 (old Intel) — archaeological
   └─ MIPS, PPC, etc. — no demand
```

### Implementation

All AC binaries are **64-bit pointers, 64-bit arithmetic by default:**

```ac
// AC code compiles to 64-bit everywhere
x = 9223372036854775807  // 64-bit max int (8EB)
y = ptr x                // 64-bit pointer
z = x * y               // 64-bit arithmetic
```

**If someone needs 32-bit:**
```ac
AC->C
// They get C code
// Compile with: gcc -m32 program.c -o program
// (if their system has 32-bit libc)
```

But that's **their choice**, not AC's responsibility.

---

## Design Decision Summary

| Decision | Rationale |
|----------|-----------|
| **64-bit only** | Modern systems universal, 32-bit dead |
| **No 32-bit variant** | Zero demand, high complexity cost |
| **Modern ecosystem** | All dependencies assume 64-bit |
| **Bank exception** | They're COBOL-locked forever anyway |
| **Cross-platform strategy** | 64-bit on Linux, Windows, macOS, ARM |

---

## Precedent

### Languages That Are 64-Bit Only

| Language | Year | 32-bit Support |
|----------|------|---|
| **Go** | 2009+ | Yes (but rare, deprecated) |
| **Rust** | 2010+ | Yes (x86-i586/i686 community-maintained) |
| **Python 3.10+** | 2021+ | ❌ No (dropped in 3.10) |
| **JavaScript** | 2024+ | ❌ No (Node 20+ dropped it) |
| **Java 17+** | 2021+ | ❌ Effectively no (ARM32 dead) |
| **Kotlin** | 2011+ | ❌ No (JVM runs 64-bit) |
| **Swift** | 2014+ | ❌ No (Apple Silicon only) |

**AC would join the modern language cohort by being 64-bit only.**

---

## The Path Forward

### AC v0.5+ Design

```cpp
// In exp_bny.cpp, exp_arm.cpp, etc.

#define AC_BITS 64  // Always 64-bit

// No conditionals for 32-bit
// No dual register conventions
// No 32-bit helpers
// No separate code paths

// Result: Clean, fast, maintainable compiler
```

### If Someone Really Needs 32-Bit

**Use AC->C:**
```bash
ac program.ac C      # Generate C code
gcc -m32 program.c   # Compile to 32-bit (their choice, their toolchain)
```

**Or:** Use an older language (Python 2, C, Java 8, etc.)

AC is **not for 32-bit systems.** AC is for modern software.

---

## Conclusion

**AC should be 64-bit only** because:

✅ **64-bit is universal** (not just common, but default everywhere)
✅ **32-bit is genuinely dead** (except COBOL bunkers)
✅ **Complexity cost is high** (dual code paths, testing burden)
✅ **Benefit is zero** (nobody runs AC on 32-bit systems)
✅ **The ecosystem chose 64-bit** (Python, JS, Rust, Go, etc.)
✅ **Banks are unsolvable** (they'll never leave COBOL)

**Decision: AC is 64-bit only. No exceptions, no variants.**

This makes the compiler simpler, faster, and focused on what actually matters in 2025.
