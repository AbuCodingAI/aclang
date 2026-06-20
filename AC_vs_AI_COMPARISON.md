# AC vs AI: Complete Comparison

**AC and AI are complementary languages, not rivals.** They serve fundamentally different purposes and should be in separate repositories.

---

## Quick Summary

| Feature | AC | AI |
|---------|----|----|
| **Purpose** | High-performance compiled multi-target language | Interpreted language for interactive/dynamic code |
| **Execution model** | Ahead-of-time (AOT) compilation | Bytecode interpretation (AIVM) |
| **Output** | Native binaries (BNY, C, PY, JS, GO, RS, etc.) | .aibc bytecode files |
| **Performance** | ⚡ Native speed (x86), C-equivalent (ARM) | 🐢 Interpreted, with optional JIT |
| **Use case** | **Production servers, performance-critical apps** | **Scripts, education, interactive tools** |
| **Type system** | Static (int64, str, list, dec) | Static (same + dec as first-class) |
| **Memory model** | Managed (no manual allocation) | Managed (AIVM handles it) |
| **Exact decimals** | Via GoodDec type | Via `dec` type (first-class citizen) |
| **eval/exec** | Foreign blocks (no guardrails) | exec() possible (with guardrails) |
| **Startup time** | Instant (native binary) | Slow (load bytecode + JIT) |
| **Distribution** | Single binary | Bytecode + AIVM |
| **Security** | Built-in guardrails for clients | Built-in guardrails for all code |

---

## Detailed Comparison

### 1. **Execution Model**

**AC: Ahead-of-Time (AOT) Compilation**
```
Source Code
    ↓
AC Compiler
    ↓
Native Binary (or C/PY/JS/etc. source)
    ↓
Execute directly (no compilation overhead)
```

**AI: Bytecode + Virtual Machine**
```
Source Code
    ↓
AI Compiler
    ↓
.aibc Bytecode (platform-independent)
    ↓
AIVM (interpreter or JIT)
    ↓
Execute (with runtime startup cost)
```

**Winner:** AC wins for performance. AI wins for portability.

---

### 2. **Language Syntax**

**Both use the same syntax:**
```ac
/* AC code */
AC->PY
x = 5
y = x + 3
Term.display y

/* AI code (same syntax, different target) */
AI->VM
x = 5
y = x + 3
Term.display y
```

**Key difference:** Target declaration
- AC: `AC->PY`, `AC->C`, `AC->BNY`, etc. (11 backends)
- AI: `AI->VM` (only AIVM)

---

### 3. **Type System**

**Both have:**
- `int` (64-bit signed)
- `str` (immutable strings)
- `list` (heterogeneous arrays)
- `dec` (exact decimals)

**Difference:**

**AC's `dec` type:**
- Via `GoodDec` in ilib/math
- Not first-class (requires explicit library)
- Used with: `math.GoodDec(19.99)`

**AI's `dec` type:**
- First-class primitive type
- Built into the language
- Used with: `dec 19.99`

**Winner:** Tie. AC wins for flexibility (choose precision level). AI wins for convenience (always exact).

---

### 4. **Decimal Arithmetic**

**Important note from user:** ⚠️ **AI is NOT for finances**

**Why:**

Finance needs **fixed-precision decimals** (exactly 2 places for cents):
```
$19.99 USD
$1.09 EUR  
$0.01 cent
```

**AI provides arbitrary-precision decimals:**
```ai
price = dec 19.99        -- stored as {1:1, 0:9, -1:9, -2:9}
tax   = dec 1.08         -- stored as {1:1, 0:8}
total = price @ tax      -- 21.5892 (4 decimal places!)
```

**Problem:** Finance needs to **drop** fractional cents, not preserve them.

**AC does this better:**
```ac
/* Use fixed-size integer in cents */
price_cents = 1999       /* $19.99 */
tax_rate = 108           /* 1.08 = 108% */
total_cents = (price_cents * tax_rate) / 100
/* Result: 2158 cents = $21.58 (correct!) */
```

**Verdict:** For finance, **use AC with integer cents, not AI with dec**. 💰

---

### 5. **Performance**

**AC: Native Speed**
```
AC->BNY (Linux x86-64)  → Direct ASM → Native binary
AC->C                   → C source   → GCC → Binary
AC->PY                  → Python     → Interpreter
```

**Startup:** Instant (already compiled)  
**Runtime:** Near-C performance (or Python speed if targeting PY)  
**Memory:** Minimal (no VM overhead)

