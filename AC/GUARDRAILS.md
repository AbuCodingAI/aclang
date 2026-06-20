# AC Guardrails: Complete List

All built-in protections and safety mechanisms.

---

## 1. **Loop & Request Rate Limiting** 🔴

**What it does:** Detects and blocks tight loops spamming requests

```ac
WHILST true
    web.request_get("https://target.com")  /* Blocked after 10 requests in 100ms */
```

- Threshold: 10+ requests in < 100ms = loop pattern
- Block duration: 1 second
- Message: "Suspicious loop pattern detected"

---

## 2. **Ping Flood Detection** 🔴

**What it does:** Blocks repeated rapid pings to same target

```ac
WHILST true
    ping("target.com")  /* Blocked after 5 pings in 10 seconds */
```

- Threshold: 5+ pings in 10 seconds
- Block duration: 60 seconds
- Message: "Ping flood detected"

---

## 3. **Connection Pooling Limit** 🔴

**What it does:** Limits concurrent connections

```ac
FOR i in range(101)
    server = web.httpServer("0.0.0.0", 8000 + i)  /* 101st rejected */
```

- Limit: 100 concurrent connections per process
- Action: Reject new connections
- Message: "Connection limit exceeded"

---

## 4. **Memory Limit** 🔴

**What it does:** Caps total memory per process

```ac
buffer = []
WHILST true
    buffer.append("x" * 1000000)  /* After ~500MB allocated, rejected */
```

- Limit: 500MB per AC process
- Action: Reject allocation
- Message: "Memory limit exceeded"

---

## 5. **CPU Spinning Detection** 🔴

**What it does:** Detects tight loops running at 100% CPU

```ac
WHILST true
    x = x + 1  /* After 1M iterations in < 1 second, throttled */
```

- Threshold: 1,000,000 iterations in < 1 second
- Action: Throttle or kill process
- Message: "CPU throttling: Excessive spin loop detected"

---

## 6. **os.sbash() Sandboxing** 🟡

**What it does:** Blocks dangerous shell commands

```ac
os.sbash("sudo rm -rf /")     /* ❌ BLOCKED: sudo not allowed */
os.sbash("nohup malware &")   /* ❌ BLOCKED: nohup not allowed */
os.sbash("ls -la")            /* ✅ ALLOWED: safe command */
```

**Blocked patterns:**
- `sudo` — privilege escalation
- `su` — user switching
- `nohup` — detached processes
- `disown` — process orphaning
- `&` — background execution
- `tmux`, `screen` — terminal multiplexing
- `function` — function definitions (bash-specific)

---

## 7. **Path Traversal Blocking** 🟡

**What it does:** Prevents `../` attacks in web handlers

```ac
server.get($/files/:name$, fn(req)
    /* SAFE: Path traversal blocked */
    file.read(req.params[$name$])  /* ../../etc/passwd rejected */
)

/* NOT SAFE: Direct os operations */
data = os.read($../../etc/passwd$)  /* Depends on OS permissions */
```

- Applies to: ilib/web path parameters
- Action: Reject request, return 403 Forbidden
- Does NOT apply to: Direct os.read/write calls

---

## 8. **SQL Injection Protection (JaSQL)** 🟢

**What it does:** Prevents SQL injection via safe parsing

```ac
/* SAFE: JaSQL parses as syntax, not strings */
query = $TAKE * FROM users IF id = $ + user_id + $$  /* Safe */

/* Even better: Use parameters */
result = web.jasql("TAKE * FROM users IF id = ?", [user_id])
```

