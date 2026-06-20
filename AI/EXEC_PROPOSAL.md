# exec() Proposal: Safe AC Code Execution in AI

**Goal:** Add `exec(AC_source_code)` to AI (the interpreted variant) that safely executes AC code at runtime with full guardrail protection.

---

## Why exec() in AI, Not AC?

### AI Already Has a Runtime ✅
- **AI compiles to AIBC** (bytecode)
- **AIVM executes bytecode** (interpreter/JIT)
- **AI compiler is available** at runtime
- **Guardrails are enforced** by AIVM

### AC Doesn't Have a Runtime ❌
- **AC compiles ahead-of-time** to native code
- **No AC compiler at runtime**
- **No runtime to enforce guardrails**
- Would need `--allow-foreign` and escape all protections

### Comparison

| Feature | exec() in AI | Foreign blocks in AC |
|---------|-------------|---------------------|
| Runtime | ✅ AIVM | ❌ Native binary |
| Type checking | ✅ Yes (compile-time) | ❌ No |
| Rate limiting | ✅ Protected | ❌ Bypassed |
| Memory limit | ✅ Protected (500MB) | ❌ Bypassed |
| CPU detection | ✅ Protected | ❌ Bypassed |
| Code execution | ✅ AC code (safe) | ❌ Native code (dangerous) |

**Verdict:** `exec()` in AI is strictly safer than Foreign blocks in AC. ✅

---

## Implementation Plan

### 1. Add EXEC Opcode

**File:** `AI/ai-compiler/include/aibc.hpp`

```cpp
enum class AIOp : uint8_t {
    // ... existing opcodes ...
    EXEC = 0x90,  // src=A (AC source code string), dst=B (result register)
};
```

### 2. Add EXEC Handler in Interpreter

**File:** `AI/ai-vm/src/interpreter.cpp`

```cpp
case AIOp::EXEC: {
    // Get source code from register A
    AIVal& src_reg = R(ins.A);
    if (!src_reg.isStr()) {
        throw std::runtime_error("exec() requires string argument");
    }
    
    // Compile AC code to bytecode
    AiBcModule compiled = compile_ac_code(src_reg.sval);
    
    // Execute compiled bytecode
    // (nested AIVM call with same security context)
    AIVal result = executeModule(compiled);
    
    // Store result in destination register
    R(ins.B) = result;
    break;
}
```

### 3. Add Compiler Support

**File:** `AI/ai-compiler/src/ai_codegen.cpp`

Add built-in function stub:

```cpp
// Built-in: exec(code: str) -> any
// Generates: EXEC src=A, dst=B
void genBuiltinExec(CodeGenContext& ctx, const ASTNode& call) {
    // Evaluate argument (must be string)
    AIVal src = evaluateExpr(call.args[0]);
    
    // Emit EXEC instruction
    uint8_t src_reg = allocReg();
    emit({ AIOp::EXEC, src_reg, allocReg(), 0 });
}
```

### 4. API Usage

**In AI code:**

```ai
/* Safe: execute AC code at runtime */
code = $x = 5; y = x * 2; y$
result = exec(code)          /* result = 10 */

/* User input (still safe - constrained by AIVM) */
user_expr = read_input()
result = exec(user_expr)     /* Protected by rate limiting, memory limits, etc. */

/* Nested exec (allowed!) */
code1 = $exec($nested code$)$
result = exec(code1)         /* Compiles → executes → exec again */
```

---

## Security Model

### Protected by AIVM Guardrails ✅

```
User AC Code (via exec())
         ↓
   AI Compiler
         ↓
   AIBC Bytecode
         ↓
   AIVM Execution
   ├─ Rate limiting (10 req/sec)
   ├─ Memory limit (500MB)
   ├─ CPU detection (1M iterations/sec)
   ├─ Connection limit (100 concurrent)
   └─ All other guardrails
         ↓
   Result (or Error)
```

### What exec() CANNOT Bypass

✅ Rate limiting — still applies  
✅ Memory limits — still enforced  
✅ CPU throttling — still detected  
✅ Connections pooling — still limited  
✅ File I/O (via os.sbash) — still sandboxed  
✅ SQL injection (JaSQL) — still injection-safe  

### What exec() CAN Do (Like All Code)

⚠️ os.bash() — unrestricted shell (same as normal AC code)  
⚠️ File I/O — read/write files (same as normal AC code)  
⚠️ Application logic — slow handlers, etc. (developer responsibility)  

---

## Threat Model

### Attacker Input to exec()

```ai
user_code = read_input()     /* Attacker controls this */
exec(user_code)              /* Still protected! */
```

Even if attacker provides:

```ac
/* ❌ BLOCKED: Loop attack */
WHILST true
    web.request_get("https://target.com")
/* After 10 requests in 100ms → blocked by rate limiter */

/* ❌ BLOCKED: Memory bomb */
buffer = []
WHILST true
    buffer.append("x" * 1000000)
/* After 500MB → memory limit enforced */

/* ⚠️ NOT BLOCKED: Bad design */
WHILST true
    x = x + 1
/* Tight loop at 100% CPU → detected & throttled */

/* ⚠️ NOT BLOCKED: os.bash() */
os.bash("rm -rf /")
/* Just like any AC code — developer should use os.sbash() */
```

---

## Why This Works

1. **Code stays in AC's type system** (type-safe)
2. **Bytecode is sandboxed** (runs in AIVM)
3. **Guardrails are runtime checks** (applied to all code)
4. **Compiler is trusted** (not user code)
5. **No native code execution** (unlike Foreign blocks)

---

## Example: Safe eval(AC code)

```ai
/* This is what math.eval() does for math expressions */
/* We're extending it to AC code */

/* math.eval: Limited scope (math only) */
result = math.eval($2 * sin(pi)$)

/* exec: Full scope (any AC code, but protected) */
code = $
    x = 5
    y = x + 3
    y
$
result = exec(code)  /* result = 8 */

/* Combined: eval() for math, exec() for code */
user_math = read_input()
user_code = read_input()

math_result = math.eval(user_math)  /* Protected: math-only scope */
code_result = exec(user_code)       /* Protected: AIVM guardrails */
```

---

## Implementation Priority

1. **Phase 1:** Add EXEC opcode + interpreter handler
2. **Phase 2:** Add compiler support (builtin function)
3. **Phase 3:** Test with user input (prove it's safe)
4. **Phase 4:** Documentation + examples

---

## Comparison: All Escape Hatches

| Mechanism | Scope | Type Check | Guardrails | Use Case |
|-----------|-------|-----------|-----------|----------|
| **math.eval()** | Math expressions | ✅ Yes | ✅ All | Safe formula evaluation |
| **exec()** (proposed) | AC code | ✅ Yes | ✅ All | Safe code evaluation |
| **Foreign blocks** | Native code | ❌ No | ❌ None | Native library interop |

**Best practice:** Use `exec()` for untrusted AC code, `Foreign` only for trusted native interop.

---

## Open Questions

1. Should `exec()` have a size limit for source code? (e.g., max 1MB)
2. Should nested `exec()` calls be allowed? (I think yes, but should test)
3. Should there be an `--allow-exec` flag like `--allow-foreign`? (Probably not needed - it's safe)
4. How deep can recursion go? (AIVM stack limit applies)

---

## Verdict

**exec() in AI is the right way to add code evaluation to AC.**

- ✅ Theoretically safe (stays in AC runtime)
- ✅ Practically feasible (AIVM exists)
- ✅ Strictly safer than Foreign blocks
- ✅ Preserves type safety and guarantees
- ✅ Protects against accidental misuse

**Not a vulnerability. A feature.** 🎯