**AI: Interpreted with Optional JIT**
```
AI->VM → AIBC bytecode → AIVM interpreter
                      ↓ (hot functions)
                  JIT compile → native
```

**Startup:** 100ms+ (load bytecode, initialize VM)  
**Runtime:** 10-100x slower than AC (until JIT kicks in)  
**Memory:** VM overhead (~50MB for AIVM)

**Winner:** AC by far. Use AC for servers, AI for scripts.

---

### 6. **Compilation Targets**

**AC: 11 First-Class Backends**

| Backend | Output | Use |
|---------|--------|-----|
| BNY | Native x86-64 binary | **Production servers (Linux)** |
| C | C source | **Embedded systems, ARM** |
| PY | Python source | **Scientific computing, ML** |
| JS | JavaScript | **Browsers, Node.js** |
| CPP | C++ source | **Graphics, games** |
| RS | Rust source | **Safe systems** |
| GO | Go source | **Concurrent services** |
| Java | Java source | **Enterprise** |
| V | V source | **Low-level systems** |
| ASM | x86 assembly | **Debugging, optimization** |
| HTML | HTML/CSS/JS | **Web frontends** |

**AI: 1 Target**

| Target | Output | Use |
|--------|--------|-----|
| AIVM | .aibc bytecode | **Interactive scripts, education** |

**Winner:** AC (11 targets vs 1)

---

### 7. **When to Use AC**

✅ **Use AC for:**

1. **Production servers**
   ```ac
   AC->BNY
   server = web.httpsServer("0.0.0.0", 8443)
   server.get("/api", fn(req) ... )
   ```
   - Native performance
   - Single binary deployment
   - Built-in guardrails

2. **Performance-critical systems**
   - Real-time processing
   - High-frequency trading (not finance, but algorithmic)
   - Game engines

3. **Cross-platform deployment**
   ```ac
   /* Single source, 11 targets */
   AC->BNY          /* Linux server */
   /* Same code compiles to: */
   AC->PY           /* Data science */
   AC->JS           /* Browser frontend */
   AC->RS           /* Systems programming */
   ```

4. **Embedded systems** (via C backend)
   ```ac
   AC->C
   /* Compiles to C → ARM-compatible via GCC */
   ```

5. **Learning compiler design** (read the codebase)

---

### 8. **When to Use AI**

✅ **Use AI for:**

1. **Educational scripts**
   ```ai
   AI->VM
   Term.display "Hello, World"
   ```
   - Simple to run: `ai script.ai`
   - No compilation needed (well, to bytecode)
   - Easy to modify and retest

2. **Interactive tools**
   ```ai
   AI->VM
   name = Term.ask "Your name? "
   Term.display "Hello, " + name
   ```
   - Read-eval-print loop (REPL) possible
   - Bytecode can be distributed

3. **Exact decimal calculations** (non-financial)
   ```ai
   AI->VM
   /* Scientific computing where exact decimals matter */
   x = dec 0.1
   y = dec 0.2
   z = x + y  /* 0.3 exactly, no floating-point error */
   ```

4. **Code execution at runtime**
   ```ai
   AI->VM
   user_code = Term.ask "Enter AC code: "
   result = exec(user_code)  /* Protected by AIVM guardrails */
   ```
   - Dynamic code execution with safety
   - All guardrails still apply

5. **JIT compilation for hot paths**
   - Most code interpreted
   - Frequently called functions JIT-compiled
   - Hybrid performance

6. **Debugging and development**
   - Faster iteration (bytecode reloads faster than recompiling binary)
   - Interactive testing

---

### 9. **Decimal Arithmetic Examples**

**AC: Finance (Use Integers)**
```ac
AC->BNY

/* Store prices in cents */
price = 1999          /* $19.99 */
tax_rate = 108        /* 108% = 1.08 */

/* Calculate in cents, divide at the end */
total = (price * tax_rate) / 100
/* Result: 2158 cents = $21.58 ✅ */

Term.display (total / 100) + "." + (total % 100)
/* Prints: 21.58 */
```

**AI: Scientific (Use dec)**
```ai
AI->VM

/* Exact decimal arithmetic */
a = dec 0.1
b = dec 0.2
c = a + b
/* Result: 0.3 exactly ✅ */

Term.display c  /* Prints: 0.3 */

/* Division preserves precision */
x = dec 1
y = dec 3
z = x / y
Term.display z  /* Prints: 0.33333333333333333333 */
```

---

### 10. **Security**