- JaSQL is NOT string-based (can't inject)
- Uses parameterized queries
- No `eval()` function

---

## 9. **Foreign Blocks Gating** 🔴

**What it does:** Requires explicit opt-in for raw code

```ac
/* By default: REJECTED */
<Foreign>
    import os
    os.system("rm -rf /")
<Foreign>

/* With --allow-foreign: ALLOWED */
/* ac program.ac --allow-foreign */
```

- Default: Foreign blocks disabled
- Activation: `--allow-foreign` compile flag
- Message: "Foreign blocks are disabled. Recompile with --allow-foreign"
- Audit: `grep -r "<Foreign>"` finds all uses

---

## 10. **Safe eval() - Math Expressions Only** 🟢

**What it does:** AC has `math.eval()` for evaluating mathematical expressions only

```ac
/* SAFE: math.eval() only evaluates math expressions */
result = math.eval($2 * x + 3$)       /* Safe - math only */
result = math.eval($sin(pi/2)$)       /* Safe - no code execution */

/* CANNOT: Eval arbitrary code */
result = math.eval($os.bash("rm -rf /")$)  /* ❌ Syntax error - not math */
```

- **math.eval()** parses mathematical expressions only
- Cannot eval arbitrary AC code (no Foreign-like escape)
- Cannot exec shell commands
- Scope: +, -, *, /, ^, sin, cos, tan, sqrt, pi, e, tau, etc.
- **Threat**: If untrusted user input in expression, could be solved with parameterized expressions (future enhancement)

---

## 11. **Type Safety** 🟢

**What it does:** Compile-time type checking prevents type confusion

```ac
x = 5
y = $hello$  /* string */
z = x + y    /* ❌ Type error at compile time */
```

- Type checker enforces type correctness
- Catches type confusion bugs
- Prevents unsafe casts

---

## 12. **Safe String Handling** 🟢

**What it does:** String syntax (`$...$`) prevents injection

```ac
/* Safe: String literals, can't be confused with code */
msg = $SELECT * FROM users$  /* Just a string */

/* vs. unsafe languages */
query = "SELECT * FROM users"  /* Could be concatenated unsafely */
```

- String delimiter (`$...$`) is unambiguous
- Can't accidentally exec code through strings
- No string interpolation without explicit concat

---

## 13. **Operator Safety** 🟢

**What it does:** Prevents undefined behavior from operators

```ac
/* SAFE: ptm/ptd are well-defined */
x = 5 ptm 3       /* 5 * 2^3 = 40 (well-defined) */
y = 20 ptd 2      /* 20 / 2^2 = 5 (well-defined) */

/* vs. C (undefined behavior possible) */
x = 5 << 35       /* UB in C (shift larger than type width) */
```

- Shift operators (ptm/ptd) clamp to valid ranges
- No undefined behavior from operators
- Bounds checked at runtime if needed

---

## 14. **Pointer Bounds** 🟡

**What it does:** Pointers are tracked, not raw memory

```ac
/* Safe: Pointers are opaque IDs, not raw addresses */
ptr = math.LongInt 123456789  /* Stores data, returns ID */
value = ptr.high              /* Accesses component */

/* vs. C */
void* ptr = malloc(10);
ptr[1000] = 5;  /* Buffer overflow, UB */
```

- Pointers are references (IDs), not addresses
- Can't do pointer arithmetic
- Can't overflow buffers
- Limited to accessing defined components

---

## 15. **No Direct Memory Access** 🟢

**What it does:** No raw pointers to arbitrary memory

```ac
/* ❌ NOT POSSIBLE in AC */
ptr = (void*)0x1000;      /* Can't create arbitrary pointers */
*ptr = 0;                 /* Can't dereference raw pointers */
memcpy(...);              /* No low-level memory ops */

/* ✅ You get managed pointers instead */
ptr = math.LongInt 123    /* Managed reference */
```

- No `malloc()` / `free()` directly
- No raw pointer dereferencing
- No `memcpy()` or `memmove()`

---

## 16. **Integer Overflow Protection** 🟡

**What it does:** Catches or wraps integer overflow

```ac
x = 9223372036854775807   /* int64 max */
y = x + 1                 /* Either: error OR wraps */
```

- Behavior: Depends on backend
- Some backends detect and error
- Some backends wrap (defined behavior, not UB)

---

## 17. **No Undefined Behavior** 🟢

**What it does:** AC semantics have well-defined behavior

```ac
/* All of these are DEFINED in AC (may error, but not UB) */
x = 1 / 0                 /* Error (not UB) */
y = math.LongInt(-1)
z = y.high                /* Defined (two's complement) */

/* vs. C */
int x = 1 / 0;            /* UB: Could do anything */
```

- Division by zero: Error
- Overflow: Wrapped or error
- Uninitialized variables: Compile-time detection

---

## 18. **No Buffer Overflow** 🟢

**What it does:** String/array bounds enforced

```ac
arr = [1, 2, 3]
x = arr[10]    /* ❌ Error: index out of bounds */

str = $hello$
c = str[100]   /* ❌ Error: index out of bounds */
```

- All array/string access is bounds-checked
- Out-of-bounds = error, not UB
- No buffer overflow possible

---

## 19. **No Use-After-Free** 🟢

**What it does:** Automatic memory management prevents UAF

```ac
/* AC handles this, no manual free */
buffer = []
buffer.append("data")  /* AC manages memory */
/* buffer automatically freed when out of scope or destroyed */

/* vs. C */
char* buf = malloc(10);
free(buf);
printf("%s", buf);  /* Use-after-free! UB! */
```

- Automatic memory management
- No manual allocation/deallocation
- Garbage collection or RAII handles cleanup

---

## 20. **No Double-Free** 🟢

**What it does:** Can't free same memory twice

```ac
/* Not possible in AC */
x = math.LongInt 123
x.free()
x.free()  /* Can't happen - automatic cleanup */
```

- Automatic cleanup prevents double-free
- Explicit `destroy` keyword is safe (idempotent)

---

## 21. **Race Condition Detection** 🟡

**What it does:** Goroutines are safe by default

```ac
/* Safe: Channels enforce synchronization */
ch = channel.new()
spawn fn()
    channel.send(ch, 42)
fn

x = channel.recv(ch)  /* Blocks until data available, no race */
```

- Channels are synchronization primitive
- Prevents data races (in goroutine model)
- No shared mutable state without synchronization

---

## 22. **No Null Pointer Dereference** 🟢

**What it does:** Null is explicit, checked

```ac
/* Safe: Must check for null */
ptr = math.LongInt.null()
IF ptr is math.LongInt.null()
    Term.display "Pointer is null"
ELSE
    x = ptr.high
```

- Null is explicit type
- Must check before use
- No implicit null dereference

---

## 23. **Safe Concurrency Primitives** 🟡

**What it does:** Goroutines use channels (not shared memory)

```ac
/* Safe: Message passing, not shared memory */
ch = channel.new()
spawn worker(ch)

/* Worker receives message safely */
```

- Based on message passing, not locks
- No manual mutex/semaphore bugs
- Goroutine model enforces safe concurrency

---

## 24. **No Format String Vulnerabilities** 🟢

**What it does:** No printf-style format strings

```ac
/* ❌ NOT POSSIBLE in AC */
msg = user_input  /* e.g., "%x %x %x" */
Term.display msg  /* Just prints the string, no format interpretation */

/* vs. C */
printf(user_input);  /* Format string attack! */
```

- No format string interpretation
- `Term.display` just prints the string
- String concatenation is explicit

---

## 25. **CORS Protection** 🟢

**What it does:** Automatic CORS headers for security

```ac
server = web.httpsServer("0.0.0.0", 8443)
server.cors_allow("https://trusted.example.com")  /* Explicit origin */
```

- Automatic X-Frame-Options: DENY
- Automatic X-Content-Type-Options: nosniff
- Automatic CSP headers
- HSTS for HTTPS

---

## 26. **No Timing Attacks** 🟡

**What it does:** (Partial) Protection against timing analysis

```ac
/* Should use constant-time comparison for secrets */
IF hash(password) is stored_hash
    authenticate()
```

- Note: Not built-in, developer responsibility
- But string comparison in AC is O(n) deterministic
- (Full timing attack prevention requires crypto library)

---

## Summary Table

| Guardrail | Type | Severity | Automatic? |
|-----------|------|----------|-----------|
| Loop rate limiting | Runtime | Critical | ✅ Yes |
| Ping flood detection | Runtime | Critical | ✅ Yes |
| Connection limits | Runtime | Critical | ✅ Yes |
| Memory limit | Runtime | Critical | ✅ Yes |
| CPU spinning detection | Runtime | Critical | ✅ Yes |
| os.sbash() sandboxing | Runtime | High | ✅ Yes |
| Path traversal blocking | Library | High | ✅ Yes (in web lib) |
| SQL injection protection | Language | High | ✅ Yes (JaSQL) |
| Foreign blocks gating | Language | Critical | ✅ Yes (--allow-foreign) |
| Safe math.eval() | Language | High | ✅ Yes (math expressions only) |
| Type safety | Compile | High | ✅ Yes |
| Safe strings | Language | Medium | ✅ Yes |
| Operator safety | Language | Medium | ✅ Yes |
| Pointer bounds | Language | High | ✅ Yes |
| No raw memory access | Language | Critical | ✅ Yes |
| Integer overflow handling | Runtime/Compile | Medium | ✅ Yes |
| No undefined behavior | Language | Critical | ✅ Yes |
| Buffer overflow prevention | Runtime | Critical | ✅ Yes |
| No use-after-free | Language | Critical | ✅ Yes |
| No double-free | Language | Critical | ✅ Yes |
| Race condition detection | Language | High | ⚠️ Partial |
| No null dereference | Language | High | ⚠️ Check required |
| Safe concurrency | Language | High | ✅ Yes |
| No format strings | Language | Critical | ✅ Yes |
| CORS protection | Library | High | ✅ Yes |

---

## The Bottom Line

**AC has 26 major guardrails**, most automatic. They protect against:
- ✅ Accidental bugs (buffer overflows, UB, use-after-free, etc.)
- ✅ Naive attacks (infinite loops, DDoS attempts, tight loops)
- ✅ Common vulnerabilities (SQL injection, format strings, etc.)

They do NOT protect against:
- ❌ Intentional malicious code (any language has this problem)
- ❌ Bad application logic (slow handlers, data leaks)
- ❌ Trusting untrusted Foreign blocks

**AC is safer than C by default. It's not safer than good developers writing any language.** 🔒