**AC Security: Client-focused**
- Rate limiting (10 req/sec) — prevents client DDoS
- Ping flood detection — prevents ping attacks
- Memory limits (500MB) — prevents memory bombs
- CPU detection — prevents spinning loops
- **NOT protected:** developer-written malicious code

**AI Security: Code-focused**
- Same guardrails as AC **+ runtime protection**
- exec() code still constrained by rate limits, memory, CPU
- No escape hatch (no Foreign blocks at runtime)
- **Same threat:** developer-written malicious code

**Difference:** AC protects against malicious clients. AI protects against malicious code.

---

### 11. **File Organization**

**AC Monorepo (aclang)**
```
AC/
  ac-compiler/       /* The AC compiler */
  library/ilib/      /* AC standard libraries */
  tests/
  examples/
  SECURITY_FEATURES.md
  GUARDRAILS.md
  ARCHITECTURE.md
```

**AI Should Be Separate Repository**

```
ai-lang/
  ai-compiler/       /* The AI compiler */
  ai-vm/             /* The AIVM interpreter/JIT */
  library/ilib/      /* AI standard libraries (shared with AC) */
  tests/
  examples/
  AI.md
```

**Why separate?**
1. Different release cycles
2. Different focus (performance vs. education)
3. Different communities (game devs vs. students)
4. AI depends on AC compiler, not vice versa
5. Makes git history cleaner

---

### 12. **Compilation Comparison**

**AC Compilation Flow**
```
.ac source
    ↓
Lexer + Parser (shared with AI)
    ↓
AST
    ↓
AC IR (unified intermediate representation)
    ↓
Optimization passes
    ↓
11 different codegens (BNY, C, PY, JS, etc.)
    ↓
Output (binary, source, etc.)
```

**AI Compilation Flow**
```
.ai source
    ↓
Lexer + Parser (shared with AC)
    ↓
AST
    ↓
AI IR (simplified from AC IR)
    ↓
AIBC Bytecode emitter
    ↓
.aibc file
    ↓
AIVM (interpreter)
    ↓
Runtime execution (or JIT)
```

**Key insight:** AI reuses AC's lexer/parser, but has its own IR and codegen.

---

## Recommendation Summary

### **Use AC if you need:**
- 🚀 High performance (native binary)
- 📦 Single-binary deployment
- 🎯 Cross-platform support (11 backends)
- 🔒 Production-grade security
- ⚡ Sub-millisecond latency
- 💾 Minimal resource footprint

**Example:** Production web server
```ac
AC->BNY
server = web.httpsServer("0.0.0.0", 8443)
/* Single binary, native performance, built-in guardrails */
```

### **Use AI if you need:**
- 📚 Education / learning
- 🎮 Interactive tools
- 🔬 Exact decimal arithmetic (non-financial)
- ⚙️ Runtime code execution (with safety)
- 🔄 Faster iteration cycle
- 🌐 Platform independence (single bytecode)

**Example:** Educational script
```ai
AI->VM
x = dec 0.1
y = dec 0.2
z = x + y
Term.display z  /* 0.3 exactly */
```

### **Summary Matrix**

| Goal | Use AC | Use AI |
|------|--------|--------|
| **Production server** | ✅ AC | ❌ |
| **Web service** | ✅ AC | ❌ |
| **Teaching compiler design** | ✅ AC | (AC code) |
| **Student assignment** | ❌ | ✅ AI |
| **Exact decimal math** | GoodDec | ✅ dec |
| **Finance** | ✅ (use cents) | ❌ (wrong precision) |
| **Scientific computing** | ✅ AC->PY | ✅ AI->VM |
| **Games** | ✅ AC->C++ | ❌ |
| **Embedded systems** | ✅ AC->C | ❌ |
| **Interactive REPL** | ❌ | ✅ AI |
| **Maximum speed** | ✅ AC | ❌ |
| **Learning curves** | Steep | ✅ Gentle |

---

## Conclusion

**AC and AI are complementary, not competing.**

- **AC** is the production-grade, high-performance, multi-target compiler for serious applications
- **AI** is the interpreted, educational, exact-decimal tool for scripts and learning

**They should be in separate repositories** because they have different:
- Release cycles
- Target audiences
- Development priorities
- Feature sets

**They can share:**
- Lexer and parser
- AST representation
- Standard library concepts (ilib)
- Type system basics

**Bottom line:** AC is a superpower for production. AI is a teaching tool with exact decimals. Use each for what it's designed for. 🎯
